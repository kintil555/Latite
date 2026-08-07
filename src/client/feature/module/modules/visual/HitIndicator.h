#pragma once
#include "../../Module.h"

class HitIndicator : public Module {
public:
    void onRenderLevel(RenderLevelEvent& event);

    HitIndicator();

private:
    static constexpr int style_fullBox = 0;
    static constexpr int style_outline = 1;

    EnumData style;

    ValueType inReachColor = ColorValue(1.f, 0.f, 0.f, 0.5f);
    ValueType outOfReachColor = ColorValue(1.f, 1.f, 1.f, 0.5f);

    ValueType reach = FloatValue(3.f);

    ValueType transparent = BoolValue(true);
};
