#include "pch.h"
#include "HitIndicator.h"
#include <util/DrawUtil3D.h>

namespace {
    // 12-edge wireframe only (no quad faces) — a thinner "outline" look
    // compared to Hitboxes-style drawBox, which also fills top/bottom faces.
    void drawWireframe(MCDrawUtil3D& dc, AABB const& bb, d2d::Color const& color) {
        Vec3 const& lo = bb.lower;
        Vec3 const& hi = bb.higher;

        Vec3 corners[8] = {
            { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { lo.x, lo.y, hi.z },
            { lo.x, hi.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z },
        };

        // bottom face
        dc.drawLine(corners[0], corners[1], color);
        dc.drawLine(corners[1], corners[2], color);
        dc.drawLine(corners[2], corners[3], color);
        dc.drawLine(corners[3], corners[0], color);
        // top face
        dc.drawLine(corners[4], corners[5], color);
        dc.drawLine(corners[5], corners[6], color);
        dc.drawLine(corners[6], corners[7], color);
        dc.drawLine(corners[7], corners[4], color);
        // verticals
        dc.drawLine(corners[0], corners[4], color);
        dc.drawLine(corners[1], corners[5], color);
        dc.drawLine(corners[2], corners[6], color);
        dc.drawLine(corners[3], corners[7], color);
    }
}

HitIndicator::HitIndicator()
    : Module("HitIndicator", LocalizeString::get("client.module.hitIndicator.name"),
             LocalizeString::get("client.module.hitIndicator.desc"), GAME) {
    style.addEntry(EnumEntry(style_fullBox, LocalizeString::get("client.module.hitIndicator.styleFullBox.name"),
                             LocalizeString::get("client.module.hitIndicator.styleFullBox.name")));
    style.addEntry(EnumEntry(style_outline, LocalizeString::get("client.module.hitIndicator.styleOutline.name"),
                             LocalizeString::get("client.module.hitIndicator.styleOutline.name")));
    addEnumSetting("style", LocalizeString::get("client.module.hitIndicator.style.name"),
                   LocalizeString::get("client.module.hitIndicator.style.desc"), style);

    addSetting("inReachColor", LocalizeString::get("client.module.hitIndicator.inReachColor.name"),
               LocalizeString::get("client.module.hitIndicator.inReachColor.desc"), inReachColor);
    addSetting("outOfReachColor", LocalizeString::get("client.module.hitIndicator.outOfReachColor.name"),
               LocalizeString::get("client.module.hitIndicator.outOfReachColor.desc"), outOfReachColor);
    addSliderSetting("reach", LocalizeString::get("client.module.hitIndicator.reach.name"),
                     LocalizeString::get("client.module.hitIndicator.reach.desc"), reach, FloatValue(1.f),
                     FloatValue(6.f), FloatValue(0.05f));
    addSetting("transparent", LocalizeString::get("client.module.hitIndicator.transparent.name"),
               LocalizeString::get("client.module.hitIndicator.transparent.desc"), transparent,
               Setting::Condition("style", Setting::Condition::EQUALS, { style_fullBox }));

    Eventing::get().listen<RenderLevelEvent, &HitIndicator::onRenderLevel>(this);
}

void HitIndicator::onRenderLevel(RenderLevelEvent& event) {
    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    auto level = SDK::ClientInstance::get()->minecraft->getLevel();
    if (level == nullptr || lp == nullptr) return;

    bool useFullBox = style.getSelectedKey() == style_fullBox;

    auto material = (useFullBox && std::get<BoolValue>(transparent)) ? SDK::MaterialPtr::getSelectionOverlayMaterial()
                                                                     : SDK::MaterialPtr::getSelectionBoxMaterial();
    auto dc = MCDrawUtil3D(SDK::ClientInstance::get()->levelRenderer, SDK::ScreenContext::instance3d, material);

    float alpha = SDK::ClientInstance::get()->minecraft->timer->alpha;
    float maxReach = std::get<FloatValue>(reach);

    // Eye position, interpolated the same way entity positions are, so the
    // distance check matches what's actually rendered this frame.
    Vec3 eyePos = {
        std::lerp(lp->getPosOld().x, lp->getPos().x, alpha),
        std::lerp(lp->getPosOld().y, lp->getPos().y, alpha) + (lp->getPos().y - lp->getBoundingBox().lower.y),
        std::lerp(lp->getPosOld().z, lp->getPos().z, alpha)
    };

    float calcYaw = (lp->getRot().y + 90.f) * (pi_f / 180.f);
    float calcPitch = lp->getRot().x * -(pi_f / 180.f);
    Vec3 viewDir = {
        cos(calcYaw) * cos(calcPitch),
        sin(calcPitch),
        sin(calcYaw) * cos(calcPitch)
    };

    for (const auto entt : level->getRuntimeActorList()) {
        if (entt->isInvisible()) continue;
        if (entt == lp) continue;
        if (entt->getEntityTypeID() == 64) continue;

        Vec3 newPos = {
            std::lerp(entt->getPosOld().x, entt->getPos().x, alpha),
            std::lerp(entt->getPosOld().y, entt->getPos().y, alpha),
            std::lerp(entt->getPosOld().z, entt->getPos().z, alpha)
        };

        AABB bb = entt->getBoundingBox();
        float eyeOffset = entt->getPos().y - bb.lower.y;
        Vec3 rebasePos =
            newPos.operator-({ 0.f, eyeOffset, 0.f }).operator+({ 0.f, (bb.higher.y - bb.lower.y) / 2.f, 0.f });
        bb.rebase(rebasePos);

        // Actual Minecraft-accurate reach: distance from eye to the nearest
        // point on the target's box, not center-to-center or eye-to-eye.
        Vec3 closest = bb.closestPoint(eyePos);
        float dist = eyePos.distance(closest);

        bool inReach = dist <= maxReach && bb.intersectsRay(eyePos, viewDir, maxReach).has_value();

        // Not actually hittable right now (still in hit-invulnerability
        // window from a prior hit) — never show as "in reach" even if the
        // crosshair is on the hitbox and distance is fine.
        auto health = entt->getHealth();
        bool isHittable = entt->invulnerableTime <= 0 && (!health.has_value() || health.value() > 0.f);
        inReach = inReach && isHittable;

        auto boxCol = (inReach ? std::get<ColorValue>(inReachColor) : std::get<ColorValue>(outOfReachColor))
                          .getMainColor();

        if (useFullBox) {
            dc.drawBox(bb, boxCol);
        } else {
            drawWireframe(dc, bb, boxCol);
        }
        dc.flush();
    }
}
