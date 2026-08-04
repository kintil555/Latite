#include "pch.h"
#include "NoticeScreen.h"

#include "client/event/Eventing.h"
#include "client/event/events/ClickEvent.h"
#include "client/event/events/KeyUpdateEvent.h"
#include "client/event/events/RenderOverlayEvent.h"
#include "client/Latite.h"
#include "client/localization/LocalizeString.h"
#include "util/DrawContext.h"
#include "util/Util.h"

#include <algorithm>

NoticeScreen::NoticeScreen() {
    // Priority 11 (higher than Notifications' priority 10) so the disclaimer
    // panel + background blur are drawn FIRST, and toast notifications are
    // drawn on top of them afterward. This keeps notifications fully visible
    // and unblurred even when they appear at the same time as this screen,
    // instead of being drawn underneath the blur.
    Eventing::get().listen<RenderOverlayEvent>(this, (EventListenerFunc)&NoticeScreen::onRender, 11, true);
    Eventing::get().listen<ClickEvent>(this, (EventListenerFunc)&NoticeScreen::onClick, 4);
    Eventing::get().listen<KeyUpdateEvent>(this, (EventListenerFunc)&NoticeScreen::onKey);
}

void NoticeScreen::onEnable(bool ignoreAnimations) {
    anim = ignoreAnimations ? 1.f : 0.f;
    scrollOffset = 0.f;
    resetInputState();
}

void NoticeScreen::onDisable() {
    resetInputState();
}

