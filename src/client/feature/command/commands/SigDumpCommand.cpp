#include "pch.h"
#include "SigDumpCommand.h"
#include "util/SigDump.h"

SigDumpCommand::SigDumpCommand()
    : Command("sigdump", L"Dumps offsets and signatures when an event fires",
              "<arm <event|*> [count] | trace <sigName|0xRVA> [stacks] | str <text> | xref <sigName|0xRVA> | vref <sigName|0xRVA> | off | status | events | all | hooks | at <rva|address>>", { "sd" }) {}

bool SigDumpCommand::execute(std::string const label, std::vector<std::string> args) {
    if (args.empty()) return false;

    std::string sub;
    for (char c : args[0])
        sub += (char)tolower((unsigned char)c);

    if (sub == "arm") {
        if (args.size() < 2) return false;
        int count = 1;
        if (args.size() > 2) {
            try {
                count = std::stoi(args[2]);
            } catch (...) {
                return false;
            }
        }
        message(SigDump::arm(args[1], count) + ". Trigger the action in game, result is written to the SigDump folder.");
        return true;
    }
    if (sub == "trace") {
        if (args.size() < 2) return false;
        int count = 3;
        if (args.size() > 2) {
            try {
                count = std::stoi(args[2]);
            } catch (...) {
                return false;
            }
        }
        message(SigDump::trace(args[1], count));
        return true;
    }
    if (sub == "str") {
        if (args.size() < 2) return false;
        std::string text = args[1];
        for (size_t i = 2; i < args.size(); i++)
            text += " " + args[i];
        message(SigDump::strRefs(text));
        return true;
    }
    if (sub == "xref") {
        if (args.size() < 2) return false;
        message(SigDump::xrefs(args[1]));
        return true;
    }
    if (sub == "vref") {
        if (args.size() < 2) return false;
        message(SigDump::vrefs(args[1]));
        return true;
    }
    if (sub == "off") {
        message(SigDump::disarm());
        return true;
    }
    if (sub == "status") {
        message(SigDump::status());
        return true;
    }
    if (sub == "events") {
        std::string list;
        for (auto& e : SigDump::knownEvents())
            list += e + ", ";
        if (list.size() >= 2) list.resize(list.size() - 2);
        message("Events (substring match, case-insensitive): " + list);
        return true;
    }
    if (sub == "all") {
        message(SigDump::dumpAll());
        return true;
    }
    if (sub == "hooks") {
        message(SigDump::dumpHooks());
        return true;
    }
    if (sub == "at") {
        if (args.size() < 2) return false;
        uintptr_t v = 0;
        try {
            v = std::stoull(args[1], nullptr, 16);
        } catch (...) {
            message("Invalid hex value", true);
            return true;
        }
        message(SigDump::dumpAt(v));
        return true;
    }
    return false;
}
