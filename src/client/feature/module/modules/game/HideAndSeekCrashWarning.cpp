#include "pch.h"
#include "HideAndSeekCrashWarning.h"

#include "client/event/events/ClientTextEvent.h"
#include "client/event/events/LeaveGameEvent.h"
#include "client/event/events/UpdateEvent.h"
#include "client/Latite.h"

#include "mc/common/network/MinecraftPackets.h"
#include "mc/common/network/packet/CommandRequestPacket.h"
#include "mc/common/network/RemoteConnectorComposite.h"

#include <cctype>

using namespace std::chrono_literals;

namespace {
    constexpr std::string_view hiveAddress = "hivebedrock.network";
    constexpr auto connectionRefreshDelay = 3s;
    constexpr auto connectionResponseWindow = 20s;
} // namespace

// This module is intentionally always-on: it is constructed with visible =
// false (so it never appears in ClickGUI and cannot be disabled by the
// player) and its listeners are registered directly on Eventing in the
// constructor rather than through Module::listen(), which normally gates
// callbacks behind isEnabled(). See Module::isVisible()/ClickGUI.cpp filter.
//
// It also polls "/connection" itself (same trick DiscordPresence.cpp uses to
// learn the current Hive game/hub code) instead of relying on DiscordPresence
// for that data, since DiscordPresence is a regular toggleable module and
// could be disabled by the player, which would silently break this warning.
HideAndSeekCrashWarning::HideAndSeekCrashWarning()
    : Module("HideAndSeekCrashWarning", LocalizeString::get("client.module.hideAndSeekCrashWarning.name"),
             LocalizeString::get("client.module.hideAndSeekCrashWarning.desc"), OTHER, nokeybind, false, false) {
    Eventing::get().listen<UpdateEvent, &HideAndSeekCrashWarning::onUpdate>(this, 0, true);
    Eventing::get().listen<ClientTextEvent, &HideAndSeekCrashWarning::onClientText>(this, 100, true);
    Eventing::get().listen<LeaveGameEvent, &HideAndSeekCrashWarning::onLeaveGame>(this, 0, true);
}

void HideAndSeekCrashWarning::onLeaveGame(LeaveGameEvent&) {
    activeServerAddress.clear();
    onHive = false;
    warned = false;
    connectionRefreshAt = {};
    suppressUntil = {};
}

void HideAndSeekCrashWarning::updateConnectionState() {
    const auto now = std::chrono::steady_clock::now();

    SDK::ClientInstance* clientInstance = SDK::ClientInstance::get();
    SDK::Social::GameConnectionInfo* connectionInfo = SDK::RemoteConnectorComposite::getConnectionInfo();

    std::string serverAddress;
    if (clientInstance && clientInstance->minecraft && clientInstance->minecraft->getLevel() &&
        clientInstance->getLocalPlayer() && connectionInfo) {
        serverAddress = connectionInfo->unresolvedUrl + '\n' + connectionInfo->hostIpAddress + '\n' +
                        connectionInfo->thirdPartyServerInfo.creatorName;
    }

    if (serverAddress != activeServerAddress) {
        activeServerAddress = serverAddress;

        onHive = connectionInfo && (connectionInfo->unresolvedUrl.find(hiveAddress) != std::string::npos ||
                                    connectionInfo->hostIpAddress.find(hiveAddress) != std::string::npos);

        warned = false;
        suppressUntil = {};
        connectionRefreshAt = onHive ? now + connectionRefreshDelay : std::chrono::steady_clock::time_point {};
    }

    if (connectionRefreshAt == std::chrono::steady_clock::time_point {} || now < connectionRefreshAt || !onHive ||
        !clientInstance || !clientInstance->getLocalPlayer() || !clientInstance->getLocalPlayer()->packetSender) {
        return;
    }

    std::shared_ptr<SDK::Packet> packet = SDK::MinecraftPackets::createPacket(SDK::PacketID::COMMAND_REQUEST);
    if (!packet) return;

    auto* command = reinterpret_cast<SDK::CommandRequestPacket*>(packet.get());
    command->applyCommand("/connection");

    connectionRefreshAt = {};
    suppressUntil = now + connectionResponseWindow;
    clientInstance->getLocalPlayer()->packetSender->sendToServer(packet.get());
}

void HideAndSeekCrashWarning::onUpdate(UpdateEvent&) {
    updateConnectionState();
}

void HideAndSeekCrashWarning::onClientText(ClientTextEvent& ev) {
    if (warned || !onHive) return;
    if (std::chrono::steady_clock::now() > suppressUntil) return;

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
