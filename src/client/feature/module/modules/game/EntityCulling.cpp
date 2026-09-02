#include "pch.h"
#include "EntityCulling.h"
#include <mc/common/client/game/ClientInstance.h>
#include <mc/common/client/player/LocalPlayer.h>
#include <mc/common/world/actor/Actor.h>

EntityCulling::EntityCulling()
    : Module("EntityCulling", LocalizeString::get("client.module.entityCulling.name"),
             LocalizeString::get("client.module.entityCulling.desc"), GAME, nokeybind) {
    addSetting("useDistance", LocalizeString::get("client.module.entityCulling.useDistance.name"),
               LocalizeString::get("client.module.entityCulling.useDistance.desc"), useDistance);
    addSliderSetting("maxDistance", LocalizeString::get("client.module.entityCulling.maxDistance.name"),
                     LocalizeString::get("client.module.entityCulling.maxDistance.desc"), maxDistance,
                     FloatValue(8.f), FloatValue(256.f), FloatValue(1.f), "useDistance"_istrue);
    addSetting("useFOV", LocalizeString::get("client.module.entityCulling.useFOV.name"),
               LocalizeString::get("client.module.entityCulling.useFOV.desc"), useFOV);
    addSliderSetting("fov", LocalizeString::get("client.module.entityCulling.fov.name"),
                     LocalizeString::get("client.module.entityCulling.fov.desc"), fov, FloatValue(30.f),
                     FloatValue(180.f), FloatValue(1.f), "useFOV"_istrue);

    listen<RenderEntityEvent>((EventListenerFunc)&EntityCulling::onRender);
}

void EntityCulling::onRender(Event& evG) {
    auto& ev = reinterpret_cast<RenderEntityEvent&>(evG);
    auto entity = ev.getEntity();
    if (!entity) return;

    // never cull ourselves
    auto localPlayer = SDK::ClientInstance::get()->getLocalPlayer();
    if (!localPlayer || entity == reinterpret_cast<SDK::Actor*>(localPlayer)) return;

    Vec3 const& camPos = ev.getCameraPos();
    Vec3 entityPos = entity->getPos();
    Vec3 toEntity = entityPos - camPos;
    float dist = toEntity.magnitude();

    if (std::get<BoolValue>(useDistance) && dist > std::get<FloatValue>(maxDistance)) {
        ev.setCancelled(true);
        return;
    }

    if (std::get<BoolValue>(useFOV) && dist > 0.0001f) {
        Vec2 rot = localPlayer->getRot();
        float yawRad = LatiteMath::deg2rad(rot.y);
        // Bedrock forward vector on the XZ plane from yaw
        Vec3 forward = { -std::sin(yawRad), 0.f, std::cos(yawRad) };

        Vec3 dir = toEntity.normalized();
        // flatten to XZ so pitch doesn't affect horizontal FOV culling
        Vec3 dirFlat = { dir.x, 0.f, dir.z };
        float dirFlatLen = dirFlat.magnitude();
        if (dirFlatLen > 0.0001f) {
            dirFlat.x /= dirFlatLen;
            dirFlat.z /= dirFlatLen;

            float dot = forward.x * dirFlat.x + forward.z * dirFlat.z;
            dot = std::clamp(dot, -1.f, 1.f);
            float angle = LatiteMath::abs(std::acos(dot)) * (180.f / pi_f);

            if (angle > std::get<FloatValue>(fov) * 0.5f) {
                ev.setCancelled(true);
                return;
            }
        }
    }
}
