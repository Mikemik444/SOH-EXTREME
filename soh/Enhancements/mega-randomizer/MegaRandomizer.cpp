#include "MegaRandomizer.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace MegaRandomizer {
namespace {
Settings gSettings;

std::string Trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

bool ToBool(const std::string& value) {
    return value == "true" || value == "1" || value == "yes" || value == "on";
}

SoulMode ToSoulMode(const std::string& value) {
    if (value == "by_type") return SoulMode::ByType;
    if (value == "by_region") return SoulMode::ByRegion;
    if (value == "individual") return SoulMode::Individual;
    return SoulMode::Off;
}

void LoadConfig() {
    gSettings = {};
    std::ifstream in("mega_randomizer.cfg");
    if (!in.good()) {
        // The launcher also copies the config beside this source while developing.
        in.open("soh/soh/Enhancements/mega-randomizer/mega_randomizer.cfg");
    }
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));
        if (key == "seed") {
            try { gSettings.seed = std::stoull(value); } catch (...) { gSettings.seed = 0; }
        } else if (key == "barren_start") {
            gSettings.barrenStart = ToBool(value);
        } else if (key == "everything_sanity") {
            gSettings.everythingSanity = ToBool(value);
        } else if (key.rfind("ability.", 0) == 0) {
            gSettings.abilities[key.substr(8)] = ToBool(value);
        } else if (key.rfind("check.", 0) == 0) {
            gSettings.checks[key.substr(6)] = ToBool(value);
        } else if (key.rfind("soul.", 0) == 0) {
            gSettings.souls[key.substr(5)] = ToSoulMode(value);
        }
    }
}
}

const Settings& GetSettings() { return gSettings; }

bool AbilityEnabled(const std::string& key) {
    const auto it = gSettings.abilities.find(key);
    return it != gSettings.abilities.end() && it->second;
}

bool CheckEnabled(const std::string& key) {
    const auto it = gSettings.checks.find(key);
    return it != gSettings.checks.end() && it->second;
}

SoulMode GetSoulMode(const std::string& key) {
    const auto it = gSettings.souls.find(key);
    return it == gSettings.souls.end() ? SoulMode::Off : it->second;
}

void Init() {
    LoadConfig();
    // Hook registrations are intentionally centralized here so the Mega systems can
    // attach to Ship's actor/player/randomizer lifecycle without touching decomp files.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadFile>([](int32_t) { LoadConfig(); });
}

static RegisterShipInitFunc sMegaRandomizerInit(Init);
}
