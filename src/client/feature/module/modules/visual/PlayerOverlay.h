#pragma once
#include "../../Module.h"
#include <unordered_map>

// Debug: draws a small WAILA-style panel above every player's nametag showing their name and
// whether they're currently visible. If a player turns invisible, switches to spectator, or
// otherwise stops rendering, the panel keeps showing at their last known position until they
// become visible again. Cache can be wiped with the "Clear Cache" action setting.
class PlayerOverlay final : public Module {
public:
    PlayerOverlay();

private:
    struct CachedPlayer {
        std::wstring name;
        Vec3 anchorPos;
    };

    void onRenderLayer(Event& event);

    ValueType panelColor = ColorValue(0.f, 0.f, 0.f, 0.55f);
    ValueType nameColor = ColorValue(1.f, 1.f, 1.f, 1.f);
    ValueType liveColor = ColorValue(0.4f, 1.f, 0.4f, 1.f);
    ValueType lastSeenColor = ColorValue(1.f, 0.55f, 0.2f, 1.f);
    ValueType textSize = FloatValue(13.f);
    ValueType maxDistance = FloatValue(128.f);

    // Keyed by Actor::getRuntimeID(). Holds the last known name/position for players that have
    // gone invisible so their panel can keep rendering while they're gone.
    std::unordered_map<uint64_t, CachedPlayer> cache;
};
