#include "pch.h"
#include "PlayerHooks.h"
#include "client/event/events/PlayerHurtScreenCloseEvent.h"

static std::shared_ptr<Hook> ActorAttackHook;
static std::shared_ptr<Hook> UISceneCloseOnPlayerHurtHook;

void* PlayerHooks::hkActorAttack(SDK::Actor* obj, void* ret, SDK::Actor* target, void* cause, void* a4) {
    if (obj == SDK::ClientInstance::get()->getLocalPlayer()) {
        AttackEvent ev { target };
        Eventing::get().dispatch(ev);
    }

    return ActorAttackHook->oFunc<decltype(&hkActorAttack)>()(obj, ret, target, cause, a4);
}

// UIScene::closeOnPlayerHurt(this) -> bool. Returning the original result
// tells the caller whether to close the current screen; cancelling the
// event (AntiScreenClose does this while enabled) short-circuits straight
// to "false" without ever calling the original, so no screen ever gets
// force-closed by taking damage.
bool PlayerHooks::hkUIScene_closeOnPlayerHurt(void* uiScene) {
    PlayerHurtScreenCloseEvent ev {};
    if (Eventing::get().dispatch(ev)) {
        return false;
    }

    return UISceneCloseOnPlayerHurtHook->oFunc<decltype(&hkUIScene_closeOnPlayerHurt)>()(uiScene);
}

PlayerHooks::PlayerHooks() {
    ActorAttackHook = this->addHook(Signatures::Actor_attack.result, &hkActorAttack, "Actor::attack");
    UISceneCloseOnPlayerHurtHook = this->addHook(Signatures::UIScene_closeOnPlayerHurt.result,
                                                 &hkUIScene_closeOnPlayerHurt, "UIScene::closeOnPlayerHurt");
}
