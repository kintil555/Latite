#include "pch.h"
#include "MumbleLinkClient.h"

#include <algorithm>
#include <cstring>

MumbleLinkClient::MumbleLinkClient() {
    // Name is fixed by the protocol — Mumble looks for exactly "MumbleLink"
    // (no prefix/suffix), so this can't be namespaced per-instance.
    fileMapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(LinkedMem), L"MumbleLink");
    if (!fileMapping) return;

    linkedMemory = static_cast<LinkedMem*>(MapViewOfFile(fileMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(LinkedMem)));
    if (!linkedMemory) {
        CloseHandle(fileMapping);
        fileMapping = nullptr;
        return;
    }

    memset(linkedMemory, 0, sizeof(LinkedMem));
}

MumbleLinkClient::~MumbleLinkClient() {
    if (linkedMemory) {
        // Zero uiVersion so Mumble immediately treats the link as inactive
        // instead of holding onto our last-known position after we unmap.
        linkedMemory->uiVersion = 0;
        UnmapViewOfFile(linkedMemory);
        linkedMemory = nullptr;
    }

    if (fileMapping) {
        CloseHandle(fileMapping);
        fileMapping = nullptr;
    }
}

void MumbleLinkClient::update(const float* avatarPos, const float* avatarFront, const float* avatarTop,
                              const float* cameraPos, const float* cameraFront, const float* cameraTop,
                              std::wstring const& identity, std::string const& context) {
    if (!linkedMemory) return;

    if (linkedMemory->uiVersion != 2) {
        linkedMemory->uiVersion = 2;
        wcsncpy_s(linkedMemory->name, L"Minecraft Bedrock (Latite)", _TRUNCATE);
        // description is shown to the user when they link a new app in
        // Mumble's UI; it isn't otherwise used, so it only needs setting once.
        wcsncpy_s(linkedMemory->description, L"Latite proximity chat via Minecraft Bedrock.", _TRUNCATE);
    }

    memcpy(linkedMemory->fAvatarPosition, avatarPos, sizeof(linkedMemory->fAvatarPosition));
    memcpy(linkedMemory->fAvatarFront, avatarFront, sizeof(linkedMemory->fAvatarFront));
    memcpy(linkedMemory->fAvatarTop, avatarTop, sizeof(linkedMemory->fAvatarTop));
    memcpy(linkedMemory->fCameraPosition, cameraPos, sizeof(linkedMemory->fCameraPosition));
    memcpy(linkedMemory->fCameraFront, cameraFront, sizeof(linkedMemory->fCameraFront));
    memcpy(linkedMemory->fCameraTop, cameraTop, sizeof(linkedMemory->fCameraTop));

    wcsncpy_s(linkedMemory->identity, identity.c_str(), _TRUNCATE);

    // context_len + context together are Mumble's "same channel" key: two
    // clients only hear each other positionally if these bytes match
    // exactly, so this is what keeps players on different servers from
    // being linked just because they both run this client.
    auto len = static_cast<uint32_t>(std::min(context.size(), sizeof(linkedMemory->context)));
    memcpy(linkedMemory->context, context.data(), len);
    linkedMemory->context_len = len;

    linkedMemory->uiTick++;
}
