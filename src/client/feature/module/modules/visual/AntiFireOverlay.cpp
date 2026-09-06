#include "AntiFireOverlay.h"
#include <mc/Addresses.h>

// Signature resolves to the start of the second instruction in the pair:
//   F3 0F 10 25 ?? ?? ?? ??   movss xmm4, [rip+X]      (scale constant, untouched)
//   F3 0F 10 7B ??            movss xmm7, [rbx+X]      (raw overlay field — this is what we patch)
//   F3 0F 59 FC               mulss xmm7, xmm4         (result feeds the overlay's opacity/offset)
// Replacing the "movss xmm7, [rbx+X]" load (5 bytes) with "xorps xmm7, xmm7"
// (3 bytes: 0F 57 FF) + 2 bytes of NOP padding forces xmm7 to 0.0f before
// the multiply, so the overlay's value comes out to 0 no matter what the
// scale constant or the underlying field actually is.
static constexpr size_t instructionSize = 5;
static const unsigned char patchedBytes[instructionSize] = { 0x0F, 0x57, 0xFF, 0x90, 0x90 };

static char originalBytes[instructionSize] = {};
static void* instructionPointer = nullptr;

void AntiFireOverlay::onEnable() {
    uintptr_t base = Signatures::FireOverlayOffset.result;
    if (!base) return;

    instructionPointer = reinterpret_cast<void*>(base);

    memcpy(originalBytes, instructionPointer, instructionSize);

    DWORD protect;
    VirtualProtect(instructionPointer, instructionSize, PAGE_EXECUTE_READWRITE, &protect);
    memcpy(instructionPointer, patchedBytes, instructionSize);
    VirtualProtect(instructionPointer, instructionSize, protect, &protect);
}

void AntiFireOverlay::onDisable() {
    if (!instructionPointer) return;

    DWORD protect;
    VirtualProtect(instructionPointer, instructionSize, PAGE_EXECUTE_READWRITE, &protect);
    memcpy(instructionPointer, originalBytes, instructionSize);
    VirtualProtect(instructionPointer, instructionSize, protect, &protect);
}
