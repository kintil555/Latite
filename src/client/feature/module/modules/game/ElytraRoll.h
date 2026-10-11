#pragma once
#include "../../Module.h"

class ElytraRoll : public Module {
public:
    ElytraRoll();
    virtual ~ElytraRoll() {};

    void onTurnDelta(Event&);
    void onCameraUpdate(Event&);

private:
    static constexpr int kGlidingFlag = 32; // ActorFlags::GLIDING (Bedrock entity flag id)

    ValueType maxRoll = FloatValue(25.f);
    ValueType strength = FloatValue(1.5f);
    ValueType smoothing = FloatValue(8.f);
    ValueType invert = BoolValue(false);

    float yawInput = 0.f; // yaw delta accumulated since last camera update
    float currentRoll = 0.f;
    std::chrono::steady_clock::time_point lastUpdate {};
};
