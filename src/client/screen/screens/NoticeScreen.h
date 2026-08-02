#pragma once

#include "../Screen.h"

// Shown once after the client finishes injecting.
// Informs the user this build is a modified/independent fork and states its license.
class NoticeScreen final : public Screen {
public:
    NoticeScreen();

    std::string getName() override { return "Notice"; }

    void onEnable(bool ignoreAnimations) override;
    void onDisable() override;

private:
    void onRender(Event& event);
    void onClick(Event& event);
    void onKey(Event& event);

    float anim = 0.f;
    d2d::Rect panelRect = {};
    d2d::Rect closeButtonRect = {};
};
