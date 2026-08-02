#include "pch.h"
#include "JumpOnDamage.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/BeforeMoveEvent.h"

JumpOnDamage::JumpOnDamage()
    : Module("JumpOnDamage", L"Jump On Damage",
             L"Automatically jumps once when you take damage.", GAME) {

    addSliderSetting("delay", L"Jump Delay",
                     L"Ticks to wait after taking damage before jumping. 0 = instant.",
                     delay, IntValue(0), IntValue(20), IntValue(1));

    listen<TickEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onTick));
    listen<BeforeMoveEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onBeforeMove));
}

void JumpOnDamage::onTick(Event& evGeneric) {
    auto plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (!plr) return;

    auto healthOpt = plr->getHealth();
    if (!healthOpt.has_value()) return;
    float curHealth = healthOpt.value();

    // m_lastHealth = -1 artinya belum diinisialisasi (frame pertama)
    if (m_lastHealth < 0.f) {
        m_lastHealth = curHealth;
        return;
    }

    // Health turun = kena damage. Hanya trigger jika tidak sedang nunggu reset.
    if (curHealth < m_lastHealth && !m_waitingForReset) {
        m_waitingForReset = true;
        int d = std::get<IntValue>(delay).value;
        if (d <= 0) {
            m_pendingJump = true;
        } else {
            m_jumpTicksLeft = d;
        }
    }

    // Reset kunci saat health stabil (tidak turun lagi)
    // Pakai threshold kecil untuk regen natural
    if (curHealth >= m_lastHealth) {
        m_waitingForReset = false;
    }

    // Countdown delay
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

    mic->rawInputState.jumpDown               = true;
    mic->rawInputState.jumpInputWasPressed     = true;
    mic->rawInputState.jumpInputCurrentlyDown  = true;
    mic->jumping                               = true;

    m_pendingJump = false;
}
