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

    ValueType delay = FloatValue(0.f);  // FloatValue agar slider renderer bekerja
    EnumData triggerMode;

    float m_lastHealth      = -1.f;
    int   m_lastInvulnTime  = -1;
    int   m_jumpTicksLeft   = 0;
    bool  m_pendingJump     = false;
    bool  m_jumpQueued      = false;
    bool  m_releasePending  = false;
};
