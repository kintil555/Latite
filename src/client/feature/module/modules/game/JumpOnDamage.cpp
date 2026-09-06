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

    addSliderSetting("cooldown", L"Hit Cooldown",
                     L"Only jumps on the first 2 hits within this window (seconds). Hit count resets once the cooldown ends.",
                     this->cooldown, FloatValue(1.f), FloatValue(10.f), FloatValue(1.f));

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

int JumpOnDamage::getCooldownTicks() const {
    // Slider is in seconds (1-10); convert to ticks at 20 ticks/second.
    // Defensive fallback mirrors getDelayTicks() in case the variant was
    // ever overwritten by config deserialization.
    if (const auto* fv = std::get_if<FloatValue>(&this->cooldown)) {
        return static_cast<int>(fv->value * 20.f);
    }
    return 3 * 20;
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
        // Cooldown window is running: this hit only counts (and jumps) if
        // we haven't already used up the 2 allowed hits within it.
        if (m_hitsInWindow < kMaxHitsPerWindow) {
            m_hitsInWindow++;

            // First hit of a fresh window starts the cooldown countdown.
            if (m_hitsInWindow == 1) {
                m_cooldownTicksLeft = getCooldownTicks();
            }

            int d = getDelayTicks();
            if (d <= 0) {
                m_pendingJump = true;
            } else {
                m_jumpTicksLeft = d;
            }
        }
        // else: 3rd+ hit inside the window — counted as damage taken, but no jump.

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

    // Count down the cooldown window; once it elapses, the hit counter
    // resets and the next hit is treated as a fresh "first hit" again.
    if (m_cooldownTicksLeft > 0) {
        m_cooldownTicksLeft--;
        if (m_cooldownTicksLeft == 0) {
            m_hitsInWindow = 0;
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
