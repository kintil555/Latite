#include "pch.h"
#include "AntiScreenClose.h"
#include "client/event/events/PlayerHurtScreenCloseEvent.h"

AntiScreenClose::AntiScreenClose()
    : Module("AntiScreenClose", L"Anti Screen Close",
             L"Stops the game from force-closing your open screen (chat, forms, etc.) when you take damage.",
             GAME) {
    listen<PlayerHurtScreenCloseEvent>((EventListenerFunc)&AntiScreenClose::onScreenClose);
}

void AntiScreenClose::onScreenClose(Event& evGeneric) {
    auto& ev = reinterpret_cast<PlayerHurtScreenCloseEvent&>(evGeneric);
    ev.setCancelled(true);
}
