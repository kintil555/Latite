#pragma once
#include "../../Module.h"
#include <unordered_map>
#include <vector>

// Battle-royale style floating damage numbers: when you land a hit, the
// amount of health it took off shows above the target and floats up while
// fading out. Not chat-based — this is a pure 3D-world overlay drawn every
// frame via RenderLayerEvent, positioned with WorldToScreen::convert.
class DamageIndicator final : public Module {
public:
    DamageIndicator();

    void onAttack(Event& ev);
    void onPacketReceive(Event& ev);
    void onTick(Event& ev);
    void onRenderLayer(Event& ev);

private:
    struct Popup {
        Vec3 anchorPos;
        float damage;
        std::chrono::steady_clock::time_point spawnedAt;
    };

    // Pending hit waiting for the HURT_ANIMATION actor-event that confirms
    // the attack actually landed (same confirm pattern as ComboCounter).
    uint64_t m_pendingRuntimeId = 0;
    bool     m_hasPendingHit    = false;

    // HURT_ANIMATION only confirms the hit landed — it does NOT mean the
    // target's health attribute has been applied yet (that comes from a
    // separate attribute-sync packet that isn't guaranteed to be processed
    // before we see the ACTOR_EVENT packet). Reading getHealth() right in
    // onPacketReceive was reading stale pre-hit health almost every time,
    // so "dealt" came out <= 0 and the popup got silently dropped. Instead,
    // once the hit is confirmed we wait a couple of ticks (same approach as
    // JumpOnDamage's Health Decrease mode) before diffing health, giving
    // the game time to actually apply the update.
    bool     m_awaitingHealthSync   = false;
    uint64_t m_confirmedRuntimeId   = 0;
    int      m_healthSyncTicksLeft  = 0;

    // Last known health per target runtime ID, used to compute the damage
    // delta once a hit is confirmed. Cleared entries are fine to miss —
    // a first-ever hit on an entity falls back to "unknown" and is skipped.
    std::unordered_map<uint64_t, float> m_lastHealth;

    std::vector<Popup> m_popups;

    ValueType textColor    = ColorValue(1.f, 0.2f, 0.2f, 1.f);
    ValueType critColor    = ColorValue(1.f, 0.85f, 0.1f, 1.f);
    ValueType critThreshold = FloatValue(6.f);
    ValueType textSize     = FloatValue(16.f);
    ValueType riseSpeed    = FloatValue(0.6f);
    ValueType lifetime     = FloatValue(1.f); // seconds, 0.5 - 3
};
