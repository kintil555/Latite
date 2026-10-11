#pragma once
class UpdatePlayerCameraEvent : public Event {
public:
    static const uint32_t hash = TOHASH(UpdatePlayerCameraEvent);

    void setViewAngles(Vec2 const& angles) { newViewAngles = angles; }

    std::optional<Vec2> getNewRot() { return newViewAngles; }

    // Camera roll in degrees (rotation around the view axis).
    void setRoll(float degrees) { newRoll = degrees; }
    std::optional<float> getNewRoll() { return newRoll; }

private:
    std::optional<Vec2> newViewAngles;
    std::optional<float> newRoll;
};
