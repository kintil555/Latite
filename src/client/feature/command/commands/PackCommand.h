#pragma once
#include "client/feature/command/Command.h"

// .pack                - list semua resource pack + status
// .pack on <nama>      - aktifkan pack
// .pack off <nama>     - non-aktifkan pack
// .pack apply          - terapkan perubahan (tulis options.txt)
class PackCommand : public Command {
public:
    PackCommand();
    bool execute(std::string const label, std::vector<std::string> args) override;

private:
    std::filesystem::path getPacksPath() const;
    std::filesystem::path getOptionsPath() const;

    struct PackEntry {
        std::wstring name;
        bool active = false;
    };

    std::vector<PackEntry> packs;
    bool dirty = true;

    void scanPacks();
    void loadActive();
    void saveAndApply();
};
