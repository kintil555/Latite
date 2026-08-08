#include "pch.h"
#include "HideAndSeekCrashWarning.h"

#include "client/event/events/ClientTextEvent.h"
#include "client/event/events/LeaveGameEvent.h"
#include "client/Latite.h"

#include <cctype>

// This module is intentionally always-on: it is constructed with visible =
// false (so it never appears in ClickGUI and cannot be disabled by the
// player) and its listener is registered directly on Eventing in the
// constructor rather than through Module::listen(), which normally gates
// callbacks behind isEnabled(). See Module::isVisible()/ClickGUI.cpp filter.
HideAndSeekCrashWarning::HideAndSeekCrashWarning()
    : Module("HideAndSeekCrashWarning", LocalizeString::get("client.module.hideAndSeekCrashWarning.name"),
             LocalizeString::get("client.module.hideAndSeekCrashWarning.desc"), OTHER, nokeybind, false, false) {
    Eventing::get().listen<ClientTextEvent, &HideAndSeekCrashWarning::onClientText>(this, 10, true);
    Eventing::get().listen<LeaveGameEvent, &HideAndSeekCrashWarning::onLeaveGame>(this, 10, true);
}

void HideAndSeekCrashWarning::onLeaveGame(LeaveGameEvent&) {
    warned = false;
}

void HideAndSeekCrashWarning::onClientText(ClientTextEvent& ev) {
    if (warned) return;

    SDK::TextPacket* textPacket = ev.getTextPacket();
    std::string* rawMessagePtr = textPacket ? textPacket->getMessage() : nullptr;
    if (!rawMessagePtr) return;

    // Strip Minecraft formatting codes (both raw section-sign and its
    // UTF-8 encoding 0xC2 0xA7) so we can match plain text reliably,
    // mirroring the same cleanup DiscordPresence::onClientText does.
    std::string message;
    const std::string& rawMessage = *rawMessagePtr;
    message.reserve(rawMessage.size());

    for (size_t i = 0; i < rawMessage.size(); i++) {
        const auto ch = static_cast<unsigned char>(rawMessage[i]);
        if (ch == 0xC2 && i + 2 < rawMessage.size() && static_cast<unsigned char>(rawMessage[i + 1]) == 0xA7) {
            i += 2;
            continue;
        }
        if (ch == 0xA7 && i + 1 < rawMessage.size()) {
            i++;
            continue;
        }
        message.push_back(rawMessage[i]);
    }

    constexpr std::string_view serverNamePrefix = "You are connected to server name ";
    if (!message.starts_with(serverNamePrefix)) return;

    std::string gameCode = message.substr(serverNamePrefix.size());
    while (!gameCode.empty() && std::isspace(static_cast<unsigned char>(gameCode.back()))) {
        gameCode.pop_back();
    }
    while (!gameCode.empty() && std::isdigit(static_cast<unsigned char>(gameCode.back()))) {
        gameCode.pop_back();
    }
    while (!gameCode.empty() && std::isspace(static_cast<unsigned char>(gameCode.back()))) {
        gameCode.pop_back();
    }

    // "HIDE" is the Hive's server-name code for Hide and Seek (see the same
    // code used in DiscordPresence::hiveGameNames), covering both the hub
    // ("HIDE") and in-game ("HIDE" variants) server name forms.
    if (gameCode != "HIDE" && !gameCode.starts_with("HIDE")) return;

    warned = true;
    Latite::getClientMessageQueue().push(
        "\u00a7l\u00a7cWarning:\u00a7r\u00a7c This client can suddenly crash while in the Hide and Seek hub, "
        "especially on the upper floors of the hub building. Please avoid moving around too much up there.");
}
