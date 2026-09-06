#include "pch.h"
#include "ProximityChat.h"
#include "client/event/events/TickEvent.h"
#include "mc/common/network/RemoteConnectorComposite.h"

namespace {
    // Minecraft yaw 0 = south (+Z), increasing clockwise when viewed from
    // above; pitch 0 = level, positive = looking down (same convention
    // Hitboxes::onRenderLevel already relies on for its own yaw/pitch use).
    // Mumble just wants plain unit vectors in its own left-handed, Y-up,
    // meters space — build "front" and "top" straight from the angles
    // rather than pulling in a matrix/quaternion helper for two vectors.
    void anglesToFrontTop(float yawDeg, float pitchDeg, float* outFront, float* outTop) {
        float yaw   = LatiteMath::deg2rad(yawDeg);
        float pitch = LatiteMath::deg2rad(pitchDeg);

        float cosPitch = std::cos(pitch);
        float sinPitch = std::sin(pitch);
        float cosYaw   = std::cos(yaw);
        float sinYaw   = std::sin(yaw);

        // Forward vector: yaw rotates in the XZ plane, pitch tilts it
        // toward/away from Y. Negative sinPitch because positive pitch
        // (looking down) should point the vector toward -Y.
        outFront[0] = -sinYaw * cosPitch;
        outFront[1] = -sinPitch;
        outFront[2] = cosYaw * cosPitch;

        // "Top" (head's up vector) is front rotated +90 degrees along the
        // same pitch axis, reusing the exact same yaw/pitch rotation family
        // offset by pi/2 — that construction guarantees it stays exactly
        // perpendicular to front at every angle, unlike composing it from
        // front's components directly (which drifts off-perpendicular
        // everywhere except the pitch==0 case).
        float topPitch    = pitch - (pi_f / 2.f);
        float cosTopPitch = std::cos(topPitch);
        float sinTopPitch = std::sin(topPitch);

        outTop[0] = -sinYaw * cosTopPitch;
        outTop[1] = -sinTopPitch;
        outTop[2] = cosYaw * cosTopPitch;
    }
}

ProximityChat::ProximityChat()
    : Module("ProximityChat", L"Proximity Chat",
             L"Feeds your position into Mumble's positional audio (Mumble Link) so voice chat gets quieter with "
             L"distance — free, no extra server, just open Mumble and join the same channel as others.",
             GAME) {
    addSliderSetting("distanceScale", L"Distance Scale",
                     L"Blocks-per-meter fudge factor if voices feel too loud/quiet at range. 1.0 = 1 block = 1 "
                     L"meter.",
                     distanceScale, FloatValue(0.5f), FloatValue(3.f), FloatValue(0.1f));

    listen<TickEvent>((EventListenerFunc)&ProximityChat::onTick);
}

ProximityChat::~ProximityChat() {
    // linkClient's destructor (if still open) unmaps/closes on its own;
    // nothing else to release here.
}

void ProximityChat::onEnable() {
    linkClient = std::make_unique<MumbleLinkClient>();
    if (!linkClient->isOpen()) {
        // Mapping only fails on genuine OS-level errors (e.g. permissions);
        // Mumble not running is NOT a failure case — the mapping still
        // succeeds and just sits there unread until Mumble opens later.
        linkClient.reset();
    }
}

void ProximityChat::onDisable() {
    linkClient.reset();
}

void ProximityChat::onTick(Event&) {
    if (!linkClient || !linkClient->isOpen()) return;

    auto lp = SDK::ClientInstance::get()->getLocalPlayer();
    if (!lp) return;

    float scale = std::get<FloatValue>(distanceScale);

    Vec3 pos = lp->getPos();
    Vec2 rot = lp->getRot(); // x = pitch, y = yaw

    float avatarPos[3] = { pos.x * scale, pos.y * scale, pos.z * scale };
    float front[3];
    float top[3];
    anglesToFrontTop(rot.y, rot.x, front, top);

    // First-person: camera and avatar (ears and eyes) are the same point —
    // no separate spectator/third-person camera position to track here.
    std::wstring identity = util::StrToWStr(lp->getNameTag());

    std::string context = "latite-proximity-chat:none";
    auto* connectionInfo = SDK::RemoteConnectorComposite::getConnectionInfo();
    if (connectionInfo && !connectionInfo->hostIpAddress.empty()) {
        context = "latite-proximity-chat:" + connectionInfo->hostIpAddress + ":" +
                  std::to_string(connectionInfo->port);
    }

    linkClient->update(avatarPos, front, top, avatarPos, front, top, identity, context);
}
