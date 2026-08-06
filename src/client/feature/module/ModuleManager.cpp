#include "pch.h"
#include "ModuleManager.h"
#include "modules/misc/TestModule.h"
#include "modules/misc/DebugInfo.h"
#include "modules/misc/Nickname.h"
#include "modules/misc/ItemTweaks.h"
#include "modules/misc/DebugInfo.h"
#include "modules/misc/CommandShortcuts.h"
#include "modules/misc/DiscordPresence.h"
#include "modules/misc/BlockGame.h"
#include "modules/misc/SkinStealer.h"
// FlipMesh disabled: FlipMeshAsset.h missing from repo, excluded from build in CMakeLists.txt
// #include "modules/misc/FlipMesh.h"

#include "modules/game/Zoom.h"
#include "modules/game/CinematicCamera.h"
#include "modules/game/ToggleSprintSneak.h"
#include "modules/game/BehindYou.h"
#include "modules/game/ThirdPersonNametag.h"
#include "modules/game/EnvironmentChanger.h"
#include "modules/game/TextHotkey.h"
#include "modules/game/Freelook.h"
#include "modules/game/AutoGG.h"
#include "modules/game/HiveTranslate.h"
#include "modules/game/KillNotification.h"
#include "modules/game/JumpOnDamage.h"
#include "modules/game/Gyro.h"
#include "modules/game/InventoryWalk.h"

#include "modules/visual/Fullbright.h"
#include "modules/visual/MotionBlur.h"
#include "modules/visual/HurtColor.h"
#include "modules/visual/Hitboxes.h"
#include "modules/visual/HitIndicator.h"
#include "modules/visual/ChunkBorders.h"
#include "modules/visual/Hitboxes.h"
#include "modules/visual/BlockOutline.h"
#include "modules/visual/PlayerOverlay.h"

#include "modules/hud/FPSCounter.h"
#include "modules/hud/CPSCounter.h"
#include "modules/hud/ServerDisplay.h"
#include "modules/hud/PingDisplay.h"
#include "modules/hud/SpeedDisplay.h"
#include "modules/hud/Clock.h"
#include "modules/hud/BowIndicator.h"
#include "modules/hud/GuiscaleChanger.h"
#include "modules/hud/TabList.h"
#include "modules/hud/Keystrokes.h"
#include "modules/hud/BreakIndicator.h"
#include "modules/hud/HealthWarning.h"
#include "modules/hud/ArmorHUD.h"
#include "modules/hud/MovablePaperdoll.h"
#include "modules/hud/MovableScoreboard.h"
#include "modules/hud/ReachDisplay.h"
#include "modules/hud/MovableBossbar.h"
#include "modules/hud/ItemCounter.h"
#include "modules/hud/Chat.h"
#include "modules/hud/ComboCounter.h"
#include "modules/hud/CustomCoordinates.h"
#include "modules/hud/MovableCoordinates.h"
#include "modules/hud/FrameTimeDisplay.h"
#include "modules/hud/WAILA.h"

#include "client/event/events/KeyUpdateEvent.h"

ModuleManager::ModuleManager() {
    // jod-only branch: only JumpOnDamage is registered, no other modules/GUI clutter.
    this->items.push_back(std::make_shared<JumpOnDamage>());

    for (auto& mod : items) {
        mod->onInit();
    }
    Eventing::get().listen<KeyUpdateEvent>(this, (EventListenerFunc)&ModuleManager::onKey);
}

ModuleManager::~ModuleManager() {
    shutdownForEject();
}

void ModuleManager::shutdownForEject() {
    if (shuttingDown.exchange(true, std::memory_order_acq_rel)) return;

    for (auto& mod : items) {
        if (mod->isEnabled()) mod->setEnabled(false);
    }
}

void ModuleManager::onKey(Event& evGeneric) {
    auto& ev = reinterpret_cast<KeyUpdateEvent&>(evGeneric);
    for (auto& mod : items) {
        if (ev.inUI()) return;
        if (mod->getKeybind() == ev.getKey()) {
            if (mod->shouldHoldToToggle()) {
                if (!mod->isEnabled() && ev.isDown()) {
                    mod->setEnabled(true);
                } else if (mod->isEnabled() && !ev.isDown()) {
                    mod->setEnabled(false);
                }
                continue;
            } else if (ev.isDown()) {
                mod->setEnabled(!mod->isEnabled());
            }
        }
    }
}
