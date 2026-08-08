#pragma once
#include <client/feature/module/Module.h>

class HideAndSeekCrashWarning : public Module {
public:
    HideAndSeekCrashWarning();
    virtual ~HideAndSeekCrashWarning() {};

    // Always-on safety notice: not shown in ClickGUI (visible = false) and
    // its listener is registered directly with Eventing rather than gated
    // behind Module::isEnabled(), so it cannot be turned off by the player.
    void onClientText(class ClientTextEvent& ev);
    void onLeaveGame(class LeaveGameEvent& ev);

private:
    bool warned = false;
};
