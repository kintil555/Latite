#pragma once
#include "../../Module.h"

class FlipMesh final : public Module {
public:
    FlipMesh();
    ~FlipMesh() override = default;

    void onEnable() override;
    void onDisable() override;

private:
    bool writePack();
    void removePack();
};
