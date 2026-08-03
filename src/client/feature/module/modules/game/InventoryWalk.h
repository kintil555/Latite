#pragma once
#include "../../Module.h"

class InventoryWalk : public Module {
public:
    InventoryWalk();

    void onRenderLayer(Event& ev);
    void onAfterMove(Event& ev);

private:
    ValueType allowJump = BoolValue(true);
    ValueType allowSneak = BoolValue(true);

    // Cached every frame from RenderLayerEvent (the only place the current
    // screen's UIControl tree is available); read back in onAfterMove, which
    // runs on the movement tick rather than the render loop.
    bool inInventoryScreen = false;
};
