#include "pch.h"
#include "SigDump.h"
#include "util/Util.h"
#include "util/Logger.h"
#include "util/signature.h"

#include <Zydis/Zydis.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

std::atomic<int> SigDump::armedCount { 0 };

namespace {
    constexpr size_t SNAPSHOT_BYTES = 192; // bytes copied from each function start
    constexpr size_t MAX_PATTERN_BYTES = 128;
    constexpr size_t MIN_PATTERN_BYTES = 10;
    constexpr int MAX_MAIN_FRAMES = 12;

    // ---------------------------------------------------------------- module info
    struct ModuleInfo {
        uintptr_t base = 0;
        size_t size = 0;
        std::vector<std::pair<const uint8_t*, size_t>> exec;
    };

    const ModuleInfo& mainModule() {
        static const ModuleInfo info = [] {
            ModuleInfo m;
            auto* dos = reinterpret_cast<PIMAGE_DOS_HEADER>(GetModuleHandleA(nullptr));
            m.base = reinterpret_cast<uintptr_t>(dos);
            auto* nt = reinterpret_cast<PIMAGE_NT_HEADERS>(m.base + dos->e_lfanew);
            m.size = nt->OptionalHeader.SizeOfImage;
            auto* sec = IMAGE_FIRST_SECTION(nt);
            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
                if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                    m.exec.emplace_back(reinterpret_cast<const uint8_t*>(m.base + sec[i].VirtualAddress),
                                        (size_t)sec[i].Misc.VirtualSize);
                }
            }
            return m;
        }();
        return info;
    }

    bool inMain(uintptr_t a) {
        auto& m = mainModule();
        return a >= m.base && a < m.base + m.size;
    }

    std::string hexStr(uintptr_t v) {
        return std::format("0x{:X}", v);
    }

    std::string rvaStr(uintptr_t a) {
        return std::format("Minecraft.Windows.exe+0x{:X}", a - mainModule().base);
    }

    // ---------------------------------------------------------------- SEH-guarded memory helpers (POD only)
    bool safeCopy(void* dst, const void* src, size_t n) {
        __try {
            std::memcpy(dst, src, n);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // counts matches (stops once count > cap). Returns false if the memory could not be read.
    bool scanRaw(const uint8_t* b, size_t n, const uint8_t* pat, const uint8_t* mask, size_t len, size_t anchor,
                 size_t cap, size_t* count) {
        if (n < len) return true;
        __try {
            const uint8_t* x = b + anchor;
            const uint8_t* xmax = b + n - (len - anchor);
            while (x <= xmax) {
                x = static_cast<const uint8_t*>(std::memchr(x, pat[anchor], (size_t)(xmax - x) + 1));
                if (!x) break;
                const uint8_t* s = x - anchor;
                bool ok = true;
                for (size_t i = 0; i < len; i++) {
                    if (mask[i] && s[i] != pat[i]) {
                        ok = false;
                        break;
                    }
                }
                if (ok && ++*count > cap) return true;
                ++x;
            }
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // ---------------------------------------------------------------- unwind-table function lookup
    struct FuncInfo {
        uintptr_t start = 0;
        uint32_t size = 0;
    };

    FuncInfo funcInfoFor(uintptr_t pc) {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(pc, &imageBase, nullptr);
        if (!rf) return {};
        // follow chained unwind info back to the primary function entry
        for (int i = 0; i < 8; i++) {
            if (rf->UnwindData & 1) break;
            auto* ui = reinterpret_cast<const uint8_t*>(imageBase + rf->UnwindData);
            if (!((ui[0] >> 3) & UNW_FLAG_CHAININFO)) break;
            const uint8_t codes = ui[2];
            rf = reinterpret_cast<PRUNTIME_FUNCTION>(const_cast<uint8_t*>(ui) + 4 + ((codes + 1) & ~1) * 2);
        }
        return { static_cast<uintptr_t>(imageBase + rf->BeginAddress), rf->EndAddress - rf->BeginAddress };
    }

    // follows an incremental-linking `jmp rel32` thunk
    uintptr_t followThunk(uintptr_t p) {
        uint8_t b[5];
        if (safeCopy(b, reinterpret_cast<void*>(p), 5) && b[0] == 0xE9) {
            int32_t rel;
            std::memcpy(&rel, b + 1, 4);
            return p + 5 + rel;
        }
        return p;
    }

    // ---------------------------------------------------------------- hooks
    struct HookInfo {
        uintptr_t func = 0; // function that got hooked
        uintptr_t detour = 0;
        std::string name;
        bool tableSwap = false;
        std::array<uint8_t, SNAPSHOT_BYTES> orig {};
        bool origValid = false;
    };

    std::mutex hooksMtx;
    std::vector<HookInfo>& hooks() {
        static std::vector<HookInfo> h;
        return h;
    }

    const HookInfo* findHookByFunc(uintptr_t func) {
        for (auto& h : hooks())
            if (h.func == func) return &h;
        return nullptr;
    }

    // ---------------------------------------------------------------- pattern building (Zydis)
    struct Pat {
        std::vector<uint8_t> b;
        std::vector<uint8_t> m; // 1 = fixed, 0 = wildcard
        std::vector<size_t> bounds; // byte length after each whole instruction
    };

    // Wildcards everything that changes between game versions: 32-bit displacements (RIP-relative globals and
    // struct field offsets) and relative branch/call targets. Opcodes, registers and immediates stay fixed.
    Pat buildPattern(const uint8_t* code, size_t n, size_t maxBytes) {
        ZydisDecoder dec;
        ZydisDecoderInit(&dec, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

        Pat p;
        size_t off = 0;
        while (off < n && p.b.size() < maxBytes) {
            ZydisDecodedInstruction ins;
            ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT];
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&dec, code + off, n - off, &ins, ops))) break;
            if (ins.mnemonic == ZYDIS_MNEMONIC_INT3) break; // function padding

            const size_t base = p.b.size();
            for (size_t i = 0; i < ins.length; i++) {
                p.b.push_back(code[off + i]);
                p.m.push_back(1);
            }
            auto wild = [&](size_t at, size_t count) {
                for (size_t i = 0; i < count && base + at + i < p.m.size(); i++)
                    p.m[base + at + i] = 0;
            };
            if (ins.raw.disp.size == 32) wild(ins.raw.disp.offset, 4);
            for (int i = 0; i < 2; i++) {
                if (ins.raw.imm[i].is_relative) wild(ins.raw.imm[i].offset, ins.raw.imm[i].size / 8);
            }
            off += ins.length;
            p.bounds.push_back(p.b.size());
        }
        return p;
    }

    std::string patToString(const Pat& p, size_t len) {
        while (len > 0 && !p.m[len - 1])
            len--; // trailing wildcards add nothing
        std::string s;
        for (size_t i = 0; i < len; i++) {
            if (i) s += ' ';
            s += p.m[i] ? std::format("{:02X}", p.b[i]) : std::string("?");
        }
        return s;
    }

    // number of matches of the first `len` bytes of the pattern in the executable sections (capped at cap + 1)
    // returns SIZE_MAX if memory could not be read
    size_t countMatches(const Pat& p, size_t len, size_t cap) {
        static const std::array<uint8_t, 14> common { 0x00, 0x48, 0x89, 0x8B, 0xFF, 0xCC, 0x0F,
                                                      0xE8, 0x24, 0x83, 0xC0, 0x4C, 0x8D, 0x41 };
        size_t anchor = len;
        for (size_t i = 0; i < len; i++) {
            if (!p.m[i]) continue;
            if (anchor == len) anchor = i; // first fixed byte as fallback
            if (std::find(common.begin(), common.end(), p.b[i]) == common.end()) {
                anchor = i;
                break;
            }
        }
        if (anchor == len) return SIZE_MAX;

        size_t count = 0;
        for (auto const& [b, n] : mainModule().exec) {
            if (!scanRaw(b, n, p.b.data(), p.m.data(), len, anchor, cap, &count)) return SIZE_MAX;
            if (count > cap) break;
        }
        return count;
    }

    struct SigResult {
        std::string pattern;
        size_t length = 0;
        size_t matches = 0; // total matches found in the module (incl. the function itself unless patched)
        bool unique = false;
        bool ok = false;
    };

    // shortest instruction-aligned prefix that is unique in the module. `expected` = how many matches the
    // function itself accounts for (1, or 0 if its bytes in memory are currently patched by a hook).
    SigResult makeSig(const uint8_t* bytes, size_t n, size_t expected) {
        SigResult r;
        Pat p = buildPattern(bytes, n, MAX_PATTERN_BYTES);
        if (p.bounds.empty()) return r;

        // candidate lengths (instruction boundaries) >= MIN_PATTERN_BYTES
        std::vector<size_t> cands;
        for (size_t b : p.bounds)
            if (b >= MIN_PATTERN_BYTES) cands.push_back(b);
        if (cands.empty()) cands.push_back(p.bounds.back());

        size_t full = countMatches(p, cands.back(), expected);
        r.ok = true;
        if (full == SIZE_MAX) {
            r.ok = false;
            return r;
        }
        if (full > expected) {
            r.pattern = patToString(p, cands.back());
            r.length = cands.back();
            r.matches = full;
            r.unique = false;
            return r;
        }

        size_t lo = 0, hi = cands.size() - 1; // hi is known to be unique
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            size_t c = countMatches(p, cands[mid], expected);
            if (c != SIZE_MAX && c <= expected)
                hi = mid;
            else
                lo = mid + 1;
        }
        r.pattern = patToString(p, cands[hi]);
        r.length = cands[hi];
        r.matches = expected;
        r.unique = true;
        return r;
    }

    // ---------------------------------------------------------------- jobs
    struct FuncJob {
        std::string label;
        uintptr_t addr = 0; // address the signature was taken at
        uint32_t funcSize = 0;
        std::array<uint8_t, SNAPSHOT_BYTES> bytes {};
        size_t nbytes = 0;
        bool patched = false; // bytes come from a hook's saved original and the live memory is overwritten
        std::string note;
    };

    struct Job {
        std::string title;
        std::string tag;
        std::string header; // free text printed before the functions
        std::vector<FuncJob> funcs;
    };

    std::atomic<int> pending { 0 };

    std::string sanitize(std::string s) {
        for (auto& c : s)
            if (!isalnum((unsigned char)c)) c = '_';
        return s;
    }

    std::string nowStamp() {
        std::time_t t = std::time(nullptr);
        std::tm tm {};
        localtime_s(&tm, &t);
        char buf[32];
        std::strftime(buf, sizeof buf, "%Y%m%d_%H%M%S", &tm);
        return buf;
    }

    std::filesystem::path outDir() {
        auto d = util::GetLatitePath() / "SigDump";
        std::error_code ec;
        std::filesystem::create_directories(d, ec);
        return d;
    }

    // snapshot function bytes; if a Latite hook owns this function, use its saved original bytes
    FuncJob snapshot(std::string label, uintptr_t addr, uint32_t size) {
        FuncJob f;
        f.label = std::move(label);
        f.addr = addr;
        f.funcSize = size;
        {
            std::lock_guard lk { hooksMtx };
            if (auto* h = findHookByFunc(addr); h && h->origValid) {
                f.bytes = h->orig;
                f.nbytes = SNAPSHOT_BYTES;
                uint8_t live[16] {};
                f.patched = !h->tableSwap && safeCopy(live, reinterpret_cast<void*>(addr), sizeof live) &&
                            std::memcmp(live, h->orig.data(), sizeof live) != 0;
                f.note = "hooked by Latite (" + h->name + "): signature built from the original, pre-patch bytes";
                return f;
            }
        }
        // read as many bytes as are readable
        size_t n = SNAPSHOT_BYTES;
        while (n >= 16 && !safeCopy(f.bytes.data(), reinterpret_cast<void*>(addr), n))
            n -= 16;
        f.nbytes = n >= 16 ? n : 0;
        return f;
    }

    void writeFunc(std::ostream& o, FuncJob& f, size_t index) {
        o << f.label << "\n";
        o << "   address : " << rvaStr(f.addr);
        if (f.funcSize) o << "   (function size ~0x" << std::format("{:X}", f.funcSize) << ")";
        o << "\n";
        if (!f.note.empty()) o << "   note    : " << f.note << "\n";
        if (f.nbytes == 0) {
            o << "   sig     : <could not read memory>\n\n";
            return;
        }
        auto r = makeSig(f.bytes.data(), f.nbytes, f.patched ? 0 : 1);
        if (!r.ok || r.pattern.empty()) {
            o << "   sig     : <could not build a signature (decode/scan failed)>\n\n";
            return;
        }
        if (r.unique)
            o << "   sig     : [UNIQUE, " << r.length << " bytes]\n";
        else
            o << "   sig     : [NOT UNIQUE: " << (r.matches > 1 ? ">" : "") << r.matches
              << " matches even at " << r.length << " bytes - use a caller/callee frame or a manual sig]\n";
        o << "             " << r.pattern << "\n";
        std::string nm = sanitize("Sig_" + std::to_string(index));
        o << "   snippet : inline static SigImpl " << nm << " { [](memory::signature_store&, uintptr_t res) { return res; },\n"
          << "                 \"" << r.pattern << "\"_sig, \"" << nm << "\" };\n\n";
    }

    std::string runJob(Job job) {
        auto path = outDir() / (job.tag + "_" + nowStamp() + ".txt");
        std::ofstream o(path);
        if (!o) {
            Logger::Warn("[SigDump] cannot write {}", path.string());
            return "cannot write " + path.string();
        }
        auto& m = mainModule();
        o << "=== SigDump: " << job.title << " ===\n";
        o << "Module  : Minecraft.Windows.exe  base=" << hexStr(m.base) << "  size=" << hexStr(m.size) << "\n";
        o << "Policy  : 32-bit displacements and relative branch/call targets are wildcarded ('?').\n";
        o << "          Signatures are shortened to the smallest instruction-aligned prefix that is unique.\n\n";
        if (!job.header.empty()) o << job.header << "\n";
        for (size_t i = 0; i < job.funcs.size(); i++) {
            o.flush();
            writeFunc(o, job.funcs[i], i);
        }
        o.flush();
        Logger::Info("[SigDump] wrote {}", path.string());
        return path.string();
    }

    void spawn(Job job) {
        pending++;
        std::thread([job = std::move(job)]() mutable {
            try {
                runJob(std::move(job));
            } catch (...) {
                Logger::Warn("[SigDump] worker failed");
            }
            pending--;
        }).detach();
    }

    // ---------------------------------------------------------------- arming
    std::mutex armMtx;
    std::string armFilter; // lowercase
    int armRemaining = 0;

    std::string lower(std::string s) {
        for (auto& c : s)
            c = (char)tolower((unsigned char)c);
        return s;
    }

    std::string parseEventName(std::string_view funcSig) {
        auto b = funcSig.find("dispatch<");
        if (b == std::string_view::npos) return std::string(funcSig);
        b += 9;
        auto e = funcSig.find('>', b);
        std::string s(funcSig.substr(b, e == std::string_view::npos ? std::string_view::npos : e - b));
        for (std::string_view pre : { "class ", "struct " })
            if (s.starts_with(pre)) s.erase(0, pre.size());
        while (!s.empty() && (s.back() == ' ' || s.back() == '&' || s.back() == '*'))
            s.pop_back();
        return s;
    }

    void captureAndSpawn(std::string const& eventName) {
        void* frames[64] {};
        const USHORT n = CaptureStackBackTrace(1, 64, frames, nullptr);

        Job job;
        job.title = "event " + eventName;
        job.tag = sanitize(eventName);

        std::string hdr;
        int mainFrames = 0;
        uintptr_t lastFunc = 0;
        bool hookReported = false;
        std::vector<FuncJob> funcs;

        for (USHORT i = 0; i < n; i++) {
            const uintptr_t pc = reinterpret_cast<uintptr_t>(frames[i]);
            if (inMain(pc)) {
                if (mainFrames >= MAX_MAIN_FRAMES) break;
                FuncInfo fi = funcInfoFor(pc);
                if (!fi.start || fi.start == lastFunc) continue;
                lastFunc = fi.start;
                funcs.push_back(snapshot(std::format("[stack #{}] return address {}  ->  function start {}", mainFrames,
                                                     rvaStr(pc), rvaStr(fi.start)),
                                         fi.start, fi.size));
                mainFrames++;
            } else {
                // Latite-side frame: is it one of our hook detours?
                if (hookReported) continue;
                FuncInfo fi = funcInfoFor(pc);
                if (!fi.start) continue;
                std::string hookName;
                uintptr_t hookFunc = 0;
                bool hookTable = false;
                {
                    std::lock_guard lk { hooksMtx };
                    for (auto& h : hooks()) {
                        if (h.detour && (h.detour == fi.start || followThunk(h.detour) == fi.start)) {
                            hookName = h.name;
                            hookFunc = h.func;
                            hookTable = h.tableSwap;
                            break;
                        }
                    }
                }
                if (hookFunc) {
                    hookReported = true;
                    hdr += "Dispatched from Latite hook '" + hookName + "'\n";
                    hdr += "   hooked game function: " + rvaStr(hookFunc) + (hookTable ? "  (vtable-swap hook)" : "") + "\n";
                    hdr += "   (that function is NOT on the stack below - the stack starts at its caller)\n\n";
                    funcs.insert(funcs.begin(), snapshot("[hooked function that fires this event]", hookFunc, 0));
                }
            }
        }
        if (hdr.empty()) hdr = "No Latite hook detour found on the stack (event dispatched from Latite itself?).\n\n";
        hdr += "Stack, innermost game frame first:\n";
        job.header = std::move(hdr);
        job.funcs = std::move(funcs);
        spawn(std::move(job));
    }
} // namespace

