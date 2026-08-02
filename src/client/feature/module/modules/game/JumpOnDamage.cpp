#include "pch.h"
#include "JumpOnDamage.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/BeforeMoveEvent.h"

JumpOnDamage::JumpOnDamage()
    : Module("JumpOnDamage", L"Jump On Damage",
             L"Automatically jumps once when you take damage.", GAME, nokeybind) {

    triggerMode.addEntry(EnumEntry(0, L"Health Decrease", L"Triggers when your health value drops."));
    triggerMode.addEntry(EnumEntry(1, L"Hurt Animation", L"Triggers on the hit/hurt animation, even at full health (e.g. absorption)."));
    addEnumSetting("triggerMode", L"Trigger Mode",
                  L"What counts as taking damage.", triggerMode);

    addSliderSetting("delay", L"Jump Delay",
                     L"Ticks to wait after taking damage before jumping. 0 = instant.",
                     this->delay, FloatValue(0.f), FloatValue(20.f), FloatValue(1.f));

    listen<TickEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onTick));
    listen<BeforeMoveEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onBeforeMove));
}

int JumpOnDamage::getTriggerMode() const {
    return triggerMode.getSelectedKey();
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

    bool tookDamage = false;

    if (getTriggerMode() == 1) {
        // Hurt animation mode: invulnerableTime jumps to its max value the
        // tick a hit lands, then decays to 0. A 0 -> >0 transition means we
        // just got hit, regardless of whether health actually dropped.
        int invuln = plr->invulnerableTime;

        if (m_lastInvulnTime < 0) {
            m_lastInvulnTime = invuln;
            return;
        }

        tookDamage = invuln > 0 && m_lastInvulnTime == 0;
        m_lastInvulnTime = invuln;
    } else {
        auto healthOpt = plr->getHealth();
        if (!healthOpt.has_value()) return;
        float curHealth = healthOpt.value();

        if (m_lastHealth < 0.f) {
            m_lastHealth = curHealth;
            return;
        }

        tookDamage = curHealth < m_lastHealth;

        if (curHealth >= m_lastHealth + 0.5f) {
            m_jumpQueued = false;
        }

        m_lastHealth = curHealth;
    }

    if (tookDamage && !m_jumpQueued) {
        int d = getDelayTicks();
        if (d <= 0) {
            m_pendingJump = true;
        } else {
            m_jumpTicksLeft = d;
        }
        m_jumpQueued = true;

        // Hurt mode has no continuous "still hurt" signal like health does
        // to naturally close the queue, so release it immediately — the
        // invulnerableTime 0->>0 transition already prevents re-triggering
        // every tick on its own.
        if (getTriggerMode() == 1) {
            m_jumpQueued = false;
        }
    }

    if (m_jumpTicksLeft > 0) {
        m_jumpTicksLeft--;
        if (m_jumpTicksLeft == 0) {
            m_pendingJump = true;
        }
    }
}

void JumpOnDamage::onBeforeMove(Event& evGeneric) {
    auto& ev  = reinterpret_cast<BeforeMoveEvent&>(evGeneric);
    auto  mic = ev.getMoveInputHandler();
    if (!mic) return;

    if (m_pendingJump) {
        mic->rawInputState.jumpDown              = true;
        mic->rawInputState.jumpInputWasPressed    = true;
        mic->rawInputState.jumpInputCurrentlyDown = true;
        mic->jumping                             = true;

        m_pendingJump = false;
        m_releasePending = true;
        return;
    }

    // Release the key on the tick right after the tap, so we never leave
    // jumpDown latched as "held" once the game reads our forced input.
    if (m_releasePending) {
        mic->rawInputState.jumpDown              = false;
        mic->rawInputState.jumpInputCurrentlyDown = false;
        m_releasePending = false;
    }
}
