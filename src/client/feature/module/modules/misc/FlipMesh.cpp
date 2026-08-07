#include "pch.h"
#include "FlipMesh.h"
#include "FlipMeshAsset.h"
#include "client/Latite.h"

#include <filesystem>
#include <fstream>
#include <Windows.h>

namespace {
    namespace fs = std::filesystem;

    // GDK builds moved com.mojang from the old UWP package path to %appdata%\Minecraft Bedrock.
    // Falls back to the Preview folder if the release folder isn't present.
    fs::path GetComMojangPath() {
        wchar_t buf[MAX_PATH]{};
        auto len = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return {};

        fs::path appdata(buf);
        fs::path release = appdata / L"Minecraft Bedrock" / L"users" / L"shared" / L"games" / L"com.mojang";
        if (fs::exists(release)) return release;

        fs::path preview = appdata / L"Minecraft Bedrock Preview" / L"users" / L"shared" / L"games" / L"com.mojang";
        if (fs::exists(preview)) return preview;

        return release; // best-effort default; writePack() will report failure if it can't be created
    }

    fs::path GetPackDir() {
        auto mojang = GetComMojangPath();
        if (mojang.empty()) return {};
        return mojang / L"development_resource_packs" / L"LatiteFlipMesh";
    }
}

FlipMesh::FlipMesh()
    : Module("FlipMesh", L"FlipMesh", L"Injects a flipped entity mesh resource pack into com.mojang.", GAME, 0,
             false, false) {
}

void FlipMesh::onEnable() {
    if (!writePack()) {
        Latite::getClientMessageQueue().push(
            std::string("[FlipMesh] Failed to write resource pack. Check that Minecraft Bedrock has been run at "
                        "least once."));
        setEnabled(false);
        return;
    }
    Latite::getClientMessageQueue().push(
        std::string("[FlipMesh] Pack written to development_resource_packs. Rejoin your world to apply."));
}

void FlipMesh::onDisable() {
    removePack();
}

bool FlipMesh::writePack() {
    auto dir = GetPackDir();
    if (dir.empty()) return false;

    std::error_code ec;
    fs::create_directories(dir / L"renderer" / L"materials", ec);
    if (ec) return false;

    std::ofstream manifest(dir / L"manifest.json", std::ios::binary | std::ios::trunc);
    if (!manifest) return false;
    manifest << FlipMeshAsset::kManifest;
    manifest.close();

    std::ofstream material(dir / L"renderer" / L"materials" / L"entity.material", std::ios::binary | std::ios::trunc);
    if (!material) return false;
    material << FlipMeshAsset::kEntityMaterial;
    material.close();

    return true;
}

void FlipMesh::removePack() {
    auto dir = GetPackDir();
    if (dir.empty()) return;

    std::error_code ec;
    fs::remove_all(dir, ec);
}
