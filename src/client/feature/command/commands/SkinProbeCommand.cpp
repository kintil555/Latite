#include "pch.h"
#include "SkinProbeCommand.h"
#include "mc/common/world/actor/player/Player.h"
#include "mc/common/world/actor/player/SerializedSkinRef.h"
#include "util/Util.h"

#include <fstream>
#include <sstream>

namespace {
    bool readable(void const* p, size_t n) {
        MEMORY_BASIC_INFORMATION mbi {};
        if (!p || !VirtualQuery(p, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        auto end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        return reinterpret_cast<uintptr_t>(p) + n <= end;
    }
}

SkinProbeCommand::SkinProbeCommand()
    : Command("skinprobe", L"Finds SkinImage structs (skin/cape) inside your SerializedSkin", "", { "sp" }) {}

bool SkinProbeCommand::execute(std::string const, std::vector<std::string>) {
    auto plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (!plr) { message("No local player."); return true; }
    auto ref = plr->getSkin();
    if (!ref) { message("No skin."); return true; }

    auto impl = *reinterpret_cast<uintptr_t const*>(ref);
    if (!impl || !readable(reinterpret_cast<void*>(impl), 0x400)) { message("Skin impl unreadable."); return true; }

    std::ostringstream out;
    out << "skinImpl=0x" << std::hex << impl << std::dec << "\n";
    int found = 0;
    // SkinImage layout (see SerializedSkinRef.h): int fmt, u32 w, u32 h, u32 depth, u8 usage, pad7, blob{deleter,data,size}
    for (size_t off = 0; off + sizeof(SDK::SkinImage) <= 0x600; off += 8) {
        auto p = reinterpret_cast<void const*>(impl + off);
        if (!readable(p, sizeof(SDK::SkinImage))) break;
        auto img = reinterpret_cast<SDK::SkinImage const*>(p);
        if (img->width == 0 || img->height == 0 || img->width > 4096 || img->height > 4096) continue;
        if (img->imageFormat < 1 || img->imageFormat > 4) continue;
        if (!img->hasRgbaBytes()) continue;
        out << "+0x" << std::hex << off << std::dec << "  " << img->width << "x" << img->height
            << " fmt=" << img->imageFormat << " bytes=" << img->bytes.size() << "\n";
        found++;
    }
    out << "candidates=" << found << " (skin should be at +0xA0; a 64x32/22x17 etc. entry is the cape)\n";

    auto path = util::GetLatitePath() / "SkinProbe.txt";
    std::ofstream(path) << out.str();
    message("Wrote " + path.string() + " (" + std::to_string(found) + " candidates). Equip a cape, run again, send the file.");
    return true;
}
