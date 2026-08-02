#include "pch.h"
#include "JumpOnDamage.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/BeforeMoveEvent.h"

JumpOnDamage::JumpOnDamage()
    : Module("JumpOnDamage", L"Jump On Damage",
             L"Automatically jumps once when you take damage.", GAME) {

    // Slider: 0 = lompat langsung saat kena hit, lebih tinggi = lebih lambat
    addSliderSetting("delay", L"Jump Delay",
                     L"Ticks to wait after taking damage before jumping. 0 = instant.",
                     delay, IntValue(0), IntValue(20), IntValue(1));

    listen<TickEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onTick));
    listen<BeforeMoveEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onBeforeMove));
}

void JumpOnDamage::onTick(Event& evGeneric) {
    auto plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (!plr) return;

    int curInvul = plr->invulnerableTime;

    // Rising edge: invulnerableTime naik = baru kena damage
    // m_waitingForReset memastikan tidak trigger lagi sampai invulTime
    // sudah turun ke 0 dulu (damage baru)
    if (curInvul > m_lastInvulTime && !m_waitingForReset) {
        m_waitingForReset = true;  // kunci sampai damage berikutnya
        int d = std::get<IntValue>(delay).value;
        if (d <= 0) {
            m_pendingJump = true;
        } else {
            m_jumpTicksLeft = d;
        }
    }

    // Reset kunci ketika invulTime sudah habis (siap terima damage baru)
    if (curInvul == 0) {
        m_waitingForReset = false;
    }

    // Countdown delay
    if (m_jumpTicksLeft > 0) {
        m_jumpTicksLeft--;
        if (m_jumpTicksLeft == 0) {
            m_pendingJump = true;
        }
    }

    m_lastInvulTime = curInvul;
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

    m_pendingJump = false;  // reset - hanya jump sekali
}
