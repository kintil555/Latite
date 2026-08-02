#include "pch.h"
#include "PackCommand.h"
#include "client/Latite.h"
#include <fstream>
#include <algorithm>

// Helper: temukan folder com.mojang
static std::filesystem::path findComMojangForPack() {
    wchar_t localApp[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
        std::filesystem::path pkgRoot = std::filesystem::path(localApp) / L"Packages";
        if (std::filesystem::exists(pkgRoot)) {
            for (auto const& e : std::filesystem::directory_iterator(pkgRoot)) {
                if (!e.is_directory()) continue;
                std::wstring n = e.path().filename().wstring();
                if (n.rfind(L"Microsoft.MinecraftUWP_", 0) == 0 ||
                    n.rfind(L"Microsoft.MinecraftWindowsBeta_", 0) == 0) {
                    auto c = e.path() / L"LocalState" / L"games" / L"com.mojang";
                    if (std::filesystem::exists(c)) return c;
                }
            }
        }
        wchar_t roaming[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"APPDATA", roaming, MAX_PATH) > 0) {
            for (auto sub : { L"MinecraftPE", L"Minecraft" }) {
                auto c = std::filesystem::path(roaming) / sub / L"games" / L"com.mojang";
                if (std::filesystem::exists(c)) return c;
            }
        }
    }
    // Levi Launcher fallback: simpan di levilauncher AppData
    wchar_t appData[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH) > 0) {
        auto c = std::filesystem::path(appData) / L"levilauncher" / L"com.mojang";
        if (std::filesystem::exists(c)) return c;
    }
    return {};
}

PackCommand::PackCommand()
    : Command("pack", L"Kelola resource pack in-game. Ketik .pack help untuk bantuan.",
              "pack [list|on|off|apply] [nama]", { "rp" }) {}

std::filesystem::path PackCommand::getPacksPath() const {
    auto base = findComMojangForPack();
    return base.empty() ? std::filesystem::path{} : base / L"resource_packs";
}

std::filesystem::path PackCommand::getOptionsPath() const {
    auto base = findComMojangForPack();
    return base.empty() ? std::filesystem::path{} : base / L"minecraftpe" / L"options.txt";
}

void PackCommand::loadActive() {
    // Bersihkan status active dulu
    for (auto& p : packs) p.active = false;

    auto opt = getOptionsPath();
    if (opt.empty() || !std::filesystem::exists(opt)) return;

    std::ifstream f(opt);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("global_resource_packs:", 0) != 0 &&
            line.rfind("texture_packs:", 0) != 0) continue;
        size_t pos = 0;
        while (true) {
            size_t bs = line.find('\\', pos);
            if (bs == std::string::npos) break;
            size_t ns = bs + 1;
            size_t ne = line.find_first_of("\"}", ns);
            if (ne == std::string::npos) break;
            std::string name = line.substr(ns, ne - ns);
            if (!name.empty()) {
                std::wstring wname = util::StrToWStr(name);
                for (auto& p : packs)
                    if (p.name == wname) { p.active = true; break; }
            }
            pos = ne + 1;
        }
    }
}

void PackCommand::scanPacks() {
    packs.clear();
    auto packsPath = getPacksPath();
    if (packsPath.empty() || !std::filesystem::exists(packsPath)) {
        dirty = false;
        return;
    }
    for (auto const& e : std::filesystem::directory_iterator(packsPath)) {
        if (!e.is_directory()) continue;
        if (!std::filesystem::exists(e.path() / L"manifest.json") &&
            !std::filesystem::exists(e.path() / L"pack_manifest.json")) continue;
        PackEntry pe;
        pe.name   = e.path().filename().wstring();
        pe.active = false;
        packs.push_back(std::move(pe));
    }
    std::ranges::sort(packs, {}, [](PackEntry const& e) { return e.name; });
    loadActive();
    dirty = false;
}

