#pragma once
#include "../../Module.h"

// Keeps the currently open UI screen open when you take damage. Vanilla
// force-closes some screens (e.g. chat/forms) as soon as you get hurt —
// this listens for the game's own close-on-hurt decision (hooked in
// PlayerHooks) and cancels it while enabled, so nothing gets closed out
// from under you mid-fight.
class AntiScreenClose final : public Module {
public:
    AntiScreenClose();

    void onScreenClose(Event& ev);
};
