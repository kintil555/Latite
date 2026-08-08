#pragma once
#include <client/feature/module/Module.h>

#include <chrono>
#include <string>

class HideAndSeekCrashWarning : public Module {
public:
    HideAndSeekCrashWarning();
    virtual ~HideAndSeekCrashWarning() {};

    // Always-on safety notice: not shown in ClickGUI (visible = false) and
    // its listeners are registered directly with Eventing rather than gated
    // behind Module::isEnabled(), so it cannot be turned off by the player.
    // It also does its own "/connection" polling instead of relying on
    // DiscordPresence, since that module is a regular toggleable module and
    // might be disabled by the player.
    void onUpdate(class UpdateEvent& ev);
    void onClientText(class ClientTextEvent& ev);
    void onLeaveGame(class LeaveGameEvent& ev);

private:
    void updateConnectionState();

    std::string activeServerAddress;
    bool onHive = false;
    bool warned = false;

    std::chrono::steady_clock::time_point connectionRefreshAt {};
    std::chrono::steady_clock::time_point suppressUntil {};
};
