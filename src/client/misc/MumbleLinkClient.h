#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

// Thin wrapper around the Mumble Positional Audio "Link" protocol —
// https://www.mumble.info/documentation/developer/positional-audio/
// Mumble (and compatible clients) memory-map a file literally named
// "MumbleLink" and poll it themselves every ~50ms; the game side only ever
// writes to it, no sockets, no handshake, no external process required.
// If Mumble isn't running the mapping still succeeds (Windows creates the
// backing file on demand) — Mumble just won't be reading it, which is a
// harmless no-op, not an error.
class MumbleLinkClient final {
public:
    MumbleLinkClient();
    ~MumbleLinkClient();

    MumbleLinkClient(const MumbleLinkClient&) = delete;
    MumbleLinkClient& operator=(const MumbleLinkClient&) = delete;
    MumbleLinkClient(MumbleLinkClient&&) = delete;
    MumbleLinkClient& operator=(MumbleLinkClient&&) = delete;

    [[nodiscard]] bool isOpen() const noexcept { return linkedMemory != nullptr; }

    // avatarPos/avatarFront/avatarTop and cameraPos/cameraFront/cameraTop
    // are plain float[3] in Mumble's left-handed, meters, Y-up space.
    // context uniquely identifies "who can hear who" (Mumble only links
    // players sharing the exact same context bytes) — pass the server
    // address string. identity is shown in Mumble's user list tooltip.
    void update(const float* avatarPos, const float* avatarFront, const float* avatarTop, const float* cameraPos,
                const float* cameraFront, const float* cameraTop, std::wstring const& identity,
                std::string const& context);

private:
    // Layout is fixed by the Mumble Link protocol spec — do not reorder,
    // resize, or add padding; every field's offset is part of the ABI that
    // Mumble itself parses.
#pragma pack(push, 1)
    struct LinkedMem {
        uint32_t uiVersion;
        DWORD uiTick;
        float fAvatarPosition[3];
        float fAvatarFront[3];
        float fAvatarTop[3];
        wchar_t name[256];
        float fCameraPosition[3];
        float fCameraFront[3];
        float fCameraTop[3];
        wchar_t identity[256];
        uint32_t context_len;
        unsigned char context[256];
        wchar_t description[2048];
    };
#pragma pack(pop)

    HANDLE fileMapping = nullptr;
    LinkedMem* linkedMemory = nullptr;
};
