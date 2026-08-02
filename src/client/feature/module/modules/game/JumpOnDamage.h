#pragma once
#include "../../Module.h"

class JumpOnDamage : public Module {
public:
    JumpOnDamage();

    void onTick(Event& ev);
    void onBeforeMove(Event& ev);

private:
    // Delay dalam tick setelah kena damage sebelum jump (0 = langsung)
    ValueType delay = IntValue(0);

    int  m_lastInvulTime  = 0;  // invulnerableTime tick sebelumnya
    int  m_jumpTicksLeft  = 0;  // countdown sebelum trigger jump
    bool m_pendingJump    = false;
};
