#pragma once
#include "../../Module.h"

class JumpOnDamage : public Module {
public:
    JumpOnDamage();

    void onTick(Event& ev);
    void onBeforeMove(Event& ev);

private:
    ValueType delay = IntValue(0);

    float m_lastHealth    = -1.f;  // -1 = belum init
    int   m_jumpTicksLeft = 0;
    bool  m_pendingJump   = false;
    bool  m_jumpQueued    = false;  // kunci: satu jump per hit event
};
