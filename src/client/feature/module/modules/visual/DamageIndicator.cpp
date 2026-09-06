#include "pch.h"
#include "DamageIndicator.h"
#include "client/event/events/RenderLayerEvent.h"
#include "mc/common/network/packet/ActorEventPacket.h"
#include "mc/common/client/game/FontRepository.h"
#include "mc/common/client/gui/controls/UIControl.h"
#include "mc/common/client/gui/controls/VisualTree.h"
#include "mc/common/world/level/Level.h"
#include "util/WorldToScreen.h"

DamageIndicator::DamageIndicator()
    : Module("DamageIndicator", L"Damage Indicator",
             L"Shows a floating number over an entity's head for how much health your hit took off — a battle-royale style damage popup, not a chat message.",
             GAME) {

    addSetting("textColor", L"Text Color",
               L"Color of a normal-hit damage number.", textColor);
    addSetting("critColor", L"Crit Color",
               L"Color used when a hit's damage meets or exceeds the crit threshold.", critColor);
    addSliderSetting("critThreshold", L"Crit Threshold",
                     L"Damage amount at or above which the crit color is used instead.",
                     critThreshold, FloatValue(1.f), FloatValue(20.f), FloatValue(0.5f));
    addSliderSetting("textSize", L"Text Size",
                     L"Size of the damage number text.", textSize, FloatValue(8.f), FloatValue(32.f), FloatValue(1.f));
    addSliderSetting("riseSpeed", L"Rise Speed",
                     L"How fast the number floats upward, in blocks per second.",
                     riseSpeed, FloatValue(0.1f), FloatValue(2.f), FloatValue(0.05f));
    addSliderSetting("lifetime", L"Lifetime",
                     L"How long a damage number stays on screen before it fully fades out (seconds).",
                     lifetime, FloatValue(0.5f), FloatValue(3.f), FloatValue(0.1f));

    listen<AttackEvent>((EventListenerFunc)&DamageIndicator::onAttack);
    listen<PacketReceiveEvent>((EventListenerFunc)&DamageIndicator::onPacketReceive);
    listen<RenderLayerEvent>((EventListenerFunc)&DamageIndicator::onRenderLayer);
}

void DamageIndicator::onAttack(Event& evGeneric) {
    auto& ev  = reinterpret_cast<AttackEvent&>(evGeneric);
    auto  ent = ev.getActor();
    if (!ent) return;

    // Snapshot the target's health right before our hit lands, so once the
    // server confirms the hit (HURT_ANIMATION below) we can diff against
    // this to get the actual damage dealt.
    auto healthOpt = ent->getHealth();
    if (healthOpt.has_value()) {
        m_lastHealth[ent->getRuntimeID()] = healthOpt.value();
    }

    m_pendingRuntimeId = ent->getRuntimeID();
    m_hasPendingHit    = true;
}

void DamageIndicator::onPacketReceive(Event& evGeneric) {
    auto& ev  = reinterpret_cast<PacketReceiveEvent&>(evGeneric);
    auto  pkt = ev.getPacket();
    if (pkt->getID() != SDK::PacketID::ACTOR_EVENT || !m_hasPendingHit) return;

    auto actorEvent = static_cast<SDK::ActorEventPacket*>(pkt);
    if (actorEvent->eventID != SDK::ActorEventID::HURT_ANIMATION ||
        actorEvent->runtimeID != m_pendingRuntimeId) {
        return;
    }

    m_hasPendingHit = false;

    auto it = m_lastHealth.find(m_pendingRuntimeId);
    if (it == m_lastHealth.end()) return; // no pre-hit snapshot, nothing to diff

    auto level = SDK::ClientInstance::get()->minecraft->getLevel();
    if (!level) return;

    // Find the live actor to read post-hit health and current position.
    for (const auto entt : level->getRuntimeActorList()) {
        if (entt->getRuntimeID() != m_pendingRuntimeId) continue;

        auto healthOpt = entt->getHealth();
        if (!healthOpt.has_value()) break;

        float dealt = it->second - healthOpt.value();
        it->second  = healthOpt.value(); // update snapshot for the next hit

        if (dealt <= 0.f) break; // healed or no actual drop; nothing to show

        Vec3 anchor = entt->getPos();
        auto bb     = entt->getBoundingBox();
        anchor.y    = bb.higher.y + 0.3f; // just above the head, like a nametag

        m_popups.push_back(Popup { anchor, dealt, std::chrono::steady_clock::now() });
        break;
    }
}

void DamageIndicator::onRenderLayer(Event& evGeneric) {
    auto& ev          = reinterpret_cast<RenderLayerEvent&>(evGeneric);
    auto  screenView  = ev.getScreenView();
    if (!screenView || !screenView->visualTree || !screenView->visualTree->rootControl) return;
    if (screenView->visualTree->rootControl->name != "hud_screen") return;

    if (m_popups.empty()) return;

    SDK::ClientInstance* clientInstance = SDK::ClientInstance::get();
    if (!clientInstance || !clientInstance->minecraftGame) return;

    MCDrawUtil dc { ev.getUIRenderContext(), clientInstance->minecraftGame->getFontRepository()->getMinecraftFont() };

    float fontSize   = std::get<FloatValue>(textSize);
    float rise       = std::get<FloatValue>(riseSpeed);
    float lifeSecs   = std::get<FloatValue>(lifetime);
    float critAt     = std::get<FloatValue>(critThreshold);
    auto  normalCol  = std::get<ColorValue>(textColor).getMainColor();
    auto  critCol    = std::get<ColorValue>(critColor).getMainColor();

    auto now = std::chrono::steady_clock::now();

    // Iterate + prune expired popups in one pass.
    for (auto it = m_popups.begin(); it != m_popups.end();) {
        float age = std::chrono::duration<float>(now - it->spawnedAt).count();
        if (age >= lifeSecs) {
            it = m_popups.erase(it);
            continue;
        }

        float t = age / lifeSecs; // 0 -> 1 over its lifetime

        Vec3 pos = it->anchorPos;
        pos.y += rise * age;

        auto screenPos = WorldToScreen::convert(pos);
        if (!screenPos) {
            ++it;
            continue;
        }

        bool isCrit = it->damage >= critAt;
        auto baseCol = isCrit ? critCol : normalCol;

        // Fade alpha out over the last half of the lifetime so numbers
        // don't just pop out of existence.
        float fadeAlpha = t < 0.5f ? 1.f : std::clamp(1.f - (t - 0.5f) / 0.5f, 0.f, 1.f);
        d2d::Color col { baseCol.r, baseCol.g, baseCol.b, baseCol.a * fadeAlpha };

        wchar_t buf[16];
        swprintf_s(buf, L"-%.1f", it->damage);

        float halfW = 40.f;
        float halfH = fontSize;
        d2d::Rect textRect { screenPos->x - halfW, screenPos->y - halfH, screenPos->x + halfW, screenPos->y + halfH };

        dc.drawText(textRect, buf, col, Renderer::FontSelection::PrimaryRegular, isCrit ? fontSize * 1.2f : fontSize,
                    DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        ++it;
    }
}
