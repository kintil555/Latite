#pragma once
#include "../../Module.h"
#include "client/misc/MumbleLinkClient.h"

#include <memory>

class TickEvent;

// Proximity voice chat via the Mumble Positional Audio "Link" protocol.
// This client never touches audio or networking itself — it only writes
// the local player's position/orientation into a shared-memory block that
// Mumble (or any Link-compatible voice client) reads on its own every
// frame. Free, no extra server, near-zero overhead: one memcpy per tick.
// Everyone hearing each other still needs Mumble open and joined to the
// same voice server — this only supplies the positional data, matching
// people up automatically only if they're also on the same Minecraft
// server (see MumbleLinkClient's context, keyed off host:port).
class ProximityChat final : public Module {
public:
    ProximityChat();
    ~ProximityChat() override;

    void onEnable() override;
    void onDisable() override;
    void onTick(Event& ev);

private:
    std::unique_ptr<MumbleLinkClient> linkClient;

    ValueType distanceScale = FloatValue(1.f); // blocks-per-meter fudge factor, 0.5 - 3
};
