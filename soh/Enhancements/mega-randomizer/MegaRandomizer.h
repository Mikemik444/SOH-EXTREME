#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace MegaRandomizer {

enum class SoulMode { Off, ByType, ByRegion, Individual };

struct Settings {
    uint64_t seed = 0;
    bool barrenStart = false;
    bool everythingSanity = false;
    std::unordered_map<std::string, bool> abilities;
    std::unordered_map<std::string, bool> checks;
    std::unordered_map<std::string, SoulMode> souls;
};

void Init();
const Settings& GetSettings();
bool AbilityEnabled(const std::string& key);
bool CheckEnabled(const std::string& key);
SoulMode GetSoulMode(const std::string& key);

}
