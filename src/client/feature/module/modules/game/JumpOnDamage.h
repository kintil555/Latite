#pragma once
#include "../../Module.h"

class JumpOnDamage : public Module {
public:
    JumpOnDamage();

    void onTick(Event& ev);
    void onBeforeMove(Event& ev);

private:
    int getDelayTicks() const;
    int getTriggerMode() const;
    int getCooldownTicks() const;

    ValueType delay = FloatValue(0.f);  // FloatValue agar slider renderer bekerja
    ValueType cooldown = FloatValue(3.f); // detik, dikonversi ke tick (20 tick/detik)
    EnumData triggerMode;

    static constexpr int kMaxHitsPerWindow = 2;

    float m_lastHealth      = -1.f;
    int   m_lastInvulnTime  = -1;
    int   m_jumpTicksLeft   = 0;
    bool  m_pendingJump     = false;
    bool  m_jumpQueued      = false;
    bool  m_releasePending  = false;

    int   m_hitsInWindow    = 0;   // jumlah hit yang sudah dihitung di window/cooldown saat ini
    int   m_cooldownTicksLeft = 0; // sisa tick cooldown sebelum counter hit direset
};
