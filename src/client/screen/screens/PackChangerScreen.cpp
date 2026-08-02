#include "pch.h"
#include "PackChangerScreen.h"

#include "client/event/Eventing.h"
#include "client/event/events/ClickEvent.h"
#include "client/event/events/KeyUpdateEvent.h"
#include "client/event/events/RenderLayerEvent.h"
#include "client/Latite.h"
#include "util/DrawContext.h"
#include "util/Util.h"
#include "mc/common/client/game/ClientInstance.h"
#include "mc/common/resources/ResourcePackManager.h"

#include <fstream>
#include <algorithm>

// ============================================================================
// Helpers
// ============================================================================
namespace {

std::filesystem::path findComMojang() {
    wchar_t localApp[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
        std::filesystem::path pkgRoot = std::filesystem::path(localApp) / L"Packages";
        if (std::filesystem::exists(pkgRoot)) {
            for (auto const& e : std::filesystem::directory_iterator(pkgRoot)) {
                if (!e.is_directory()) continue;
                std::wstring n = e.path().filename().wstring();
                if (n.rfind(L"Microsoft.MinecraftUWP_", 0) == 0 ||
                    n.rfind(L"Microsoft.MinecraftWindowsBeta_", 0) == 0) {
                    auto c = e.path() / L"LocalState" / L"games" / L"com.mojang";
                    if (std::filesystem::exists(c)) return c;
                }
            }
        }
        wchar_t roaming[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"APPDATA", roaming, MAX_PATH) > 0) {
            for (auto sub : { L"MinecraftPE", L"Minecraft" }) {
                auto c = std::filesystem::path(roaming) / sub / L"games" / L"com.mojang";
                if (std::filesystem::exists(c)) return c;
            }
        }
    }
    return {};
}

} // namespace

// ============================================================================
// Constructor
// ============================================================================
PackChangerScreen::PackChangerScreen() {
    Eventing::get().listen<RenderLayerEvent>(this, (EventListenerFunc)&PackChangerScreen::onRender, 1, true);
    Eventing::get().listen<ClickEvent>(this, (EventListenerFunc)&PackChangerScreen::onClick, 4);
    Eventing::get().listen<KeyUpdateEvent>(this, (EventListenerFunc)&PackChangerScreen::onKey, 1);
}

void PackChangerScreen::onEnable(bool) {
    dirty       = true;
    scroll      = 0.f;
    lerpScroll  = 0.f;
    scrollMax   = 0.f;
    draggingBar = false;
    applyFeedbackUntil = std::nullopt;
}

void PackChangerScreen::onDisable() {
    mouseButtons       = {};
    activeMouseButtons = {};
    justClicked        = {};
    draggingBar        = false;
}

// ============================================================================
// File I/O
// ============================================================================
std::filesystem::path PackChangerScreen::getMinecraftResourcePacksPath() const {
    auto base = findComMojang();
    return base.empty() ? std::filesystem::path{} : base / L"resource_packs";
}

std::filesystem::path PackChangerScreen::getOptionsPath() const {
    auto base = findComMojang();
    return base.empty() ? std::filesystem::path{} : base / L"minecraftpe" / L"options.txt";
}

bool PackChangerScreen::isValidPack(std::filesystem::path const& p) const {
    return std::filesystem::is_directory(p) &&
           (std::filesystem::exists(p / L"manifest.json") ||
            std::filesystem::exists(p / L"pack_manifest.json"));
}

void PackChangerScreen::loadActivePacks() {
    activePacks.clear();
    auto opt = getOptionsPath();
    if (opt.empty() || !std::filesystem::exists(opt)) return;

    std::ifstream f(opt);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("global_resource_packs:", 0) != 0 &&
            line.rfind("texture_packs:", 0) != 0) continue;
        size_t pos = 0;
        while (true) {
            // cari backslash lalu nama pack sampai tanda kutip
            size_t bs = line.find('\\', pos);
            if (bs == std::string::npos) break;
            size_t ns = bs + 1;
            size_t ne = line.find_first_of("\"}", ns);
            if (ne == std::string::npos) break;
            std::string name = line.substr(ns, ne - ns);
            if (!name.empty()) activePacks.push_back(util::StrToWStr(name));
            pos = ne + 1;
        }
    }
}

