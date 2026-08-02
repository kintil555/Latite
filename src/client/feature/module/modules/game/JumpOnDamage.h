#pragma once
#include "../../Module.h"

class JumpOnDamage : public Module {
public:
    JumpOnDamage();

    void onTick(Event& ev);
    void onBeforeMove(Event& ev);

private:
    ValueType delay = IntValue(0);  // slider 0-20 ticks

    int  m_lastInvulTime   = 0;
    int  m_jumpTicksLeft   = 0;
    bool m_pendingJump     = false;
    bool m_waitingForReset = false;  // cegah jump berulang dari 1 damage
};