// -------------------------------------------------------------------- public API
void SigDump::onEvent(std::string_view funcSig) {
    thread_local bool busy = false;
    if (busy) return;

    std::string name = parseEventName(funcSig);
    {
        std::lock_guard lk { armMtx };
        if (armRemaining <= 0) return;
        if (armFilter != "*" && lower(name).find(armFilter) == std::string::npos) return;
        if (--armRemaining <= 0) armedCount = 0;
    }
    busy = true;
    captureAndSpawn(name);
    busy = false;
}

void SigDump::registerHook(uintptr_t target, void* detour, std::string_view name, bool tableSwap) {
    HookInfo h;
    h.detour = reinterpret_cast<uintptr_t>(detour);
    h.name = std::string(name);
    h.tableSwap = tableSwap;
    h.func = target;
    if (tableSwap) {
        uintptr_t fn = 0;
        if (!safeCopy(&fn, reinterpret_cast<void*>(target), sizeof fn) || !fn) return;
        h.func = fn;
    }
    h.origValid = safeCopy(h.orig.data(), reinterpret_cast<void*>(h.func), SNAPSHOT_BYTES);
    std::lock_guard lk { hooksMtx };
    hooks().push_back(std::move(h));
}

std::string SigDump::arm(std::string_view eventFilter, int count) {
    std::lock_guard lk { armMtx };
    armFilter = lower(std::string(eventFilter));
    armRemaining = std::max(1, count);
    armedCount = 1;
    return std::format("armed: next {} dispatch(es) of events matching '{}'", armRemaining, armFilter);
}

