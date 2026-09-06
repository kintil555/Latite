#pragma once
#include "client/event/Event.h"
#include "util/Crypto.h"

// Fired right before the game force-closes the current UI screen because
// the local player just took damage (e.g. closing chat/forms on hurt).
// Cancel it to keep the screen open — used by AntiScreenClose.
class PlayerHurtScreenCloseEvent : public Cancellable {
public:
    static const uint32_t hash = TOHASH(PlayerHurtScreenCloseEvent);

    PlayerHurtScreenCloseEvent() {}
};
