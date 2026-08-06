#include "Platform_GameCore.h"

SDK::MinecraftGame* SDK::Platform_GameCore::getMinecraftGame() {
    return hat::member_at<MinecraftGame*>(this, 0x38);
}

SDK::GameCore* SDK::Platform_GameCore::getGameCore() {
    return hat::member_at<GameCore*>(this, 0xD0);
}

SDK::Platform_GameCore* SDK::Platform_GameCore::get() {
    auto* base = *reinterpret_cast<void**>(Signatures::Misc::Platform_GameCore.result);
    if (!base) return nullptr;
    return hat::member_at<Platform_GameCore*>(base, 0x8);
}
