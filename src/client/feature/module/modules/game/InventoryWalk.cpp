#include "pch.h"
#include "InventoryWalk.h"
#include "client/Latite.h"
#include "client/input/Keyboard.h"
#include "client/event/events/RenderLayerEvent.h"
#include "client/event/events/AfterMoveEvent.h"
#include "client/event/events/TickEvent.h"
#include "mc/common/client/gui/controls/VisualTree.h"
#include "mc/common/client/gui/controls/UIControl.h"
#include "mc/common/client/game/ClientInstance.h"
#include "mc/common/world/actor/player/Player.h"

InventoryWalk::InventoryWalk()
    : Module("InventoryWalk", LocalizeString::get("client.module.inventoryWalk.name"),
             LocalizeString::get("client.module.inventoryWalk.desc"), GAME, nokeybind) {
    addSetting("allowJump", LocalizeString::get("client.module.inventoryWalk.allowJump.name"),
               LocalizeString::get("client.module.inventoryWalk.allowJump.desc"), allowJump);
    addSetting("allowSneak", LocalizeString::get("client.module.inventoryWalk.allowSneak.name"),
               LocalizeString::get("client.module.inventoryWalk.allowSneak.desc"), allowSneak);

    listen<RenderLayerEvent>(static_cast<EventListenerFunc>(&InventoryWalk::onRenderLayer));
    listen<TickEvent>(static_cast<EventListenerFunc>(&InventoryWalk::onTick));
    listen<AfterMoveEvent>(static_cast<EventListenerFunc>(&InventoryWalk::onAfterMove));
}

void InventoryWalk::applyInput(SDK::MoveInputComponent* input) {
    // Poll the real, physical keyboard state directly rather than going
    // through KeyUpdateEvent::inUI() (which is false while the inventory is
    // open, since the cursor isn't grabbed).
    auto& kb = Latite::getKeyboard();
    input->rawInputState.up = kb.isKeyDown(kb.getMappedKey("forward"));
    input->rawInputState.down = kb.isKeyDown(kb.getMappedKey("back"));
    input->rawInputState.left = kb.isKeyDown(kb.getMappedKey("left"));
    input->rawInputState.right = kb.isKeyDown(kb.getMappedKey("right"));

    if (std::get<BoolValue>(allowJump)) {
        input->rawInputState.jumpDown = kb.isKeyDown(kb.getMappedKey("jump"));
    }
    if (std::get<BoolValue>(allowSneak)) {
        input->rawInputState.sneakDown = kb.isKeyDown(kb.getMappedKey("sneak"));
    }
}

void InventoryWalk::onRenderLayer(Event& evGeneric) {
    auto& ev = reinterpret_cast<RenderLayerEvent&>(evGeneric);

    // Cache whether the current screen is an inventory/container-style screen
    // (player inventory, chest, crafting table, furnace, etc). All of these
    // share the "inventory_screen" root control name in this Minecraft
    // version. This flag is read back on the movement tick in onAfterMove,
    // since the UI tree is only available here on the render layer.
    auto* root = ev.getScreenView() && ev.getScreenView()->visualTree ? ev.getScreenView()->visualTree->rootControl
                                                                       : nullptr;
    inInventoryScreen = root && root->name == "inventory_screen";
}

void InventoryWalk::onTick(Event&) {
    // World ticks (MultiPlayerLevel::subTick) run every game tick regardless
    // of which screen is open, unlike the client-input tick that drives
    // AfterMoveEvent -- that one appears to be skipped by the game entirely
    // while a UI like the inventory is open, which meant the AfterMoveEvent
    // override below was never actually reached while testing this. Doing
    // the override here as well guarantees it runs even in that case.
    if (!inInventoryScreen) return;

    auto* localPlayer = SDK::ClientInstance::get()->getLocalPlayer();
    auto* input = localPlayer ? localPlayer->getMoveInputComponent() : nullptr;
    if (!input) return;

    applyInput(input);
}

void InventoryWalk::onAfterMove(Event& evGeneric) {
    auto& ev = reinterpret_cast<AfterMoveEvent&>(evGeneric);
    if (!inInventoryScreen) return;

    auto* input = ev.getMoveInputHandler();
    if (!input) return;

    applyInput(input);
}
