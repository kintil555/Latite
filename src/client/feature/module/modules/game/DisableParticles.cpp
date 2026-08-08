#include "DisableParticles.h"
#include <mc/Addresses.h>

// Signature resolves to the start of the "cmp byte ptr [rbp+X], 0" gate check
// (7 bytes: 80 BD ? ? ? ? ?) immediately followed by the "jne" that skips
// particle spawning (6 bytes: 0F 85 ? ? ? ?). NOP-ing the jne forces the
// branch to always fall through, disabling particles while enabled.
static constexpr size_t cmpInstructionSize = 7;
static constexpr size_t instructionSize = 6;

static char originalBytes[instructionSize] = {};
static void* instructionPointer = nullptr;

void DisableParticles::onEnable() {
    uintptr_t base = Signatures::LevelRendererCamera_disableParticlesGate.result;
    if (!base) return;

    instructionPointer = reinterpret_cast<void*>(base + cmpInstructionSize);

    memcpy(originalBytes, instructionPointer, instructionSize);

    DWORD protect;
    VirtualProtect(instructionPointer, instructionSize, PAGE_EXECUTE_READWRITE, &protect);
    memset(instructionPointer, 0x90 /*No-Operation*/, instructionSize);
    VirtualProtect(instructionPointer, instructionSize, protect, &protect);
}

void DisableParticles::onDisable() {
    if (!instructionPointer) return;

    DWORD protect;
    VirtualProtect(instructionPointer, instructionSize, PAGE_EXECUTE_READWRITE, &protect);
    memcpy(instructionPointer, originalBytes, instructionSize);
    VirtualProtect(instructionPointer, instructionSize, protect, &protect);
}
