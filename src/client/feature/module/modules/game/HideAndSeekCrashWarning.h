#pragma once
#include <client/feature/module/Module.h>

class HideAndSeekCrashWarning : public Module {
public:
    HideAndSeekCrashWarning();
    virtual ~HideAndSeekCrashWarning() {};

    void onText(Event&);
};
