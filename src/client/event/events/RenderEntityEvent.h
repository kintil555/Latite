#pragma once
#include <util/LMath.h>
#include <client/event/Event.h>

namespace SDK {
    class Actor;
}

// Dispatched right before an entity is rendered, allowing modules to cancel
// (cull) the render entirely. Distinct from AfterRenderEntityEvent, which
// fires post-render and is currently non-functional on 1.21.92+.
class RenderEntityEvent : public Cancellable {
private:
    SDK::Actor* entity;
    Vec3 cameraPos;

public:
    static const uint32_t hash = TOHASH(RenderEntityEvent);

    RenderEntityEvent(SDK::Actor* entity, Vec3 const& cameraPos)
        : entity(entity)
        , cameraPos(cameraPos) {}

    SDK::Actor* getEntity() const { return entity; }
    Vec3 const& getCameraPos() const { return cameraPos; }
};
