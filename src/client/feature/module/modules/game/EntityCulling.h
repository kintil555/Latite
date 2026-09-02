#pragma once
#include <client/feature/module/Module.h>
#include <client/event/events/AfterEntityRenderEvent.h>

class EntityCulling : public Module {
public:
    EntityCulling();
    virtual ~EntityCulling() {};

    void onRender(Event&);

private:
    ValueType useDistance = BoolValue(true);
    ValueType maxDistance = FloatValue(48.f);
    ValueType useFOV = BoolValue(true);
    ValueType fov = FloatValue(110.f);
};
