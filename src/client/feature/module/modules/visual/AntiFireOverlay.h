#pragma once
#include "../../Module.h"

// Removes the red/orange fire overlay that covers the screen while your
// camera is inside fire or you're on fire — pure inline instruction patch
// (no hooked function), same approach as ThirdPersonNametag/DisableParticles:
// overwrite the "movss xmm7, [rbx+X]" load that feeds the overlay's opacity
// multiply with a zeroing "xorps xmm7, xmm7" instead, so the overlay's
// alpha/offset is always 0.
class AntiFireOverlay final : public Module {
public:
    AntiFireOverlay()
        : Module("AntiFireOverlay", L"Anti Fire Overlay",
                 L"Removes the fire screen overlay so it doesn't block your view while burning.", GAME) {}

    ~AntiFireOverlay() override = default;

    void onEnable() override;
    void onDisable() override;
};
