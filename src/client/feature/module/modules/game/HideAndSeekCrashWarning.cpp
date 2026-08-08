#include "pch.h"
#include "HideAndSeekCrashWarning.h"
#include "client/Latite.h"

HideAndSeekCrashWarning::HideAndSeekCrashWarning()
    : Module("HideAndSeekCrashWarning", LocalizeString::get("client.module.hideAndSeekCrashWarning.name"),
             LocalizeString::get("client.module.hideAndSeekCrashWarning.desc"), OTHER, nokeybind) {
    listen<ChatMessageEvent>(static_cast<EventListenerFunc>(&HideAndSeekCrashWarning::onText));
}

void HideAndSeekCrashWarning::onText(Event& evG) {
    ChatMessageEvent& ev = reinterpret_cast<ChatMessageEvent&>(evG);

    std::string chatMessage = ev.getMessage();

    if (chatMessage.find("Hide and Seek") != std::string::npos &&
        (chatMessage.find("starting") != std::string::npos || chatMessage.find("started") != std::string::npos)) {
        Latite::getClientMessageQueue().push(
            "\u00a77\u00a7lLatite\u00a7r \u00a7c- Hide and Seek is known to cause crashes on some setups. "
            "Consider leaving if your client becomes unstable.");
    }
}
