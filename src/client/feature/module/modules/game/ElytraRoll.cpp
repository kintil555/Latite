#include "pch.h"
#include "ElytraRoll.h"
#include "client/event/events/TurnDeltaEvent.h"
#include "client/event/events/UpdatePlayerCameraEvent.h"
#include <algorithm>
#include <cmath>

ElytraRoll::ElytraRoll()
    : Module("ElytraRoll", LocalizeString::get("client.module.elytraRoll.name"),
             LocalizeString::get("client.module.elytraRoll.desc"), GAME) {
    addSliderSetting("maxRoll", LocalizeString::get("client.module.elytraRoll.maxRoll.name"),
                     LocalizeString::get("client.module.elytraRoll.maxRoll.desc"), this->maxRoll, FloatValue(0.f),
                     FloatValue(60.f), FloatValue(1.f));
    addSliderSetting("strength", LocalizeString::get("client.module.elytraRoll.strength.name"),
                     LocalizeString::get("client.module.elytraRoll.strength.desc"), this->strength, FloatValue(0.1f),
                     FloatValue(5.f), FloatValue(0.1f));
    addSliderSetting("smoothing", LocalizeString::get("client.module.elytraRoll.smoothing.name"),
                     LocalizeString::get("client.module.elytraRoll.smoothing.desc"), this->smoothing, FloatValue(1.f),
                     FloatValue(20.f), FloatValue(0.5f));
    addSetting("invert", LocalizeString::get("client.module.elytraRoll.invert.name"),
               LocalizeString::get("client.module.elytraRoll.invert.desc"), this->invert);

    listen<TurnDeltaEvent>(static_cast<EventListenerFunc>(&ElytraRoll::onTurnDelta));
    listen<UpdatePlayerCameraEvent>(static_cast<EventListenerFunc>(&ElytraRoll::onCameraUpdate));
}

void ElytraRoll::onTurnDelta(Event& evG) {
    // delta.y = yaw (horizontal), see Gyro::onTurnDelta
    auto& ev = reinterpret_cast<TurnDeltaEvent&>(evG);
    yawInput += ev.getDelta().y;
}

void ElytraRoll::onCameraUpdate(Event& evG) {
    auto& ev = reinterpret_cast<UpdatePlayerCameraEvent&>(evG);

    auto now = std::chrono::steady_clock::now();
    float dt = lastUpdate.time_since_epoch().count() == 0
                   ? 0.f
                   : std::chrono::duration<float>(now - lastUpdate).count();
    lastUpdate = now;
    dt = std::clamp(dt, 0.f, 0.1f);

    auto* client = SDK::ClientInstance::get();
    auto* player = client ? client->getLocalPlayer() : nullptr;
    const bool gliding = player && player->getStatusFlag(kGlidingFlag);

    float target = 0.f;
    if (gliding && dt > 0.f) {
        const float maxDeg = std::get<FloatValue>(maxRoll).value;
        const float sign = std::get<BoolValue>(invert).value ? 1.f : -1.f;
        // turn speed (deg/s) scaled into a target tilt
        const float turnRate = yawInput / dt;
        target = std::clamp(sign * turnRate * std::get<FloatValue>(strength).value * 0.05f, -maxDeg, maxDeg);
    }
    yawInput = 0.f;

    const float k = 1.f - std::exp(-std::get<FloatValue>(smoothing).value * dt);
    currentRoll += (target - currentRoll) * k;

    if (std::abs(currentRoll) > 0.01f) {
        ev.setRoll(currentRoll);
    } else {
        currentRoll = 0.f;
    }
}