void PackChangerScreen::scanPacks() {
    packs.clear();
    loadActivePacks();

    auto packsPath = getMinecraftResourcePacksPath();
    if (packsPath.empty() || !std::filesystem::exists(packsPath)) { dirty = false; return; }

    for (auto const& e : std::filesystem::directory_iterator(packsPath)) {
        if (!isValidPack(e.path())) continue;
        PackEntry pe;
        pe.path        = e.path();
        pe.displayName = e.path().filename().wstring();
        pe.active      = std::find(activePacks.begin(), activePacks.end(),
                                   pe.displayName) != activePacks.end();
        packs.push_back(std::move(pe));
    }
    std::ranges::sort(packs, {}, [](PackEntry const& e) { return e.displayName; });
    dirty = false;
}

void PackChangerScreen::saveActivePacks() {
    auto opt = getOptionsPath();
    if (opt.empty()) return;

    std::vector<std::string> lines;
    {
        std::ifstream f(opt);
        std::string l;
        while (std::getline(f, l)) lines.push_back(l);
    }

    std::string val = "global_resource_packs:[";
    bool first = true;
    for (auto const& p : packs) {
        if (!p.active) continue;
        if (!first) val += ',';
        val += "\"resource_packs\\\\" + util::WStrToStr(p.displayName) + "\"";
        first = false;
    }
    val += ']';

    bool found = false;
    for (auto& l : lines) {
        if (l.rfind("global_resource_packs:", 0) == 0) { l = val; found = true; break; }
    }
    if (!found) lines.push_back(val);

    std::ofstream f(opt, std::ios::trunc);
    for (auto const& l : lines) f << l << '\n';
}

void PackChangerScreen::applyPacks() {
    saveActivePacks();

    auto* ci = SDK::ClientInstance::get();
    if (!ci) { applySuccess = false; applyFeedbackUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000); return; }

    auto& rpm = ci->getResourcePackManager();
    void** vt  = *reinterpret_cast<void***>(&rpm);
    using Fn   = void(__thiscall*)(void*);
    auto fn    = reinterpret_cast<Fn>(vt[5]);

    __try {
        fn(&rpm);
        applySuccess = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        applySuccess = false;
    }

    applyFeedbackUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
}

// ============================================================================
// Layout
// ============================================================================
void PackChangerScreen::updateLayout(float W, float H) {
    float pw = PANEL_W, ph = PANEL_H;
    float px = (W - pw) * .5f, py = (H - ph) * .5f;
    panelRect = { px, py, px + pw, py + ph };
    titleRect = { px + PAD, py + PAD, px + pw - PAD, py + PAD + 30.f };

    float btnW = 100.f, btnH = 32.f;
    float btnY = py + ph - PAD - btnH;
    applyRect  = { px + pw - PAD - btnW, btnY, px + pw - PAD, btnY + btnH };
    closeRect  = { px + PAD, btnY, px + PAD + btnW, btnY + btnH };

    float listY1 = py + PAD + 36.f, listY2 = btnY - PAD, sbW = 8.f;
    scrollTrackRect = { px + pw - PAD - sbW, listY1, px + pw - PAD, listY2 };
    listRect        = { px + PAD, listY1, px + pw - PAD - sbW - 4.f, listY2 };

    float contentH = (float)packs.size() * (ROW_H + 4.f);
    float viewH    = listRect.getHeight();
    scrollMax      = std::max(0.f, contentH - viewH);

    float y = listRect.top - lerpScroll;
    for (auto& p : packs) {
        p.rowRect    = { listRect.left, y, listRect.right, y + ROW_H };
        float tw = 60.f, th = 24.f;
        float ty = y + (ROW_H - th) * .5f;
        p.toggleRect = { listRect.right - tw, ty, listRect.right, ty + th };
        y += ROW_H + 4.f;
    }

    if (scrollMax > 0.f && contentH > 0.f) {
        float ratio = viewH / contentH;
        float tbH   = std::max(20.f, scrollTrackRect.getHeight() * ratio);
        float tbY   = scrollTrackRect.top + (lerpScroll / scrollMax) *
                      (scrollTrackRect.getHeight() - tbH);
        scrollThumbRect = { scrollTrackRect.left, tbY, scrollTrackRect.right, tbY + tbH };
    } else {
        scrollThumbRect = scrollTrackRect;
    }
}