std::string SigDump::disarm() {
    std::lock_guard lk { armMtx };
    armRemaining = 0;
    armedCount = 0;
    return "disarmed";
}

std::string SigDump::status() {
    std::lock_guard lk { armMtx };
    return std::format("{} | pending jobs: {} | output: {}",
                       armRemaining > 0 ? std::format("armed for '{}' ({} left)", armFilter, armRemaining) : "not armed",
                       pending.load(), outDir().string());
}

std::vector<std::string> SigDump::knownEvents() {
    return { "AfterEntityRenderEvent", "AfterMoveEvent", "AppSuspendedEvent", "AttackEvent", "AveragePingEvent",
             "BeforeMoveEvent", "BobMovementEvent", "CharEvent", "ChatEvent", "ChatMessageEvent",
             "CinematicCameraEvent", "ClickEvent", "ClientTextEvent", "DrawHUDModulesEvent", "FocusLostEvent",
             "FogColorEvent", "GammaEvent", "GetTimeEvent", "HideHandEvent", "KeyUpdateEvent",
             "LatiteClientMessageEvent", "LeaveGameEvent", "OutlineSelectionEvent", "OverlayColorEvent",
             "PacketReceiveEvent", "PerspectiveEvent", "PlayerHurtScreenCloseEvent", "RenderEntityEvent",
             "RenderGameEvent", "RenderGuiItemEvent", "RenderLayerEvent", "RenderLevelEvent", "RenderNameTagEvent",
             "RenderOverlayEvent", "RendererCleanupEvent", "SendPacketEvent", "SensitivityEvent", "TickEvent",
             "TurnDeltaEvent", "UpdateEvent", "UpdatePlayerCameraEvent", "WeatherEvent" };
}

