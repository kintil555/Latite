#include "pch.h"
#include "JumpOnDamage.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/BeforeMoveEvent.h"

JumpOnDamage::JumpOnDamage()
    : Module("JumpOnDamage", L"Jump On Damage",
             L"Automatically jumps once when you take damage.", GAME, nokeybind) {

    addSliderSetting("delay", L"Jump Delay",
                     L"Ticks to wait after taking damage before jumping. 0 = instant.",
                     this->delay, FloatValue(0.f), FloatValue(20.f), FloatValue(1.f));

    listen<TickEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onTick));
    listen<BeforeMoveEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onBeforeMove));
}

int JumpOnDamage::getDelayTicks() const {
    // Defensive: never assume the active alternative. If the variant was
    // ever overwritten with a non-FloatValue (e.g. by config/setting
    // deserialization), fall back to 0 instead of crashing.
    if (const auto* fv = std::get_if<FloatValue>(&this->delay)) {
        return static_cast<int>(fv->value);
    }
    return 0;
}

void JumpOnDamage::onTick(Event& evGeneric) {
    auto plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (!plr) return;

    auto healthOpt = plr->getHealth();
    if (!healthOpt.has_value()) return;
    float curHealth = healthOpt.value();

    if (m_lastHealth < 0.f) {
        m_lastHealth = curHealth;
        return;
    }

    if (curHealth < m_lastHealth && !m_jumpQueued) {
        int d = getDelayTicks();
        if (d <= 0) {
            m_pendingJump = true;
        } else {
            m_jumpTicksLeft = d;
        }
        m_jumpQueued = true;
    }

    if (curHealth >= m_lastHealth + 0.5f) {
        m_jumpQueued = false;
    }

    if (m_jumpTicksLeft > 0) {
        m_jumpTicksLeft--;
        if (m_jumpTicksLeft == 0) {
            m_pendingJump = true;
        }
    }

    m_lastHealth = curHealth;
}

void JumpOnDamage::onBeforeMove(Event& evGeneric) {
    if (!m_pendingJump) return;

    auto& ev  = reinterpret_cast<BeforeMoveEvent&>(evGeneric);
    auto  mic = ev.getMoveInputHandler();
    if (!mic) return;

    mic->rawInputState.jumpDown              = true;
    mic->rawInputState.jumpInputWasPressed    = true;
    mic->rawInputState.jumpInputCurrentlyDown = true;
    mic->jumping                             = true;

    m_pendingJump = false;
}
