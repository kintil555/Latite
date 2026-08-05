#include "pch.h"
#include "PlayerOverlay.h"
#include "client/event/events/RenderLayerEvent.h"
#include "mc/common/client/game/FontRepository.h"
#include "mc/common/client/gui/controls/UIControl.h"
#include "mc/common/client/gui/controls/VisualTree.h"
#include "mc/common/world/actor/player/Player.h"
#include "mc/common/world/level/Level.h"
#include "util/WorldToScreen.h"

PlayerOverlay::PlayerOverlay()
    : Module("PlayerOverlay", LocalizeString::get("client.module.playerOverlay.name"),
             LocalizeString::get("client.module.playerOverlay.desc"), GAME) {
    addSetting("panelColor", LocalizeString::get("client.module.playerOverlay.panelColor.name"),
               LocalizeString::get("client.module.playerOverlay.panelColor.desc"), panelColor);
    addSetting("nameColor", LocalizeString::get("client.module.playerOverlay.nameColor.name"),
               LocalizeString::get("client.module.playerOverlay.nameColor.desc"), nameColor);
    addSetting("liveColor", LocalizeString::get("client.module.playerOverlay.liveColor.name"),
               LocalizeString::get("client.module.playerOverlay.liveColor.desc"), liveColor);
    addSetting("lastSeenColor", LocalizeString::get("client.module.playerOverlay.lastSeenColor.name"),
               LocalizeString::get("client.module.playerOverlay.lastSeenColor.desc"), lastSeenColor);
    addSliderSetting("textSize", LocalizeString::get("client.module.playerOverlay.textSize.name"),
                     LocalizeString::get("client.module.playerOverlay.textSize.desc"), textSize, FloatValue(8.f),
                     FloatValue(32.f), FloatValue(1.f));
    addSliderSetting("maxDistance", LocalizeString::get("client.module.playerOverlay.maxDistance.name"),
                     LocalizeString::get("client.module.playerOverlay.maxDistance.desc"), maxDistance,
                     FloatValue(8.f), FloatValue(512.f), FloatValue(8.f));
    addActionSetting("clearCache", LocalizeString::get("client.module.playerOverlay.clearCache.name"),
                     LocalizeString::get("client.module.playerOverlay.clearCache.desc"), [this] {
                         cache.clear();
                     });

    listen<RenderLayerEvent>(&PlayerOverlay::onRenderLayer);
}

void PlayerOverlay::onRenderLayer(Event& genericEvent) {
    RenderLayerEvent& event = reinterpret_cast<RenderLayerEvent&>(genericEvent);
    SDK::ScreenView* screenView = event.getScreenView();
    if (!screenView || !screenView->visualTree || !screenView->visualTree->rootControl) return;
    if (screenView->visualTree->rootControl->name != "hud_screen") return;

    SDK::ClientInstance* clientInstance = SDK::ClientInstance::get();
    if (!clientInstance || !clientInstance->minecraft || !clientInstance->minecraftGame) return;

    SDK::Actor* lp = clientInstance->getLocalPlayer();
    if (!lp) return;

    SDK::Level* level = clientInstance->minecraft->getLevel();
    if (!level) return;

    MCDrawUtil dc { event.getUIRenderContext(),
                    clientInstance->minecraftGame->getFontRepository()->getMinecraftFont() };

    float fontSize = std::get<FloatValue>(textSize);
    float maxDist = std::get<FloatValue>(maxDistance);
    auto panelCol = std::get<ColorValue>(panelColor).getMainColor();
    auto nameCol = std::get<ColorValue>(nameColor).getMainColor();
    auto liveCol = std::get<ColorValue>(liveColor).getMainColor();
    auto lastSeenCol = std::get<ColorValue>(lastSeenColor).getMainColor();

    for (const auto entt : level->getRuntimeActorList()) {
        if (!entt->isPlayer() || entt == lp) continue;

        uint64_t runtimeID = entt->getRuntimeID();
        if (entt->isInvisible()) continue; // covers only invisible-but-still-loaded case

        auto* player = static_cast<SDK::Player*>(entt);
        std::string const& utf8Name = player->playerName;
        std::wstring wideName(utf8Name.begin(), utf8Name.end());

        Vec3 anchorPos = entt->getPos();
        anchorPos.y += 0.3f; // sit just above where the vanilla nametag would be

        cache.insert_or_assign(runtimeID, CachedPlayer { wideName, anchorPos, true });
    }

    // Draw from the cache, not from the actor list: a player who fully disappears (out of
    // simulation range, spectator, disconnected-but-not-cleared, etc.) is no longer present in
    // getRuntimeActorList() at all, so relying on that loop to draw the frozen panel would mean
    // it never draws once the actor is gone. The cache is the source of truth for what to render.
    for (auto& [runtimeID, cached] : cache) {
        if (cached.anchorPos.distance(lp->getPos()) > maxDist) continue;

        auto screenPos = WorldToScreen::convert(cached.anchorPos);
        if (!screenPos) continue;

        std::wstring statusLine = cached.wasLiveThisFrame
                                       ? LocalizeString::get("client.module.playerOverlay.status.live")
                                       : LocalizeString::get("client.module.playerOverlay.status.lastSeen");
        auto statusCol = cached.wasLiveThisFrame ? liveCol : lastSeenCol;

        float panelWidth = 140.f;
        float lineHeight = fontSize + 4.f;
        float panelHeight = lineHeight * 2.f + 6.f;
        d2d::Rect panelRect { screenPos->x - panelWidth / 2.f, screenPos->y - panelHeight, screenPos->x + panelWidth / 2.f,
                              screenPos->y };

        dc.fillRoundedRectangle(panelRect, panelCol, 3.f);
        dc.drawText({ panelRect.left + 6.f, panelRect.top + 3.f, panelRect.right - 6.f, panelRect.top + 3.f + lineHeight },
                    cached.name, nameCol, Renderer::FontSelection::PrimarySemilight, fontSize,
                    DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        dc.drawText({ panelRect.left + 6.f, panelRect.top + 3.f + lineHeight, panelRect.right - 6.f, panelRect.bottom - 3.f },
                    statusLine, statusCol, Renderer::FontSelection::PrimaryRegular, fontSize * 0.85f,
                    DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        cached.wasLiveThisFrame = false; // reset; only re-set to true next frame if actor is live again
    }
}
