#pragma once
#include "../../Module.h"

class DisableParticles : public Module {
public:
    DisableParticles()
        : Module("DisableParticles", LocalizeString::get("client.module.disableParticles.name"),
                 LocalizeString::get("client.module.disableParticles.desc"), GAME) {}

    ~DisableParticles() override {}

    void onEnable() override;
    void onDisable() override;
};
