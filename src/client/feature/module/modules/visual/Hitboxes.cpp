#include "pch.h"
#include "Hitboxes.h"
#include <util/DrawUtil3D.h>
#include <unordered_set>

Hitboxes::Hitboxes()
    : Module("Hitboxes", LocalizeString::get("client.module.hitboxes.name"),
             LocalizeString::get("client.module.hitboxes.desc"), GAME) {
    addSetting("transparent", LocalizeString::get("client.module.hitboxes.transparent.name"),
               LocalizeString::get("client.module.hitboxes.transparent.desc"), transparent);
    addSetting("boxColor", LocalizeString::get("client.module.hitboxes.boxColor.name"),
               LocalizeString::get("client.module.hitboxes.boxColor.desc"), boxColor);
    addSetting("showEyeLine", LocalizeString::get("client.module.hitboxes.showEyeLine.name"),
               LocalizeString::get("client.module.hitboxes.showEyeLine.desc"), this->showEyeLine);
    addSetting("eyeLine", LocalizeString::get("client.module.hitboxes.eyeLine.name"),
               LocalizeString::get("client.module.hitboxes.eyeLine.desc"), this->eyeColor, "showEyeLine"_istrue);
    addSetting("showLookingAt", LocalizeString::get("client.module.hitboxes.showLookingAt.name"),
               LocalizeString::get("client.module.hitboxes.showLookingAt.desc"), this->showLine);
    addSetting("lookingAt", LocalizeString::get("client.module.hitboxes.lookingAt.name"),
               LocalizeString::get("client.module.hitboxes.lookingAt.desc"), this->lineColor, "showLookingAt"_istrue);
    addSetting("items", LocalizeString::get("client.module.hitboxes.items.name"),
               LocalizeString::get("client.module.hitboxes.items.desc"), items);
    addSetting("ghostHitbox", LocalizeString::get("client.module.hitboxes.ghostHitbox.name"),
               LocalizeString::get("client.module.hitboxes.ghostHitbox.desc"), ghostHitbox);
    addSetting("ghostColor", LocalizeString::get("client.module.hitboxes.ghostColor.name"),
               LocalizeString::get("client.module.hitboxes.ghostColor.desc"), ghostColor, "ghostHitbox"_istrue);

    Eventing::get().listen<RenderLevelEvent, &Hitboxes::onRenderLevel>(this);
}

void Hitboxes::onRenderLevel(RenderLevelEvent& event) {
    auto dc = MCDrawUtil3D(SDK::ClientInstance::get()->levelRenderer, SDK::ScreenContext::instance3d,
                            SDK::MaterialPtr::getSelectionOverlayMaterial());

    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    auto level = SDK::ClientInstance::get()->minecraft->getLevel();
    auto* connectionInfo = SDK::RemoteConnectorComposite::getConnectionInfo();

    if (level == nullptr) return;

    std::unordered_set<uint64_t> seenThisFrame;

    // level can pass the null-check above and still be mid-teardown (e.g. a
    // leave-game/disconnect racing this render frame), which crashes inside
    // the virtual call in getRuntimeActorList (0xC0000005). Guard the call
    // itself rather than trusting the earlier null-check alone.
    std::vector<SDK::Actor*> actors;
    __try {
        actors = level->getRuntimeActorList();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    for (const auto entt : actors) {
        if (entt == lp) continue;
        if (!std::get<BoolValue>(items) && entt->getEntityTypeID() == 64) continue;

        auto& activeDc = dc;
        uint64_t runtimeID = entt->getRuntimeID();
        if (std::get<BoolValue>(ghostHitbox)) seenThisFrame.insert(runtimeID);

        if (entt->isInvisible()) {
            if (!std::get<BoolValue>(ghostHitbox)) continue;

            auto cached = ghostBoxes.find(runtimeID);
            if (cached == ghostBoxes.end()) continue;

            auto ghostCol = std::get<ColorValue>(ghostColor).getMainColor();
            activeDc.drawBox(cached->second, ghostCol);
            activeDc.flush();
            continue;
        }

        Vec3 newPos = {
            std::lerp(entt->getPosOld().x, entt->getPos().x, SDK::ClientInstance::get()->minecraft->timer->alpha),
            std::lerp(entt->getPosOld().y, entt->getPos().y, SDK::ClientInstance::get()->minecraft->timer->alpha),
            std::lerp(entt->getPosOld().z, entt->getPos().z, SDK::ClientInstance::get()->minecraft->timer->alpha)
        };

        AABB bb = entt->getBoundingBox();
        float eyeOffset = entt->getPos().y - bb.lower.y;
        Vec3 rebasePos =
            newPos.operator-({ 0.f, eyeOffset, 0.f }).operator+({ 0.f, (bb.higher.y - bb.lower.y) / 2.f, 0.f });
        bb.rebase(rebasePos);

        // Cache a block-snapped 1x1x1 box (flush with the block grid) as the ghost hitbox to show
        // if this entity goes invisible while standing still.
        if (std::get<BoolValue>(ghostHitbox)) {
            Vec3 snappedLower = { std::floor(bb.lower.x), std::floor(bb.lower.y), std::floor(bb.lower.z) };
            ghostBoxes.insert_or_assign(runtimeID, AABB(snappedLower, snappedLower + Vec3(1.f, 1.f, 1.f)));
        }

        bool willShowLine = std::get<BoolValue>(showLine) &&
                           (!entt->isPlayer() || (!connectionInfo || connectionInfo->hostIpAddress.empty()) ||
                            entt == lp);

        auto boxCol = std::get<ColorValue>(boxColor).getMainColor();
        auto lineCol = std::get<ColorValue>(lineColor).getMainColor();
        auto eyeCol = std::get<ColorValue>(eyeColor).getMainColor();

        activeDc.drawBox(bb, boxCol);
        float eyePos = newPos.y;
        float eyeLine = eyePos;
        bool customEyeLine = false;

        if (customEyeLine = LatiteMath::aequals(bb.lower.y, eyePos)) {
            eyeLine = bb.lower.y + (bb.higher.y - bb.lower.y) * 0.85f;
        }

        if (std::get<BoolValue>(showEyeLine)) {
            activeDc.drawQuad(Vec3(bb.lower.x, eyeLine, bb.lower.z), Vec3(bb.higher.x, eyeLine, bb.lower.z),
                        Vec3(bb.higher.x, eyeLine, bb.higher.z), Vec3(bb.lower.x, eyeLine, bb.higher.z), eyeCol);
        }

        if (willShowLine) {
            float calcYaw = (entt->getRot().y + 90) * (pi_f / 180);
            float calcPitch = entt->getRot().x * -(pi_f / 180);
            float mod = 1.f;

            Vec3 offset;
            offset.x = cos(calcYaw) * cos(calcPitch) * mod;
            offset.y = sin(calcPitch) * mod;
            offset.z = sin(calcYaw) * cos(calcPitch) * mod;

            Vec3 begin = newPos;
            begin.y = customEyeLine ? eyeLine : eyePos;
            Vec3 end = begin + offset;

            BlockPos bp { static_cast<int>((end.x)), static_cast<int>((end.y)), static_cast<int>((end.z)) };

            activeDc.drawLine(begin, end, lineCol);
        }
        activeDc.flush();
    }

    if (std::get<BoolValue>(ghostHitbox)) {
        for (auto it = ghostBoxes.begin(); it != ghostBoxes.end();) {
            it = seenThisFrame.contains(it->first) ? std::next(it) : ghostBoxes.erase(it);
        }
    }
}