std::string SigDump::dumpAll() {
    auto path = outDir() / ("signatures_" + nowStamp() + ".txt");
    std::ofstream o(path);
    if (!o) return "cannot write " + path.string();

    auto& reg = memory::signature_store::registry();
    auto& m = mainModule();
    std::vector<std::string> missing, found;
    for (auto* s : reg) {
        std::string name(s->name.empty() ? std::string_view("<unnamed>") : s->name);
        if (!s->scan_result) {
            missing.push_back("NOT FOUND  " + name);
            continue;
        }
        std::string resolved = inMain(s->result) ? rvaStr(s->result) : hexStr(s->result) + " (not inside module)";
        found.push_back(std::format("found      {:<56} match={}  resolved={}", name, rvaStr(s->scan_result), resolved));
    }
    std::sort(missing.begin(), missing.end());
    std::sort(found.begin(), found.end());
    o << "=== SigDump: all registered signatures ===\n";
    o << "Module base=" << hexStr(m.base) << "  total=" << reg.size() << "  found=" << found.size()
      << "  NOT FOUND=" << missing.size() << "\n";
    o << "match    = where the pattern matched (start of the signature)\n";
    o << "resolved = value after the signature's own resolve step (deref/offset), as RVA when inside the module\n\n";
    for (auto& l : missing)
        o << l << "\n";
    if (!missing.empty()) o << "\n";
    for (auto& l : found)
        o << l << "\n";
    o.flush();
    Logger::Info("[SigDump] {} signatures: {} found, {} NOT FOUND -> {}", reg.size(), found.size(), missing.size(),
                 path.string());
    return std::format("{} sigs: {} found, {} NOT FOUND -> {}", reg.size(), found.size(), missing.size(), path.string());
}

