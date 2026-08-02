#include "pch.h"
#include "InGamePackChanger.h"

#include "client/screen/ScreenManager.h"
#include "client/screen/screens/PackChangerScreen.h"
#include "client/Latite.h"

InGamePackChanger::InGamePackChanger()
    : Module("InGamePackChanger",
             L"In-Game Pack Changer",
             L"Ganti resource pack tanpa keluar dari world atau server.",
             GAME) {
}

void InGamePackChanger::onEnable() {
    Latite::getScreenManager().showScreen<PackChangerScreen>();
}

void InGamePackChanger::onDisable() {
    // Screen handle disable sendiri, tidak perlu apa-apa di sini
}
