#pragma once
#include "../../Module.h"

class InGamePackChanger : public Module {
public:
    InGamePackChanger();
    virtual ~InGamePackChanger() = default;

    void onEnable() override;
    void onDisable() override;
};
