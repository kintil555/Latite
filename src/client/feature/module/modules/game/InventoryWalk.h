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
    void onAfterMove(Event& ev);

private:
    void applyInput(SDK::MoveInputComponent* input);

    ValueType allowJump = BoolValue(true);
    ValueType allowSneak = BoolValue(true);

    // Cached every frame from RenderLayerEvent (the only place the current
    // screen's UIControl tree is available); read back on both TickEvent
    // (world tick -- always runs) and AfterMoveEvent (client input tick --
    // appears to be skipped while a UI is open, kept as a belt-and-suspenders
    // override in case that's not true on every screen).
    bool inInventoryScreen = false;
};
