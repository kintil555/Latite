#pragma once
#include "../Command.h"

// Developer tool: dumps function offsets and ready-to-paste signatures (see util/SigDump.h)
class SigDumpCommand final : public Command {
public:
    SigDumpCommand();
    ~SigDumpCommand() = default;

    bool execute(std::string const label, std::vector<std::string> args) override;
};
