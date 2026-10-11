#pragma once
#include "../Command.h"

// Developer tool: scans the local player's SerializedSkin memory for SkinImage-like
// structs (format/width/height/blob) so the cape image offset can be located in-game.
class SkinProbeCommand final : public Command {
public:
    SkinProbeCommand();
    ~SkinProbeCommand() = default;

    bool execute(std::string const label, std::vector<std::string> args) override;
};
