#pragma once
#include "../../Module.h"

class JumpOnDamage : public Module {
public:
    JumpOnDamage();

    void onTick(Event& ev);
    void onBeforeMove(Event& ev);

private:
    ValueType delay = FloatValue(0.f);  // FloatValue agar slider renderer bekerja

    float m_lastHealth    = -1.f;
    int   m_jumpTicksLeft = 0;
    bool  m_pendingJump   = false;
    bool  m_jumpQueued    = false;
};
