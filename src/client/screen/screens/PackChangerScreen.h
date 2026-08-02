#pragma once
#include "../Screen.h"
#include <filesystem>
#include <vector>
#include <string>
#include <optional>
#include <chrono>

class PackChangerScreen : public Screen {
public:
    PackChangerScreen();

    std::string getName() override { return "PackChanger"; }

    void onRender(Event& ev);
    void onClick(Event& ev);
    void onKey(Event& ev);

protected:
    void onEnable(bool ignoreAnimations) override;
    void onDisable() override;

private:
    struct PackEntry {
        std::wstring displayName;
        std::filesystem::path path;
        bool active = false;
        d2d::Rect rowRect    = {};
        d2d::Rect toggleRect = {};
    };

    void scanPacks();
    void loadActivePacks();
    void saveActivePacks();
    void applyPacks();
    std::filesystem::path getMinecraftResourcePacksPath() const;
    std::filesystem::path getOptionsPath() const;
    bool isValidPack(std::filesystem::path const& p) const;
    void updateLayout(float screenW, float screenH);

    std::vector<PackEntry> packs;
    std::vector<std::wstring> activePacks;

    d2d::Rect panelRect       = {};
    d2d::Rect closeRect       = {};
    d2d::Rect applyRect       = {};
    d2d::Rect titleRect       = {};
    d2d::Rect listRect        = {};
    d2d::Rect scrollTrackRect = {};
    d2d::Rect scrollThumbRect = {};

    float scroll      = 0.f;
    float scrollMax   = 0.f;
    float lerpScroll  = 0.f;
    bool  draggingBar = false;
    float dragOffset  = 0.f;
    bool  dirty       = true;

    std::optional<std::chrono::steady_clock::time_point> applyFeedbackUntil;
    bool applySuccess = false;

    static constexpr float ROW_H   = 44.f;
    static constexpr float PAD     = 12.f;
    static constexpr float RADIUS  = 8.f;
    static constexpr float PANEL_W = 480.f;
    static constexpr float PANEL_H = 520.f;
};
