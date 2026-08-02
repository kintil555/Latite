#include "pch.h"
#include "ScreenManager.h"
#include "screens/ClickGUI.h"
#include "screens/HUDEditor.h"
#include "mc/common/client/game/ClientInstance.h"
#include "client/event/events/KeyUpdateEvent.h"

ScreenManager::ScreenManager() {
    Eventing::get().listen<KeyUpdateEvent, &ScreenManager::onKey>(this);
    Eventing::get().listen<FocusLostEvent, &ScreenManager::onFocusLost>(this);
    Eventing::get().listen<UpdateEvent, &ScreenManager::onUpdate>(this);
}

void ScreenManager::activateScreen(Screen& screen, bool ignoreAnims) {
    if (this->activeScreen && &this->activeScreen->get() == &screen) {
        SDK::ClientInstance::get()->releaseCursor();
        return;
    }

    if (this->activeScreen) {
        this->activeScreen->get().setActive(false);
    }

    this->activeScreen = screen;
    screen.setActive(true, ignoreAnims);
    SDK::ClientInstance::get()->releaseCursor();
}

void ScreenManager::exitCurrentScreen() {
    if (this->activeScreen) {
        this->activeScreen->get().resetInputState();
        this->activeScreen->get().setActive(false);
        this->activeScreen = std::nullopt;

        // Only grab the cursor when actually in-game (a world is loaded and we
        // have a local player). Screens like NoticeScreen can be shown right
        // after inject, before any world exists -- unconditionally grabbing the
        // cursor here left the mouse hidden on the main menu and caused stray
        // HUD elements (e.g. the HUD editor's floating "Mod Settings" button) to
        // render there too, since isCursorGrabbed() is used elsewhere as a proxy
        // for "currently in gameplay".
        auto* client = SDK::ClientInstance::get();
        if (client && client->getLocalPlayer()) {
            client->grabCursor();
        } else if (client) {
            client->releaseCursor();
        }
    }
}

void ScreenManager::shutdownForEject() {
    if (shuttingDown.exchange(true, std::memory_order_acq_rel)) return;
    exitCurrentScreen();
}

void ScreenManager::onKey(KeyUpdateEvent& ev) {
    if (ev.isDown() && ev.getKey() == VK_ESCAPE && getActiveScreen()) {
        exitCurrentScreen();
        ev.setCancelled(true);
        return;
    }

    std::optional<std::reference_wrapper<Screen>> associatedScreen;
    this->forEach([&](Screen& s) {
        if (s.key == ev.getKey()) associatedScreen = s;
    });

    if (associatedScreen && ev.isDown() && (!ev.inUI() || getActiveScreen())) {
        if (getActiveScreen())
            exitCurrentScreen();
        else {
            activateScreen(associatedScreen->get());
        }
        ev.setCancelled(true);
        return;
    }
}

void ScreenManager::onFocusLost(FocusLostEvent& ev) {
    if (getActiveScreen()) {
        getActiveScreen()->get().resetInputState();
        if (auto client = SDK::ClientInstance::get()) {
            client->releaseCursor();
        }
        ev.setCancelled(true);
    }
}

void ScreenManager::onUpdate(UpdateEvent&) {
    auto client = SDK::ClientInstance::get();
    if (getActiveScreen() && client && client->minecraftGame && client->minecraftGame->isCursorGrabbed()) {
        client->releaseCursor();
    }
}