void PackCommand::saveAndApply() {
    auto opt = getOptionsPath();
    if (opt.empty()) { message(L"[Pack] Tidak bisa menemukan options.txt", true); return; }

    // Baca options.txt lama
    std::vector<std::string> lines;
    {
        std::ifstream f(opt);
        std::string l;
        while (std::getline(f, l)) lines.push_back(l);
    }

    // Buat value baru
    std::string val = "global_resource_packs:[";
    bool first = true;
    for (auto const& p : packs) {
        if (!p.active) continue;
        if (!first) val += ',';
        val += "\"resource_packs\\\\" + util::WStrToStr(p.name) + "\"";
        first = false;
    }
    val += ']';

    // Replace atau append
    bool found = false;
    for (auto& l : lines) {
        if (l.rfind("global_resource_packs:", 0) == 0) { l = val; found = true; break; }
    }
    if (!found) lines.push_back(val);

    std::ofstream f(opt, std::ios::trunc);
    for (auto const& l : lines) f << l << '\n';
}

bool PackCommand::execute(std::string const label, std::vector<std::string> args) {
    // .pack atau .pack list -> tampilkan semua pack
    if (args.empty() || args[0] == "list") {
        if (dirty) scanPacks();
        if (packs.empty()) {
            message(L"[Pack] Tidak ada resource pack ditemukan.");
            return true;
        }
        message(L"[Pack] Resource packs:");
        for (auto const& p : packs) {
            std::wstring status = p.active ? L"[ON]  " : L"[OFF] ";
            message(status + p.name);
        }
        message(L"Gunakan: .pack on <nama> | .pack off <nama> | .pack apply");
        return true;
    }

    // .pack help
    if (args[0] == "help") {
        message(L"[Pack] Commands:");
        message(L"  .pack list          - lihat semua pack");
        message(L"  .pack on <nama>     - aktifkan pack");
        message(L"  .pack off <nama>    - non-aktifkan pack");
        message(L"  .pack apply         - terapkan perubahan");
        message(L"  .pack reload        - scan ulang folder pack");
        return true;
    }

    // .pack reload
    if (args[0] == "reload") {
        dirty = true;
        scanPacks();
        message(std::format(L"[Pack] Ditemukan {} pack.", (int)packs.size()));
        return true;
    }

    // .pack on/off <nama>
    if (args[0] == "on" || args[0] == "off") {
        if (args.size() < 2) {
            message(L"[Pack] Usage: .pack on/off <nama>", true);
            return true;
        }
        if (dirty) scanPacks();

        // Gabungkan arg jadi nama (support spasi)
        std::string nameStr;
        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) nameStr += ' ';
            nameStr += args[i];
        }
        std::wstring wname = util::StrToWStr(nameStr);

        // Cari pack (case-insensitive partial match)
        PackEntry* found = nullptr;
        for (auto& p : packs) {
            std::wstring lower = p.name;
            std::wstring lowerSearch = wname;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::towlower);
            if (lower.find(lowerSearch) != std::wstring::npos) {
                found = &p;
                break;
            }
        }

        if (!found) {
            message(L"[Pack] Pack tidak ditemukan: " + wname, true);
            message(L"Ketik .pack list untuk melihat daftar pack.");
            return true;
        }

        bool enable = (args[0] == "on");
        found->active = enable;
        message((enable ? L"[Pack] Diaktifkan: " : L"[Pack] Dinonaktifkan: ") + found->name);
        message(L"Ketik .pack apply untuk menerapkan.");
        return true;
    }

    // .pack apply
    if (args[0] == "apply") {
        if (dirty) scanPacks();
        saveAndApply();

        int count = 0;
        for (auto const& p : packs) if (p.active) count++;
        message(std::format(L"[Pack] Diterapkan! {} pack aktif. Reconnect ke server untuk efek.", count));
        return true;
    }

    message(L"[Pack] Subcommand tidak dikenal. Ketik .pack help.", true);
    return true;
}
