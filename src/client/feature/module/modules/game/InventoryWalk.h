#pragma once
#include "../../Module.h"

namespace SDK {
    class MoveInputComponent;
}

class InventoryWalk : public Module {
public:
    InventoryWalk();

    void onRenderLayer(Event& ev);
    void onTick(Event& ev);
    void onBeforeMove(Event& ev);

private:
    void applyInput(SDK::MoveInputComponent* input);

    ValueType allowJump = BoolValue(true);
    ValueType allowSneak = BoolValue(true);

    // Cached every frame from RenderLayerEvent (the only place the current
    // screen's UIControl tree is available); read back on both TickEvent
    // (world tick) and BeforeMoveEvent (fired right before the game's own
    // ClientInputUpdateSystemInternal::tickUpdateClientInput runs, so our
    // override is what the engine actually reads that tick -- AfterMoveEvent
    // fires only once the real function has already consumed the input,
    // which made the override apply a tick late).
    bool inInventoryScreen = false;
};