void NoticeScreen::onRender(Event&) {
    if (!isActive()) return;

    D2DUtil dc;
    D2D1_SIZE_F screenSize = Latite::getRenderer().getScreenSize();
    Vec2 cursorPosition = SDK::ClientInstance::get()->cursorPos;
    d2d::Color accent = d2d::Color(Latite::get().getAccentColor().getMainColor());

    float alpha = Latite::getRenderer().getDeltaTime() / 8.f;
    anim = std::lerp(anim, 1.f, alpha);

    if (Latite::get().getMenuBlur()) dc.drawGaussianBlur(Latite::get().getMenuBlur().value() * anim);

    float scale = std::clamp(screenSize.width / 1920.f, 0.72f, 1.1f);
    float panelWidth = std::min(screenSize.width * 0.62f, 640.f * scale);
    float panelHeight = std::min(screenSize.height * 0.56f, 380.f * scale);
    panelRect = { (screenSize.width - panelWidth) * 0.5f, (screenSize.height - panelHeight) * 0.5f,
                  (screenSize.width + panelWidth) * 0.5f, (screenSize.height + panelHeight) * 0.5f };

    float padding = 28.f * scale;
    float radius = 19.f * scale;
    d2d::Color panelColor = d2d::Color::RGB(0x07, 0x07, 0x07).asAlpha(0.85f * anim);
    d2d::Color outlineColor = d2d::Color::RGB(0x00, 0x00, 0x00).asAlpha(0.3f * anim);

    dc.fillRoundedRectangle(panelRect, panelColor, radius);
    dc.drawRoundedRectangle(panelRect, outlineColor, radius, 4.f * scale, DrawUtil::OutlinePosition::Outside);

    // Title
    std::wstring title = LocalizeString::get("client.screen.notice.title");
    d2d::Rect titleRect { panelRect.left + padding, panelRect.top + 24.f * scale, panelRect.right - padding,
                          panelRect.top + 60.f * scale };
    dc.drawText(titleRect, title, d2d::Colors::WHITE.asAlpha(anim), Renderer::FontSelection::PrimaryLight,
                26.f * scale, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);

    // Body text: states this is a modified/independent build and its license. Scrollable since it no
    // longer fits a fixed panel height.
    bodyRect = { panelRect.left + padding, titleRect.bottom + 14.f * scale, panelRect.right - padding,
                 panelRect.bottom - 92.f * scale };
    std::wstring body = LocalizeString::get("client.screen.notice.body");
    float bodyFontSize = 16.5f * scale;

    float textHeight = dc.getMeasuredTextHeight(bodyRect, body, Renderer::FontSelection::PrimaryRegular, bodyFontSize);
    maxScrollOffset = std::max(0.f, textHeight - bodyRect.getHeight());
    scrollOffset = std::clamp(scrollOffset, 0.f, maxScrollOffset);

    d2d::Rect scrolledBodyRect = bodyRect;
    scrolledBodyRect.top -= scrollOffset;
    scrolledBodyRect.bottom -= scrollOffset;

    dc.drawTextClipped(bodyRect, scrolledBodyRect, body, d2d::Color::RGB(0xD6, 0xD6, 0xD6).asAlpha(0.9f * anim),
                       Renderer::FontSelection::PrimaryRegular, bodyFontSize, DWRITE_TEXT_ALIGNMENT_LEADING,
                       DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    // Simple scrollbar indicator when content overflows.
    if (maxScrollOffset > 0.f) {
        float trackHeight = bodyRect.getHeight();
        float thumbHeight = std::max(24.f * scale, trackHeight * (bodyRect.getHeight() / textHeight));
        float thumbTop = bodyRect.top + (scrollOffset / maxScrollOffset) * (trackHeight - thumbHeight);
        d2d::Rect scrollTrack { bodyRect.right + 8.f * scale, bodyRect.top, bodyRect.right + 11.f * scale,
                                bodyRect.bottom };
        d2d::Rect scrollThumb { scrollTrack.left, thumbTop, scrollTrack.right, thumbTop + thumbHeight };
        dc.fillRoundedRectangle(scrollTrack, d2d::Color::RGB(0x2A, 0x2A, 0x2A).asAlpha(0.6f * anim), 2.f * scale);
        dc.fillRoundedRectangle(scrollThumb, accent.asAlpha(0.8f * anim), 2.f * scale);
    }

    // Hint text for the Enter shortcut
    std::wstring hint = LocalizeString::get("client.screen.notice.hint");
    d2d::Rect hintRect { panelRect.left + padding, panelRect.bottom - 78.f * scale,
                        panelRect.right - padding - 190.f * scale, panelRect.bottom - 24.f * scale };
    dc.drawText(hintRect, hint, d2d::Color::RGB(0xA0, 0xA0, 0xA0).asAlpha(0.8f * anim),
                Renderer::FontSelection::PrimaryRegular, 13.5f * scale, DWRITE_TEXT_ALIGNMENT_LEADING,
                DWRITE_PARAGRAPH_ALIGNMENT_FAR);

    // Close button
    closeButtonRect = { panelRect.right - padding - 170.f * scale, panelRect.bottom - 68.f * scale,
                        panelRect.right - padding, panelRect.bottom - 24.f * scale };

    bool buttonHovered = closeButtonRect.contains(cursorPosition);
    d2d::Color buttonColor = buttonHovered ? accent : d2d::Color::RGB(0x38, 0x38, 0x38).asAlpha(0.9f);
    buttonColor = buttonColor.asAlpha(buttonColor.a * anim);
    dc.fillRoundedRectangle(closeButtonRect, buttonColor, closeButtonRect.getHeight() * 0.23f);

    std::wstring closeText = LocalizeString::get("client.screen.notice.close");
    dc.drawText(closeButtonRect, closeText, d2d::Colors::WHITE.asAlpha(anim), Renderer::FontSelection::PrimaryRegular,
                15.f * scale, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

void NoticeScreen::onClick(Event& genericEvent) {
    if (!isActive()) return;

    ClickEvent& event = reinterpret_cast<ClickEvent&>(genericEvent);
    if (event.getClickType() != ClickEvent::ClickType::None) event.setCancelled(true);

    if (event.getClickType() == ClickEvent::ClickType::Wheel) {
        Vec2 cursorPosition = SDK::ClientInstance::get()->cursorPos;
        if (bodyRect.contains(cursorPosition) && maxScrollOffset > 0.f) {
            float scrollStep = 48.f;
            scrollOffset = std::clamp(scrollOffset - event.getWheelDelta() * scrollStep, 0.f, maxScrollOffset);
        }
        return;
    }

    if (event.getClickType() != ClickEvent::ClickType::Left || !event.isDown()) return;

    Vec2 cursorPosition = SDK::ClientInstance::get()->cursorPos;
    if (closeButtonRect.contains(cursorPosition)) {
        playClickSound();
        close();
    }
}

void NoticeScreen::onKey(Event& genericEvent) {
    if (!isActive()) return;

    auto& ev = reinterpret_cast<KeyUpdateEvent&>(genericEvent);
    if (ev.getKey() == VK_RETURN && ev.isDown()) {
        close();
    }
    ev.setCancelled();
}
