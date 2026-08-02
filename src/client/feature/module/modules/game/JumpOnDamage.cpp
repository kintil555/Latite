#include "pch.h"
#include "JumpOnDamage.h"
#include "client/event/events/TickEvent.h"
#include "client/event/events/BeforeMoveEvent.h"
#include "mc/common/world/actor/Actor.h"
#include "mc/common/client/ClientInstance.h"

JumpOnDamage::JumpOnDamage()
    : Module("JumpOnDamage", L"Jump On Damage",
             L"Automatically jumps when you take damage.", GAME) {

    addSetting("delay", L"Jump Delay",
               L"Ticks to wait after taking damage before jumping (0 = instant).",
               delay);

    listen<TickEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onTick));
    listen<BeforeMoveEvent>(static_cast<EventListenerFunc>(&JumpOnDamage::onBeforeMove));
}

void JumpOnDamage::onTick(Event& evGeneric) {
    auto plr = SDK::ClientInstance::get()->getLocalPlayer();
    if (!plr) return;

    int curInvul = plr->invulnerableTime;

    // Detect rising edge: invulnerableTime naik berarti baru kena damage
    if (curInvul > m_lastInvulTime) {
        int d = std::get<IntValue>(delay);
        if (d <= 0) {
            m_pendingJump = true;
        } else {
            m_jumpTicksLeft = d;
        }
    }

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

    // Set semua flag jump di rawInputState dan MoveInputComponent
    mic->rawInputState.jumpDown            = true;
    mic->rawInputState.jumpInputWasPressed  = true;
    mic->rawInputState.jumpInputCurrentlyDown = true;
    mic->jumping                            = true;

    m_pendingJump = false;
}