std::string SigDump::dumpHooks() {
    Job job;
    job.title = "all Latite hooks";
    job.tag = "hooks";
    struct HookCopy {
        std::string name;
        uintptr_t func;
        bool table;
    };
    std::vector<HookCopy> copies;
    {
        std::lock_guard lk { hooksMtx };
        for (auto& h : hooks())
            copies.push_back({ h.name, h.func, h.tableSwap });
    }
    for (auto& h : copies) {
        job.funcs.push_back(snapshot(std::format("[hook] {}{}", h.name.empty() ? "<unnamed>" : h.name,
                                                 h.table ? "  (vtable-swap)" : ""),
                                     h.func, 0));
    }
    const size_t n = job.funcs.size();
    spawn(std::move(job));
    return std::format("{} hooks queued; this scans the whole module per hook and can take a while - result goes to {}",
                       n, outDir().string());
}

std::string SigDump::dumpAt(uintptr_t addrOrRva) {
    auto& m = mainModule();
    uintptr_t addr = addrOrRva < m.size ? m.base + addrOrRva : addrOrRva;
    if (!inMain(addr)) return "address is outside Minecraft.Windows.exe";

    Job job;
    job.title = "address " + rvaStr(addr);
    job.tag = "at_" + std::format("{:X}", addr - m.base);
    job.funcs.push_back(snapshot("[exact address]", addr, 0));
    FuncInfo fi = funcInfoFor(addr);
    if (fi.start && fi.start != addr)
        job.funcs.push_back(snapshot("[start of the containing function]", fi.start, fi.size));
    spawn(std::move(job));
    return "queued; result goes to " + outDir().string();
}

void SigDump::waitIdle() {
    for (int i = 0; i < 600 && pending.load() > 0; i++)
        Sleep(100);
}