// ============================================================================
// Render
// ============================================================================
void PackChangerScreen::onRender(Event& evG) {
    auto& ev = reinterpret_cast<RenderLayerEvent&>(evG);
    MCDrawUtil dc { ev.getUIRenderContext(), Latite::get().getFont() };
    if (dirty) scanPacks();

    lerpScroll += (scroll - lerpScroll) * .18f;

    float W = Latite::getRenderer().getScreenSize().width, H = Latite::getRenderer().getScreenSize().height;
    updateLayout(W, H);

    // Backdrop
    dc.fillRectangle({ 0, 0, W, H }, d2d::Color(0, 0, 0, .45f));

    // Panel
    dc.fillRoundedRectangle(panelRect, d2d::Color(0x1e, 0x1e, 0x1e, .96f), RADIUS);
    dc.drawRectangle(panelRect, d2d::Color(0xff, 0xff, 0xff, .08f), 1.f);

    // Title
    dc.drawText(titleRect, L"Resource Pack Changer", d2d::Color(0xff, 0xff, 0xff),
                Renderer::FontSelection::PrimaryRegular, 22.f,
                DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Separator
    float sepY = titleRect.bottom + 4.f;
    dc.drawRectangle({ panelRect.left + PAD, sepY, panelRect.right - PAD, sepY + 1.f },
                     d2d::Color(0xff, 0xff, 0xff, .10f));

    // Scrollbar
    dc.fillRoundedRectangle(scrollTrackRect, d2d::Color(0xff, 0xff, 0xff, .06f), 4.f);
    if (scrollMax > 0.f)
        dc.fillRoundedRectangle(scrollThumbRect, d2d::Color(0xff, 0xff, 0xff, .22f), 4.f);

    // Rows
    Vec2 mouse = SDK::ClientInstance::get()->cursorPos;
    for (auto const& p : packs) {
        if (p.rowRect.bottom < listRect.top || p.rowRect.top > listRect.bottom) continue;

        dc.fillRoundedRectangle(p.rowRect,
            p.rowRect.contains(mouse) ? d2d::Color(0xff,0xff,0xff,.07f)
                                      : d2d::Color(0xff,0xff,0xff,.03f), 6.f);

        d2d::Rect nameRect = { p.rowRect.left + 8.f, p.rowRect.top,
                               p.toggleRect.left - 6.f, p.rowRect.bottom };
        dc.drawText(nameRect, p.displayName, d2d::Color(0xee,0xee,0xee),
                    Renderer::FontSelection::PrimaryRegular, 14.f,
                    DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        bool togHov = p.toggleRect.contains(mouse);
        d2d::Color togBg = p.active
            ? (togHov ? d2d::Color(0x22,0xc5,0x5e) : d2d::Color(0x1a,0xa8,0x50))
            : (togHov ? d2d::Color(0x55,0x55,0x55) : d2d::Color(0x3a,0x3a,0x3a));
        dc.fillRoundedRectangle(p.toggleRect, togBg, p.toggleRect.getHeight() * .35f);
        dc.drawText(p.toggleRect, p.active ? L"ON" : L"OFF", d2d::Color(0xff,0xff,0xff),
                    Renderer::FontSelection::PrimaryRegular, 12.f,
                    DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    if (packs.empty()) {
        dc.drawText(listRect, L"Tidak ada resource pack ditemukan.",
                    d2d::Color(0x88,0x88,0x88), Renderer::FontSelection::PrimaryRegular, 14.f,
                    DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    // Apply button
    bool showFB = applyFeedbackUntil.has_value() &&
                  std::chrono::steady_clock::now() < *applyFeedbackUntil;
    d2d::Color applyCol = showFB
        ? (applySuccess ? d2d::Color(0x1a,0xa8,0x50) : d2d::Color(0xcc,0x44,0x44))
        : (applyRect.contains(mouse) ? d2d::Color(0x4a,0x9a,0xff) : d2d::Color(0x2a,0x7a,0xff));
    dc.fillRoundedRectangle(applyRect, applyCol, 6.f);
    dc.drawText(applyRect,
                showFB ? (applySuccess ? L"Diterapkan!" : L"Gagal") : L"Terapkan",
                d2d::Color(0xff,0xff,0xff), Renderer::FontSelection::PrimaryRegular, 13.f,
                DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Close button
    dc.fillRoundedRectangle(closeRect,
        closeRect.contains(mouse) ? d2d::Color(0x55,0x55,0x55)
                                  : d2d::Color(0x33,0x33,0x33), 6.f);
    dc.drawText(closeRect, L"Tutup", d2d::Color(0xee,0xee,0xee),
                Renderer::FontSelection::PrimaryRegular, 13.f,
                DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

// ============================================================================
// Click (termasuk scroll wheel via ClickEvent::ClickType::Wheel)
// ============================================================================
void PackChangerScreen::onClick(Event& evG) {
    auto& ev = reinterpret_cast<ClickEvent&>(evG);
    Vec2 mouse = SDK::ClientInstance::get()->cursorPos;

    // Scroll wheel
    if (ev.getClickType() == ClickEvent::ClickType::Wheel) {
        if (panelRect.contains(mouse) && scrollMax > 0.f) {
            scroll -= ev.getWheelDelta() * 40.f;
            scroll  = std::clamp(scroll, 0.f, scrollMax);
            ev.setCancelled();
        }
        return;
    }

    // Left click down only
    if (ev.getClickType() != ClickEvent::ClickType::Left || !ev.isDown()) return;

    // Scrollbar drag start
    if (scrollThumbRect.contains(mouse) && scrollMax > 0.f) {
        draggingBar = true;
        dragOffset  = mouse.y - scrollThumbRect.top;
        ev.setCancelled();
        return;
    }

    // Close
    if (closeRect.contains(mouse)) { setActive(false); ev.setCancelled(); return; }

    // Apply
    if (applyRect.contains(mouse)) { applyPacks(); ev.setCancelled(); return; }

    // Toggle pack
    for (auto& p : packs) {
        if (p.toggleRect.contains(mouse)) {
            p.active = !p.active;
            ev.setCancelled();
            return;
        }
    }

    // Click luar panel
    if (!panelRect.contains(mouse)) { setActive(false); ev.setCancelled(); }
}

// ============================================================================
// Key
// ============================================================================
void PackChangerScreen::onKey(Event& evG) {
    auto& ev = reinterpret_cast<KeyUpdateEvent&>(evG);
    if (ev.getKey() == VK_ESCAPE && ev.isDown()) {
        setActive(false);
        ev.setCancelled();
    }
    // Scroll via arrow keys
    if (ev.isDown() && scrollMax > 0.f) {
        if (ev.getKey() == VK_DOWN) scroll = std::clamp(scroll + 40.f, 0.f, scrollMax);
        if (ev.getKey() == VK_UP)   scroll = std::clamp(scroll - 40.f, 0.f, scrollMax);
    }
}
