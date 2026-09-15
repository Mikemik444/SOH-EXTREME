#include "ArchipelagoClient.h"

static bool ParseFlatStringIntObject(const std::string& raw, std::unordered_map<std::string, int64_t>& out);

#include <Archipelago.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <set>

#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/item.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/Enhancements/randomizer/savefile.h"
#include "soh/Enhancements/randomizer/settings.h"
#include "soh/Network/Archipelago/ArchipelagoC.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/static_data.h"
#include "soh/Enhancements/randomizer/randomizerEnums/RandomizerCheck.h"
#include "soh/Enhancements/randomizer/randomizerEnums.h"
#include "soh/Enhancements/randomizer/randomizerEnums/RandomizerInf.h"
#include "soh/Enhancements/randomizer/randomizerEnums/RandomizerMiscEnums.h"
#include "soh/Notification/Notification.h"

extern "C" {
#include "macros.h"
#include "z64.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Ossan/z_en_ossan.h"
extern PlayState* gPlayState;
}

extern "C" u16 Randomizer_Item_Give(PlayState* play, GetItemEntry giEntry);

namespace {
constexpr int64_t AP_EXTREME_ITEM_BASE = 9500000;
constexpr int64_t AP_EXTREME_SPEECH_BASE = 9600000;
constexpr int64_t AP_EXTREME_SPEECH_FALLBACK_BASE = 9650000;
constexpr int64_t AP_EXTREME_SPEECH_FALLBACK_COUNT = 512;

constexpr int64_t AP_ITEM_ROLL = 9500000;
constexpr int64_t AP_ITEM_GRAB = 9500001;
constexpr int64_t AP_ITEM_CLIMB = 9500002;
constexpr int64_t AP_ITEM_CRAWL = 9500003;
constexpr int64_t AP_ITEM_SPEAK = 9500004;
constexpr int64_t AP_ITEM_OPEN_CHEST = 9500005;
constexpr int64_t AP_ITEM_ENEMY_SOUL = 9500006;
constexpr int64_t AP_ITEM_NPC_SOUL = 9500007;
constexpr int64_t AP_ITEM_ANIMAL_SOUL = 9500008;
constexpr int64_t AP_ITEM_POT_SOUL = 9500009;
constexpr int64_t AP_ITEM_CRATE_SOUL = 9500010;
constexpr int64_t AP_ITEM_GRASS_SOUL = 9500011;
constexpr int64_t AP_ITEM_ROCK_SOUL = 9500012;
constexpr int64_t AP_ITEM_TREE_SOUL = 9500013;
constexpr int64_t AP_ITEM_BEEHIVE_SOUL = 9500014;
constexpr int64_t AP_ITEM_SIGN_SOUL = 9500015;
constexpr int64_t AP_ITEM_SKULLTULA_SOUL = 9500016;
constexpr int64_t AP_ITEM_BUSINESS_SCRUB_SOUL = 9500017;
constexpr int64_t AP_ITEM_SHOVEL = 9500018;
constexpr int64_t AP_ITEM_FLOW_OF_TIME = 9500019;


// Archipelago NetworkItem.flags:
//   0x01 progression, 0x02 useful, 0x04 trap.
// Treat progression OR useful as "important" for the colored AP icon.
// Filler/junk/trap-only remote items use the grayscale icon.
static RandomizerGet GetRemoteArchipelagoDisplay(int flags) {
    return (flags & 0x03) != 0 ? RG_AP_REMOTE_IMPORTANT : RG_AP_REMOTE_NORMAL;
}
constexpr int64_t AP_FIRST_SONG_NOTE = 9500020;
constexpr int64_t AP_LAST_SONG_NOTE = 9500093;

// AP base item IDs 1..276 intentionally match SoH RandomizerGet IDs in the
// 9.2.3-based world. This is the same numbering used by Items.py.
constexpr int64_t AP_BASE_ITEM_MIN = 1;
constexpr int64_t AP_BASE_ITEM_MAX = 276;

static void SetQuestSong(int quest) {
    gSaveContext.inventory.questItems |= (1u << quest);
}

static bool HasNote(RandomizerInf first, int offset) {
    return Flags_GetRandomizerInf(static_cast<RandomizerInf>(static_cast<int>(first) + offset));
}

static RandomizerGet MapApItemNameToRandomizerGet(const std::string& itemName) {
    static const std::unordered_map<std::string, RandomizerGet> kItemNameMap = {
#include "ArchipelagoNameMap.inc"
    };

    auto it = kItemNameMap.find(itemName);
    if (it != kItemNameMap.end()) {
        return it->second;
    }

    // All 74 SOH-EXTREME Song Notes intentionally share one physical display model.
    // Their actual reward is still the exact AP note ID handled by ProcessItem().
    if (itemName.rfind("Song Note", 0) == 0) {
        return RG_SONG_OF_TIME;
    }

    return RG_NONE;
}

// Ice Traps should mimic valuable items instead of exposing themselves with one
// fixed model. The disguise is stable per AP location.
static RandomizerGet GetIceTrapDisguise(int64_t apLocation) {
    static constexpr const char* kMajorDisguises[] = {
        "Progressive Hookshot",
        "Bow",
        "Boomerang",
        "Lens of Truth",
        "Megaton Hammer",
        "Progressive Strength Upgrade",
        "Progressive Scale",
        "Progressive Wallet",
        "Magic Meter",
        "Ocarina",
        "Mirror Shield",
        "Iron Boots",
        "Hover Boots",
        "Dins Fire",
        "Farores Wind",
        "Nayrus Love",
    };

    uint64_t x = static_cast<uint64_t>(apLocation);
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;

    const char* fakeName =
        kMajorDisguises[x % (sizeof(kMajorDisguises) / sizeof(kMajorDisguises[0]))];
    RandomizerGet display = MapApItemNameToRandomizerGet(fakeName);
    return display != RG_NONE ? display : RG_PROGRESSIVE_HOOKSHOT;
}

static std::string GetApItemDisplayName(int64_t itemId) {
    switch (itemId) {
        case AP_ITEM_ROLL: return "Roll";
        case AP_ITEM_GRAB: return "Grab / Power Bracelet";
        case AP_ITEM_CLIMB: return "Climb";
        case AP_ITEM_CRAWL: return "Crawl";
        case AP_ITEM_SPEAK: return "Speak";
        case AP_ITEM_OPEN_CHEST: return "Open Chest";
        case AP_ITEM_ENEMY_SOUL: return "Enemy Soul";
        case AP_ITEM_NPC_SOUL: return "NPC Soul";
        case AP_ITEM_ANIMAL_SOUL: return "Animal Soul";
        case AP_ITEM_POT_SOUL: return "Pot Soul";
        case AP_ITEM_CRATE_SOUL: return "Crate Soul";
        case AP_ITEM_GRASS_SOUL: return "Grass / Bush Soul";
        case AP_ITEM_ROCK_SOUL: return "Rock / Boulder Soul";
        case AP_ITEM_TREE_SOUL: return "Tree Soul";
        case AP_ITEM_BEEHIVE_SOUL: return "Beehive Soul";
        case AP_ITEM_SIGN_SOUL: return "Sign Soul";
        case AP_ITEM_SKULLTULA_SOUL: return "Skulltula Soul";
        case AP_ITEM_BUSINESS_SCRUB_SOUL: return "Scrub Soul";
        case AP_ITEM_SHOVEL: return "Shovel";
        case AP_ITEM_FLOW_OF_TIME: return "Flow of Time";
        default: break;
    }

    if (itemId >= AP_FIRST_SONG_NOTE && itemId <= AP_LAST_SONG_NOTE) {
        return "Song Note " + std::to_string(static_cast<int>(itemId - AP_FIRST_SONG_NOTE) + 1);
    }

    if (itemId >= AP_BASE_ITEM_MIN && itemId <= AP_BASE_ITEM_MAX) {
        RandomizerGet randoGet = RG_NONE;
        switch (itemId) {
#include "ArchipelagoItemMap.inc"
            default: break;
        }
        if (randoGet != RG_NONE) {
            return Rando::StaticData::RetrieveItem(randoGet).GetName().english;
        }
    }

    return "Item " + std::to_string(itemId);
}

static bool BounceHasTag(const AP_Bounce& bounce, const std::string& tag) {
    if (bounce.tags == nullptr) return false;
    return std::find(bounce.tags->begin(), bounce.tags->end(), tag) != bounce.tags->end();
}

static std::string ExtractJsonString(const std::string& json, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    size_t pos = json.find(token);
    if (pos == std::string::npos) return {};
    pos = json.find(':', pos + token.size());
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    ++pos;
    std::string out;
    bool escaped = false;
    for (; pos < json.size(); ++pos) {
        const char c = json[pos];
        if (escaped) {
            switch (c) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: out.push_back(c); break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            break;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

static std::string EscapeJsonString(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}
}

ArchipelagoClient& ArchipelagoClient::GetInstance() {
    static ArchipelagoClient instance;
    return instance;
}

bool ArchipelagoClient::IsAuthenticated() const {
    return AP_IsInit() && AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated;
}

bool ArchipelagoClient::IsConnectionRefused() const {
    return AP_IsInit() && AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused;
}

std::string ArchipelagoClient::GetStatusText() const {
    if (!enabled.load()) {
        return "Disabled";
    }
    if (!AP_IsInit()) {
        return "Starting...";
    }
    switch (AP_GetConnectionStatus()) {
        // APCpp can report Disconnected briefly while AP_Start() is establishing the
        // websocket.  Treat that as an in-progress connection while this client is
        // enabled instead of showing the contradictory "Disconnect / Disconnected" UI.
        case AP_ConnectionStatus::Disconnected: return "Connecting...";
        case AP_ConnectionStatus::Connected: return "Connected - authenticating...";
        case AP_ConnectionStatus::Authenticated:
            if (!slotSettingsLoaded) return "Authenticated - loading AP settings...";
            if (!activeLocationsLoaded) return "Authenticated - loading AP locations...";
            if (!shopPricesLoaded) return "Authenticated - loading AP prices...";
            if (expectedScoutCount > 0 && scoutedLocations.size() < expectedScoutCount) {
                return "Authenticated - loading AP placements " + std::to_string(scoutedLocations.size()) + "/" +
                       std::to_string(expectedScoutCount);
            }
            return "Authenticated - ready";
        case AP_ConnectionStatus::ConnectionRefused: return "Connection refused - check server/slot/password";
        default: return "Connecting...";
    }
}

void ArchipelagoClient::RegisterCallbacks() {
    AP_SetLoggingCallback([](std::string line) { SPDLOG_INFO("[Archipelago] {}", line); });
    AP_SetItemClearCallback([]() {
        // APCpp calls this before replaying the complete ReceivedItems list.  Do NOT
        // clear SoH inventory here: the save already persists awarded items.  Instead
        // reset our receive ordinal so the full replay can be deduplicated against the
        // number of AP items this local save has already consumed.
        ArchipelagoClient::GetInstance().BeginItemReplay();
    });
    AP_SetItemRecvCallback([](int64_t item, bool notify) {
        ArchipelagoClient::GetInstance().QueueItem(item, notify);
    });
    AP_SetLocationCheckedCallback([](int64_t location) {
        ArchipelagoClient::GetInstance().QueueCheckedLocation(location);
    });
    AP_SetLocationInfoCallback([](std::vector<AP_NetworkItem> locations) {
        auto& client = ArchipelagoClient::GetInstance();
        for (const auto& item : locations) {
            client.QueueLocationInfo(item.location, item.item, item.player, item.flags, item.itemName, item.playerName,
                                     item.locationName);
        }
    });

    // Do not mutate gameplay CVars from APCpp's networking callback thread.
    // extreme_soh_cvars below is the authoritative snapshot; it is applied only
    // after the user starts/loads an AP randomizer save on the gameplay thread.

    // Raw Bounce handling is used for TrapLink. APCpp documents that registering a
    // Bounced callback disables its automatic DeathLink handling, so SOH-EXTREME
    // handles BOTH link protocols here and advertises the matching tags after auth.
    AP_RegisterSlotDataIntCallback("death_link", [](int value) {
        ArchipelagoClient::GetInstance().deathLinkEnabled = value != 0;
    });
    AP_RegisterSlotDataIntCallback("trap_link", [](int value) {
        ArchipelagoClient::GetInstance().trapLinkEnabled = value != 0;
    });
    AP_RegisterBouncedCallback([](AP_Bounce bounce) {
        auto& client = ArchipelagoClient::GetInstance();
        if (BounceHasTag(bounce, "DeathLink") && client.deathLinkEnabled) {
            client.QueueDeathLink(ExtractJsonString(bounce.data, "source"),
                                  ExtractJsonString(bounce.data, "cause"));
        }
        if (BounceHasTag(bounce, "TrapLink") && client.trapLinkEnabled) {
            client.QueueTrapLink(ExtractJsonString(bounce.data, "source"),
                                 ExtractJsonString(bounce.data, "trap_name"));
        }
    });

    // v0.4.5 publishes one authoritative snapshot already translated into this
    // fork's native gRando.Settings CVar names. It is intentionally raw JSON so all
    // settings arrive as one readiness boundary before file-select is allowed to start.
    AP_RegisterSlotDataRawCallback("extreme_soh_cvars", [](std::string raw) {
        ArchipelagoClient::GetInstance().SetSlotSettingsFromJson(raw);
    });
    AP_RegisterSlotDataRawCallback("extreme_shop_prices", [](std::string raw) {
        ArchipelagoClient::GetInstance().SetShopPricesFromJson(raw);
    });
    AP_RegisterSlotDataIntCallback("extreme_kakariko_gate_open", [](int value) {
        ArchipelagoClient::GetInstance().kakarikoGateOpen = value != 0;
    });

    // The AP server only allows LocationScouts for locations that actually exist in
    // this slot. SOH-EXTREME publishes that exact set in slot data. Scouting the entire
    // static SoH location table is invalid when options remove locations and causes the
    // AP 0.6.7 server to close the connection (for example: "No location 61 for player").
    AP_RegisterSlotDataRawCallback("extreme_active_locations", [](std::string raw) {
        ArchipelagoClient::GetInstance().SetActiveLocationsFromJson(raw);
    });

    // The old per-setting native callbacks were intentionally removed here.
    // extreme_soh_cvars is the single authoritative snapshot and includes enum
    // translations that cannot be represented by a direct 1:1 callback.

}

void ArchipelagoClient::Enable() {
    if (enabled.load()) {
        return;
    }

    // A refused/failed APCpp session can remain initialized even after the UI considers
    // it inactive.  Always tear down stale state before starting a fresh connection.
    if (AP_IsInit()) {
        AP_Shutdown();
    }

    const char* server = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("ServerAddress"), "archipelago.gg:38281");
    const char* slot = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "");
    const char* password = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("Password"), "");

    if (slot == nullptr || slot[0] == '\0') {
        SPDLOG_ERROR("[Archipelago] Slot name is empty");
        enabled = false;
        return;
    }

    // Reset per-connection state before APCpp callbacks can begin filling it.
    {
        std::scoped_lock lock(queueMutex);
        pendingItems.clear();
        pendingCheckedLocations.clear();
        pendingScouts.clear();
        pendingDeathLinks.clear();
        pendingTrapLinks.clear();
        chatMessages.clear();
    }
    scoutedLocations.clear();
    reportedLocations.clear();
    activeLocations.clear();
    slotSettings.clear();
    shopPrices.clear();
    activeLocationsLoaded = false;
    slotSettingsLoaded = false;
    shopPricesLoaded = false;
    kakarikoGateOpen = false;
    deathLinkEnabled = false;
    trapLinkEnabled = false;
    linkTagsSynchronized = false;
    deathStateInitialized = false;
    lastPlayerAlive = true;
    suppressNextDeathLinkSend = false;
    expectedScoutCount = 0;
    scoutsRequested = false;
    saveRuntimeSynchronized = false;
    fileSelectActivationRequested = false;
    wasAuthenticated = false;
    syncFrameCounter = 0;
    incomingItemOrdinal = 0;
    receivedItemSnapshot.clear();
    currentSaveIsArchipelago = false;
    saveMetadataLoaded = false;
    saveIdentityMismatch = false;
    saveServer.clear();
    saveSlot.clear();
    // Migration fallback for pre-0.5.9 saves.  A real AP save overrides this
    // with its own serialized receive count as soon as SaveManager loads it.
    appliedItemCount = static_cast<uint64_t>(std::max(0, CVarGetInteger(GetReceivedCountCVar().c_str(), 0)));
    SPDLOG_INFO("[Archipelago] Loaded persisted received-item count {}", appliedItemCount);

    enabled = true;
    AP_Init(server, "SOH-EXTREME", slot, password == nullptr ? "" : password);
    AP_NetworkVersion version{ 0, 6, 7 };
    AP_SetClientVersion(&version);
    RegisterCallbacks();
    AP_EnableQueueItemRecvMsgs(true);
    AP_Start();
    SPDLOG_INFO("[Archipelago] Starting connection to {} as {}", server, slot);
}

void ArchipelagoClient::Disable() {
    // Shutdown should be safe even if our UI state and APCpp state got out of sync.
    enabled = false;
    if (AP_IsInit()) {
        AP_Shutdown();
    }
    std::scoped_lock lock(queueMutex);
    pendingItems.clear();
    pendingCheckedLocations.clear();
    pendingScouts.clear();
    pendingDeathLinks.clear();
    pendingTrapLinks.clear();
    scoutedLocations.clear();
    activeLocations.clear();
    slotSettings.clear();
    shopPrices.clear();
    activeLocationsLoaded = false;
    slotSettingsLoaded = false;
    shopPricesLoaded = false;
    kakarikoGateOpen = false;
    deathLinkEnabled = false;
    trapLinkEnabled = false;
    linkTagsSynchronized = false;
    deathStateInitialized = false;
    lastPlayerAlive = true;
    suppressNextDeathLinkSend = false;
    expectedScoutCount = 0;
    scoutsRequested = false;
    saveRuntimeSynchronized = false;
    fileSelectActivationRequested = false;
    currentSaveIsArchipelago = false;
    saveMetadataLoaded = false;
    saveIdentityMismatch = false;
    saveServer.clear();
    saveSlot.clear();
    chatMessages.clear();
}

void ArchipelagoClient::Toggle() {
    if (IsEnabled()) Disable(); else Enable();
}

std::string ArchipelagoClient::GetReceivedCountCVar() const {
    const char* server = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("ServerAddress"), "");
    const char* slot = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "");
    std::string identity = std::string(server ? server : "") + "_" + std::string(slot ? slot : "");
    for (char& c : identity) {
        if (!std::isalnum(static_cast<unsigned char>(c))) c = '_';
    }
    return std::string("gNetwork.Archipelago.ReceivedItemCount.") + identity;
}

void ArchipelagoClient::LoadSaveMetadata(bool isArchipelagoSave, uint64_t receivedItemCount,
                                               const std::string& server, const std::string& slot,
                                               const std::string& cachedSettingsJson) {
    std::scoped_lock lock(queueMutex);

    currentSaveIsArchipelago = isArchipelagoSave;
    saveMetadataLoaded = true;
    saveServer = server;
    saveSlot = slot;
    cachedSlotSettingsJson = cachedSettingsJson;

    // 0.7.39: Restore the last server-authoritative settings snapshot from THIS save
    // before the first gameplay scene creates its actors.  Reconnecting to AP happens
    // asynchronously, which is too late for ShouldActorInit-based systems such as Pot
    // Soul, Grass/Bush Soul, Enemy Soul, etc.  The live server snapshot will replace
    // this cache as soon as Connected slot_data arrives.
    if (isArchipelagoSave && !cachedSettingsJson.empty()) {
        std::unordered_map<std::string, int64_t> parsed;
        if (ParseFlatStringIntObject(cachedSettingsJson, parsed)) {
            slotSettings.clear();
            for (const auto& [key, value] : parsed) {
                slotSettings[key] = static_cast<int>(value);
            }
            slotSettingsLoaded = true;
            ApplySlotSettings();
            SPDLOG_INFO("[Archipelago] Restored {} cached AP settings from save before scene init", slotSettings.size());
        } else {
            SPDLOG_WARN("[Archipelago] Ignoring invalid cached AP settings stored in save");
        }
    }
    saveIdentityMismatch = false;

    if (!isArchipelagoSave) {
        return;
    }

    const char* configuredServerRaw = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("ServerAddress"), "");
    const char* configuredSlotRaw = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "");
    const std::string configuredServer = configuredServerRaw ? configuredServerRaw : "";
    const std::string configuredSlot = configuredSlotRaw ? configuredSlotRaw : "";

    // Slot is the strongest identity we can safely require here.  Server is also
    // checked when both sides have one, but an old migrated save may not have it.
    if ((!slot.empty() && !configuredSlot.empty() && slot != configuredSlot) ||
        (!server.empty() && !configuredServer.empty() && server != configuredServer)) {
        saveIdentityMismatch = true;
        pendingItems.clear();
        SPDLOG_ERROR("[Archipelago] Save belongs to AP slot '{}' at '{}', but client is configured for '{}' at '{}'; "
                     "item replay is blocked to protect the save",
                     slot, server, configuredSlot, configuredServer);
        return;
    }

    appliedItemCount = receivedItemCount;

    // APCpp may already have replayed ReceivedItems while the user was on file select.
    // Rebuild the pending queue against the count stored *inside this save*.
    pendingItems.clear();
    for (uint64_t i = appliedItemCount; i < receivedItemSnapshot.size(); ++i) {
        pendingItems.push_back({ receivedItemSnapshot[i], false, i });
    }

    SPDLOG_INFO("[Archipelago] Loaded AP save metadata: slot='{}', receive count={}, queued {} newer items",
                saveSlot, appliedItemCount, pendingItems.size());
}

void ArchipelagoClient::BeginItemReplay() {
    std::scoped_lock lock(queueMutex);
    incomingItemOrdinal = 0;
    receivedItemSnapshot.clear();
    SPDLOG_INFO("[Archipelago] Beginning server item-state replay; {} AP items already applied locally",
                appliedItemCount);
}

void ArchipelagoClient::QueueItem(int64_t itemId, bool notify) {
    std::scoped_lock lock(queueMutex);

    const uint64_t sequence = incomingItemOrdinal++;
    receivedItemSnapshot.push_back(itemId);

    // APCpp always invokes the item callback for the complete ReceivedItems replay,
    // including items the player already owns.  Its notify flag is not a reliable
    // persistence boundary across process restarts, so use our own monotonically
    // increasing receive count instead.  This is what prevents a starting item from
    // being granted again forever on reconnect/startup.
    if (sequence < appliedItemCount) {
        SPDLOG_DEBUG("[Archipelago] Replay item #{} id {} already applied; skipping", sequence, itemId);
        return;
    }

    const std::string itemName = GetApItemDisplayName(itemId);
    SPDLOG_INFO("[Archipelago] Received {} (item id {}, receive #{}, notify={})", itemName, itemId, sequence, notify);
    pendingItems.push_back({ itemId, notify, sequence });
}

void ArchipelagoClient::MarkItemApplied(uint64_t sequence) {
    const uint64_t newCount = sequence + 1;
    if (newCount <= appliedItemCount) return;
    appliedItemCount = newCount;
    // Keep the old CVar as a migration mirror, but SaveManager's per-save field is
    // authoritative from 0.5.9 onward.
    CVarSetInteger(GetReceivedCountCVar().c_str(), static_cast<int>(appliedItemCount));
    SPDLOG_INFO("[Archipelago] Applied receive #{}; current save receive count is now {}", sequence, appliedItemCount);
}

void ArchipelagoClient::PrepareNewSaveItemReplay() {
    fallbackNpcSpeechHashes.clear();
    fallbackNpcSpeechSeen.clear();

    std::scoped_lock lock(queueMutex);

    // A deliberately-created new AP save starts from a clean SoH inventory, so replay
    // the server's current item history exactly once into that new save.  This also
    // means deleting/recreating a local file does not permanently lose AP starting
    // inventory just because the server remembers that it was delivered before.
    appliedItemCount = 0;
    currentSaveIsArchipelago = true;
    saveMetadataLoaded = true;
    saveIdentityMismatch = false;
    const char* serverRaw = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("ServerAddress"), "");
    const char* slotRaw = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "");
    saveServer = serverRaw ? serverRaw : "";
    saveSlot = slotRaw ? slotRaw : "";
    CVarSetInteger(GetReceivedCountCVar().c_str(), 0); // legacy migration mirror only
    pendingItems.clear();
    for (uint64_t i = 0; i < receivedItemSnapshot.size(); ++i) {
        pendingItems.push_back({ receivedItemSnapshot[i], i >= appliedItemCount, i });
    }
    SPDLOG_INFO("[Archipelago] New AP save: queued {} historical received items for one-time reconstruction",
                pendingItems.size());
}

void ArchipelagoClient::QueueCheckedLocation(int64_t locationId) {
    std::scoped_lock lock(queueMutex);
    pendingCheckedLocations.push_back(locationId);
}

void ArchipelagoClient::QueueLocationInfo(int64_t locationId, int64_t itemId, int playerId, int flags,
                                          const std::string& itemName, const std::string& playerName,
                                          const std::string& locationName) {
    std::scoped_lock lock(queueMutex);
    PendingScout scout;
    scout.locationId = locationId;
    scout.info.itemId = itemId;
    scout.info.playerId = playerId;
    scout.info.flags = flags;
    scout.info.itemName = itemName;
    scout.info.playerName = playerName;
    scout.info.locationName = locationName;
    pendingScouts.push_back(std::move(scout));
}

int32_t ArchipelagoClient::MapApItemToRandomizerGet(int64_t itemId) const {
    // ArchipelagoItemMap.inc is an assignment table.  It expects a local
    // variable named `randoGet`; it is shared with ProcessItem below.
    RandomizerGet randoGet = RG_NONE;

    if (itemId >= AP_BASE_ITEM_MIN && itemId <= AP_BASE_ITEM_MAX) {
        switch (itemId) {
#include "ArchipelagoItemMap.inc"
            default: break;
        }
        return static_cast<int32_t>(randoGet);
    }

    switch (itemId) {
        case AP_ITEM_ROLL: randoGet = RG_ROLL; break;
        case AP_ITEM_GRAB: randoGet = RG_POWER_BRACELET; break;
        case AP_ITEM_CLIMB: randoGet = RG_CLIMB; break;
        case AP_ITEM_CRAWL: randoGet = RG_CRAWL; break;
        case AP_ITEM_SPEAK: randoGet = RG_NPC_SOUL; break; // visual stand-in for flag-only Speak
        case AP_ITEM_OPEN_CHEST: randoGet = RG_OPEN_CHEST; break;
        case AP_ITEM_ENEMY_SOUL: randoGet = RG_ENEMY_SOUL; break;
        case AP_ITEM_NPC_SOUL: randoGet = RG_NPC_SOUL; break;
        case AP_ITEM_ANIMAL_SOUL: randoGet = RG_ANIMAL_SOUL; break;
        case AP_ITEM_POT_SOUL: randoGet = RG_POT_SOUL; break;
        case AP_ITEM_CRATE_SOUL: randoGet = RG_CRATE_SOUL; break;
        case AP_ITEM_GRASS_SOUL: randoGet = RG_GRASS_SOUL; break;
        case AP_ITEM_ROCK_SOUL: randoGet = RG_ROCK_SOUL; break;
        case AP_ITEM_TREE_SOUL: randoGet = RG_TREE_SOUL; break;
        case AP_ITEM_BEEHIVE_SOUL: randoGet = RG_BEEHIVE_SOUL; break;
        case AP_ITEM_SIGN_SOUL: randoGet = RG_SIGN_SOUL; break;
        case AP_ITEM_SKULLTULA_SOUL: randoGet = RG_SKULLTULA_SOUL; break;
        case AP_ITEM_BUSINESS_SCRUB_SOUL: randoGet = RG_BUSINESS_SCRUB_SOUL; break;
        case AP_ITEM_SHOVEL: randoGet = RG_SHOVEL; break;
        case AP_ITEM_FLOW_OF_TIME: randoGet = RG_SUNS_SONG; break; // time-themed visual
        default:
            if (itemId >= AP_FIRST_SONG_NOTE && itemId <= AP_LAST_SONG_NOTE) {
                randoGet = RG_SONG_OF_TIME; // note-family visual; reward remains exact note flag
            }
            break;
    }

    return static_cast<int32_t>(randoGet);
}

static bool ParseFlatStringIntObject(const std::string& raw, std::unordered_map<std::string, int64_t>& out) {
    out.clear();
    size_t i = 0;
    auto skipWs = [&]() { while (i < raw.size() && std::isspace(static_cast<unsigned char>(raw[i]))) ++i; };
    skipWs();
    if (i >= raw.size() || raw[i++] != '{') return false;
    for (;;) {
        skipWs();
        if (i < raw.size() && raw[i] == '}') { ++i; break; }
        if (i >= raw.size() || raw[i++] != '"') return false;
        std::string key;
        while (i < raw.size() && raw[i] != '"') {
            if (raw[i] == '\\' && i + 1 < raw.size()) ++i;
            key.push_back(raw[i++]);
        }
        if (i >= raw.size() || raw[i++] != '"') return false;
        skipWs();
        if (i >= raw.size() || raw[i++] != ':') return false;
        skipWs();
        bool negative = false;
        if (i < raw.size() && raw[i] == '-') { negative = true; ++i; }
        if (i >= raw.size() || !std::isdigit(static_cast<unsigned char>(raw[i]))) return false;
        int64_t value = 0;
        while (i < raw.size() && std::isdigit(static_cast<unsigned char>(raw[i]))) {
            value = value * 10 + (raw[i++] - '0');
        }
        out[key] = negative ? -value : value;
        skipWs();
        if (i < raw.size() && raw[i] == ',') { ++i; continue; }
        if (i < raw.size() && raw[i] == '}') { ++i; break; }
        return false;
    }
    skipWs();
    return i == raw.size();
}

static void RefreshArchipelagoRandomizerHooks() {
    // IMPORTANT: use Ship's own native randomizer refresh path instead of trying
    // to maintain a hand-written list of shuffle modules here.  Normal SoH calls
    // this exact dependency path from the randomizer OnLoadGame hook after
    // IS_RANDO becomes true.  AP changes the quest/settings outside the normal
    // randomizer menu, so without this call modules that were disabled at boot
    // keep their hooks unregistered (MegaSouls was the most visible example: a
    // Pot Soul slot could be ON while pots still spawned normally).
    //
    // Running the complete IS_RANDO dependency makes AP behave the same as a
    // normally configured SoH randomizer save and automatically includes future
    // modules that register with RegisterShipInitFunc(..., { "IS_RANDO" }).
    ShipInit::Init("IS_RANDO");
    SPDLOG_INFO("[Archipelago] Refreshed ALL native IS_RANDO hooks from authoritative AP settings");
}

void ArchipelagoClient::SetSlotSettingsFromJson(const std::string& raw) {
    std::unordered_map<std::string, int64_t> parsed;
    if (!ParseFlatStringIntObject(raw, parsed)) {
        SPDLOG_ERROR("[Archipelago] Invalid extreme_soh_cvars slot data");
        return;
    }
    slotSettings.clear();
    for (const auto& [key, value] : parsed) {
        slotSettings[key] = static_cast<int>(value);
    }
    cachedSlotSettingsJson = raw;
    slotSettingsLoaded = true;
    // Keep the randomizer menu/display synchronized immediately after connection too.
    // File creation calls ApplySlotSettings again immediately before init as a hard barrier.
    ApplySlotSettings();
    SPDLOG_INFO("[Archipelago] Loaded {} authoritative SoH/Extreme settings", slotSettings.size());
}

void ArchipelagoClient::SetShopPricesFromJson(const std::string& raw) {
    std::unordered_map<std::string, int64_t> parsed;
    if (!ParseFlatStringIntObject(raw, parsed)) {
        SPDLOG_ERROR("[Archipelago] Invalid extreme_shop_prices slot data");
        return;
    }
    shopPrices.clear();
    for (const auto& [key, value] : parsed) {
        try {
            const int64_t location = std::stoll(key);
            shopPrices[location] = static_cast<uint16_t>(std::clamp<int64_t>(value, 0, 65535));
        } catch (...) {
            SPDLOG_WARN("[Archipelago] Ignoring malformed shop location id {}", key);
        }
    }
    shopPricesLoaded = true;
    SPDLOG_INFO("[Archipelago] Loaded {} exact AP shop prices", shopPrices.size());
}

void ArchipelagoClient::ApplySlotSettings() {
    if (!slotSettingsLoaded) {
        return;
    }

    SPDLOG_INFO("[Archipelago] SOH-EXTREME AP settings runtime 0.7.45 active");

    // APWorld sends extreme_soh_cvars using the *native option suffixes*
    // (ShufflePots, ShuffleGrass, PotSoul, etc.).  Do NOT reconstruct the CVar
    // prefix here.  Ship's Option table is the authority for the exact CVar name
    // consumed by Option::GetOptionIndex()/RAND_GET_OPTION().
    //
    // This fixes the failure where AP successfully wrote/read 223 synthetic CVars,
    // but the real Randomizer Options still read 0.  A normal SoH randomizer menu
    // worked because it writes Option::GetCVarName() directly.
    auto settings = Rando::Settings::GetInstance();
    if (!settings) {
        SPDLOG_ERROR("[Archipelago] Randomizer Settings singleton unavailable; cannot apply AP settings");
        return;
    }

    size_t mappedNativeOptions = 0;
    size_t nativeReadbackMismatches = 0;
    std::set<std::string> consumedKeys;

    for (const auto& option : settings->GetAllOptions()) {
        const std::string& nativeCVar = option.GetCVarName();
        if (nativeCVar.empty()) {
            continue;
        }

        // AP keys are the final component of the native CVar, e.g.
        // gRandoSettings.ShufflePots -> ShufflePots.
        const size_t dot = nativeCVar.find_last_of('.');
        const std::string nativeKey = dot == std::string::npos ? nativeCVar : nativeCVar.substr(dot + 1);
        auto it = slotSettings.find(nativeKey);
        if (it == slotSettings.end()) {
            continue;
        }

        const int requested = it->second;
        CVarSetInteger(nativeCVar.c_str(), requested);
        consumedKeys.insert(nativeKey);
        ++mappedNativeOptions;

        const int readBack = CVarGetInteger(nativeCVar.c_str(), requested - 1);
        if (readBack != requested) {
            ++nativeReadbackMismatches;
            SPDLOG_ERROR("[Archipelago] Native option CVar write failed: {} (AP key {}) requested={} readback={}",
                         nativeCVar, nativeKey, requested, readBack);
        }
    }

    // Preserve AP-only/custom CVars that are not represented by Settings::mOptions.
    // These are intentionally secondary; gameplay randomizer settings above always
    // use the exact CVar name from Ship's native Option table.
    for (const auto& [key, value] : slotSettings) {
        if (consumedKeys.contains(key)) {
            continue;
        }
        const std::string fallbackCVar = std::string("gRandoSettings.") + key;
        CVarSetInteger(fallbackCVar.c_str(), value);
    }

    // Callbacks may change visibility/availability, then copy the now-correct native
    // menu indices into the live Context used by RAND_GET_OPTION.
    settings->UpdateAllOptions();
    settings->SetAllToContext();

    if (IS_RANDO) {
        RefreshArchipelagoRandomizerHooks();
    }

    auto slotValue = [this](const char* key, int fallback = 0) {
        auto it = slotSettings.find(key);
        return it == slotSettings.end() ? fallback : it->second;
    };
    const int trapPool = slotValue("ExtremeTrapPool");
    const bool expanded = trapPool == 2;
    CVarSetInteger("gEnhancements.ExtraTraps.Enabled", trapPool != 0);
    CVarSetInteger("gEnhancements.ExtraTraps.Ice", trapPool == 1 || (expanded && slotValue("ExtremeIceTraps")));
    CVarSetInteger("gEnhancements.ExtraTraps.Burn", expanded && slotValue("ExtremeFireTraps"));
    CVarSetInteger("gEnhancements.ExtraTraps.Speed", expanded && slotValue("ExtremeSlowTraps"));
    CVarSetInteger("gEnhancements.ExtraTraps.Magic", expanded && slotValue("ExtremeMagicSuckTraps"));
    CVarSetInteger("gEnhancements.ExtraTraps.Health", expanded && slotValue("ExtremeHealthDrainTraps"));
    CVarSetInteger("gEnhancements.ExtraTraps.Shock", 0);
    CVarSetInteger("gEnhancements.ExtraTraps.Knockback", 0);
    CVarSetInteger("gEnhancements.ExtraTraps.Bomb", 0);
    CVarSetInteger("gEnhancements.ExtraTraps.Void", 0);
    CVarSetInteger("gEnhancements.ExtraTraps.Ammo", 0);
    CVarSetInteger("gEnhancements.ExtraTraps.Kill", 0);
    CVarSetInteger("gEnhancements.ExtraTraps.Teleport", 0);

    // Log AP input AND the live RSK result.  If these differ now, the log tells us
    // which exact layer is wrong instead of merely proving a synthetic CVar exists.
    SPDLOG_INFO(
        "[Archipelago] AP native settings: mapped={}/{} readback_mismatches={} | "
        "Pots AP={} RSK={} PotSoul AP={} RSK={} Grass AP={} RSK={} GrassSoul AP={} RSK={} "
        "Rocks AP={} RSK={} RockSoul AP={} RSK={} Crates AP={} RSK={} CrateSoul AP={} RSK={} "
        "Speak AP={} RSK={} NPCSpeech AP={} RSK={} FlowOfTime AP={} RSK={}",
        mappedNativeOptions, slotSettings.size(), nativeReadbackMismatches,
        slotValue("ShufflePots"), RAND_GET_OPTION(RSK_SHUFFLE_POTS).Get(),
        slotValue("PotSoul"), RAND_GET_OPTION(RSK_SHUFFLE_POT_SOUL).Get(),
        slotValue("ShuffleGrass"), RAND_GET_OPTION(RSK_SHUFFLE_GRASS).Get(),
        slotValue("GrassSoul"), RAND_GET_OPTION(RSK_SHUFFLE_GRASS_SOUL).Get(),
        slotValue("ShuffleRocks"), RAND_GET_OPTION(RSK_SHUFFLE_ROCKS).Get(),
        slotValue("RockSoul"), RAND_GET_OPTION(RSK_SHUFFLE_ROCK_SOUL).Get(),
        slotValue("ShuffleCrates"), RAND_GET_OPTION(RSK_SHUFFLE_CRATES).Get(),
        slotValue("CrateSoul"), RAND_GET_OPTION(RSK_SHUFFLE_CRATE_SOUL).Get(),
        slotValue("ShuffleSpeak"), RAND_GET_OPTION(RSK_SHUFFLE_SPEAK).Get(),
        slotValue("NPCSpeechSanity"), RAND_GET_OPTION(RSK_NPC_SPEECH_SANITY).Get(),
        slotValue("FlowOfTime"), RAND_GET_OPTION(RSK_SHUFFLE_FLOW_OF_TIME).Get());
}

void ArchipelagoClient::EnforceSlotSettings() {
    if (!slotSettingsLoaded || !currentSaveIsArchipelago || gSaveContext.ship.quest.id != QUEST_RANDOMIZER) {
        return;
    }

    // AP owns randomizer settings for an AP save. The local randomizer menu is not
    // allowed to silently change physical checks/logic after connection. If a user
    // changes one of those CVars locally, restore the server value and refresh the
    // affected hooks before the next gameplay update.
    auto settings = Rando::Settings::GetInstance();
    if (!settings) return;
    for (const auto& option : settings->GetAllOptions()) {
        const std::string& nativeCVar = option.GetCVarName();
        if (nativeCVar.empty()) continue;
        const size_t dot = nativeCVar.find_last_of('.');
        const std::string key = dot == std::string::npos ? nativeCVar : nativeCVar.substr(dot + 1);
        auto it = slotSettings.find(key);
        if (it == slotSettings.end()) continue;
        const int actual = CVarGetInteger(nativeCVar.c_str(), it->second);
        if (actual != it->second) {
            SPDLOG_WARN("[Archipelago] Native randomizer setting {}={} disagrees with AP {}; restoring server snapshot",
                        nativeCVar, actual, it->second);
            ApplySlotSettings();
            return;
        }
    }
}

bool ArchipelagoClient::IsReadyForFileSelect() const {
    if (!IsAuthenticated() || !slotSettingsLoaded || !activeLocationsLoaded || !shopPricesLoaded) return false;
    if (expectedScoutCount == 0) return activeLocations.empty();
    return scoutedLocations.size() >= expectedScoutCount;
}

void ArchipelagoClient::ApplyPostInitSlotState() {
    // Kakariko Gate was removed as a normal setting in this SoH fork. AP still has
    // that setting, so apply its resolved open state after save initialization.
    if (kakarikoGateOpen) {
        Flags_SetInfTable(INFTABLE_SHOWED_ZELDAS_LETTER_TO_GATE_GUARD);
    }
}

void ArchipelagoClient::SetActiveLocationsFromJson(const std::string& raw) {
    // APCpp gives raw slot data as JSON text.  This value is deliberately just a flat
    // array of positive integer location IDs, so do not pull jsoncpp into soh.exe only
    // to parse it.  jsoncpp in APCpp is built /MD while SoH is /MT, which causes
    // LNK2038 RuntimeLibrary mismatches on MSVC.  Scanning the integers here keeps the
    // Archipelago DLL boundary clean and avoids any extra CRT dependency.
    std::unordered_set<int64_t> parsed;
    int64_t value = 0;
    bool inNumber = false;

    for (char c : raw) {
        if (c >= '0' && c <= '9') {
            inNumber = true;
            value = (value * 10) + static_cast<int64_t>(c - '0');
            continue;
        }

        if (inNumber) {
            parsed.insert(value);
            value = 0;
            inNumber = false;
        }
    }
    if (inNumber) {
        parsed.insert(value);
    }

    // An empty array is valid (for example, a pathological option combination), but
    // malformed non-array slot data should not be treated as successfully loaded.
    const auto first = raw.find_first_not_of(" \t\r\n");
    const auto last = raw.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos ||
        raw[first] != '[' || raw[last] != ']') {
        SPDLOG_ERROR("[Archipelago] Invalid extreme_active_locations slot data: {}", raw);
        return;
    }

    activeLocations = std::move(parsed);
    activeLocationsLoaded = true;
    scoutsRequested = false;
    scoutedLocations.clear();
    SPDLOG_INFO("[Archipelago] Loaded {} active server locations from slot data", activeLocations.size());
}

void ArchipelagoClient::RequestLocationScouts() {
    if (!IsAuthenticated() || scoutsRequested) return;

    // Never scout the full static map. Archipelago validates LocationScouts against the
    // current slot, and many SoH checks do not exist for every option combination.
    if (!activeLocationsLoaded) {
        SPDLOG_DEBUG("[Archipelago] Waiting for extreme_active_locations slot data before scouting");
        return;
    }

    // extreme_active_locations already came from the server-generated APWorld, so every
    // ID in it is valid for this slot. Scout the COMPLETE active set, not only locations
    // that happen to exist in the baked rcToApLocation table. The latter was missing a
    // large number of stock SoH freestanding checks (for example KF Behind Mido's House
    // Rupee), which made them impossible to report or receive placements for.
    std::set<int64_t> locations(activeLocations.begin(), activeLocations.end());

    if (locations.empty()) {
        expectedScoutCount = 0;
        scoutsRequested = true;
        return;
    }

    expectedScoutCount = locations.size();
    scoutsRequested = true;
    SPDLOG_INFO("[Archipelago] Scouting all {} ACTIVE SOH-EXTREME locations in chunks", locations.size());

    // Large all-sanity slots can exceed 2,000 locations.  Split scout requests
    // into modest packets instead of asking APCpp/server to process one giant
    // set in a single callback burst.
    constexpr size_t kScoutChunkSize = 256;
    std::set<int64_t> chunk;
    for (int64_t locationId : locations) {
        chunk.insert(locationId);
        if (chunk.size() >= kScoutChunkSize) {
            AP_SendLocationScouts(chunk, false);
            chunk.clear();
        }
    }
    if (!chunk.empty()) {
        AP_SendLocationScouts(chunk, false);
    }
}

int64_t ArchipelagoClient::ResolveApLocationForCheck(int32_t randomizerCheck) {
    auto direct = rcToApLocation.find(randomizerCheck);
    if (direct != rcToApLocation.end()) {
        return direct->second;
    }

    // Missing static mappings are resolved lazily for the one concrete RC that
    // the game is currently touching.  Do NOT scan every RandomizerCheck at
    // runtime: some table entries are intentionally sparse/conditional, and a
    // full RC_MAX walk added unnecessary crash risk during scout processing.
    auto* location = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(randomizerCheck));
    if (location == nullptr) {
        return -1;
    }

    auto normalize = [](const std::string& value) {
        std::string out;
        out.reserve(value.size());
        for (unsigned char c : value) {
            if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
        }
        return out;
    };

    const std::string fullName = normalize(location->GetName());
    const std::string shortName = normalize(location->GetShortName());
    if (fullName.empty() && shortName.empty()) {
        return -1;
    }

    int64_t matched = -1;
    for (const auto& [apLocation, info] : scoutedLocations) {
        const std::string apName = normalize(info.locationName);
        if (apName.empty()) continue;

        bool match = (!fullName.empty() && apName == fullName);
        if (!match && !shortName.empty() && shortName.size() <= apName.size()) {
            match = apName.compare(apName.size() - shortName.size(), shortName.size(), shortName) == 0;
        }
        if (!match) continue;

        // Never guess when two AP locations normalize to the same candidate.
        if (matched != -1 && matched != apLocation) {
            SPDLOG_WARN("[Archipelago] Ambiguous lazy RC mapping for {} ({})", randomizerCheck, location->GetName());
            return -1;
        }
        matched = apLocation;
    }

    if (matched == -1) {
        return -1;
    }

    rcToApLocation[randomizerCheck] = matched;
    apLocationToRc[matched] = randomizerCheck;
    SPDLOG_INFO("[Archipelago] Lazily resolved missing RC mapping: {} -> {} ({})",
                randomizerCheck, matched, location->GetName());
    return matched;
}

std::string ArchipelagoClient::GetRemoteItemDescription(int32_t randomizerCheck) {
    if (!IsAuthenticated() || scoutedLocations.empty()) return {};
    const int64_t apLocation = ResolveApLocationForCheck(randomizerCheck);
    if (apLocation < 0) return {};
    auto scoutIt = scoutedLocations.find(apLocation);
    if (scoutIt == scoutedLocations.end()) return {};
    const auto& info = scoutIt->second;
    if (info.playerId == AP_GetPlayerID()) return {};

    std::string result = info.itemName.empty() ? "Archipelago Item" : info.itemName;
    result += " for ";
    if (!info.playerName.empty()) {
        result += info.playerName;
    } else if (info.playerId > 0) {
        result += "Player " + std::to_string(info.playerId);
    } else {
        result += "another player";
    }
    result += "'s world";
    return result;
}

void ArchipelagoClient::EnsureLocationScouts() {
    RequestLocationScouts();
}
void ArchipelagoClient::RefreshPlacementForCheck(int32_t randomizerCheck) {
    // Actor draw/drop code can ask for a GetItemEntry long after the initial AP
    // placement reconciliation.  Always make the scouted AP placement authoritative
    // immediately before SoH chooses a model.  This fixes stale native models for
    // freestanding items, shops, and item drops spawned from grass/rocks/etc.
    if (!IsEnabled() || !IsAuthenticated() || scoutedLocations.empty()) return;

    auto ctx = Rando::Context::GetInstance();
    if (!ctx) return;

    const int64_t apLocation = ResolveApLocationForCheck(randomizerCheck);
    if (apLocation < 0) return;

    auto scoutIt = scoutedLocations.find(apLocation);
    if (scoutIt == scoutedLocations.end()) return;

    auto* loc = ctx->GetItemLocation(static_cast<RandomizerCheck>(randomizerCheck));
    if (loc == nullptr || loc->HasObtained()) return;

    auto priceIt = shopPrices.find(apLocation);
    if (priceIt != shopPrices.end()) {
        loc->SetPrice(priceIt->second);
    }

    const auto& info = scoutIt->second;
    RandomizerGet display = RG_NONE;
    if (info.playerId == AP_GetPlayerID()) {
        display = MapApItemNameToRandomizerGet(info.itemName);
        if (display == RG_NONE) {
            display = static_cast<RandomizerGet>(MapApItemToRandomizerGet(info.itemId));
        }
        if (display == RG_ICE_TRAP || info.itemName == "Ice Trap") {
            display = GetIceTrapDisguise(apLocation);
        }
    } else {
        display = GetRemoteArchipelagoDisplay(info.flags);
    }

    if (display == RG_NONE) {
        display = GetRemoteArchipelagoDisplay(info.flags);
    }

    if (loc->GetPlacedRandomizerGet() != display) {
        SPDLOG_INFO("[Archipelago] Live model refresh RC {} / {} <- {} (AP item {}, RandomizerGet {}, recipient {})",
                    randomizerCheck, info.locationName, info.itemName, info.itemId, static_cast<int>(display),
                    info.playerName);
        loc->SetPlacedItem(display);
    }
}

void ArchipelagoClient::ApplyScoutedPlacements() {
    auto ctx = Rando::Context::GetInstance();
    if (!ctx || scoutedLocations.empty()) return;

    const int selfPlayer = AP_GetPlayerID();
    for (const auto& [apLocation, info] : scoutedLocations) {
        auto rcIt = apLocationToRc.find(apLocation);
        if (rcIt == apLocationToRc.end()) {
            // A number of freestanding/shop sanity checks do not have a static
            // ArchipelagoLocationMap.inc entry.  If we skip them here, the actor
            // caches the native seed's stale model even though AP owns the check.
            // Reconcile by the same conservative full/short-name matching used by
            // ResolveApLocationForCheck, but only across locations active in this
            // initialized native seed.
            auto normalize = [](const std::string& value) {
                std::string out;
                out.reserve(value.size());
                for (unsigned char c : value) {
                    if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
                }
                return out;
            };

            const std::string apName = normalize(info.locationName);
            int matchedRc = -1;
            if (!apName.empty()) {
                for (RandomizerCheck candidateRc : ctx->allLocations) {
                    auto mapped = rcToApLocation.find(static_cast<int32_t>(candidateRc));
                    if (mapped != rcToApLocation.end() && mapped->second != apLocation) continue;

                    auto* candidate = Rando::StaticData::GetLocation(candidateRc);
                    if (candidate == nullptr) continue;
                    const std::string fullName = normalize(candidate->GetName());
                    const std::string shortName = normalize(candidate->GetShortName());
                    bool match = !fullName.empty() && apName == fullName;
                    if (!match && !shortName.empty() && shortName.size() <= apName.size()) {
                        match = apName.compare(apName.size() - shortName.size(), shortName.size(), shortName) == 0;
                    }
                    if (!match) continue;

                    if (matchedRc != -1 && matchedRc != static_cast<int>(candidateRc)) {
                        matchedRc = -2; // ambiguous: never guess a visual placement
                        break;
                    }
                    matchedRc = static_cast<int>(candidateRc);
                }
            }

            if (matchedRc >= 0) {
                rcToApLocation[matchedRc] = apLocation;
                apLocationToRc[apLocation] = matchedRc;
                rcIt = apLocationToRc.find(apLocation);
                SPDLOG_INFO("[Archipelago] Placement/model mapping resolved by name: RC {} -> {} ({})",
                            matchedRc, apLocation, info.locationName);
            } else {
                SPDLOG_DEBUG("[Archipelago] Scouted location {} ({}) has no unambiguous local RC mapping", apLocation,
                             info.locationName);
                continue;
            }
        }

        auto* loc = ctx->GetItemLocation(static_cast<RandomizerCheck>(rcIt->second));
        if (loc == nullptr) continue;

        auto priceIt = shopPrices.find(apLocation);
        if (priceIt != shopPrices.end()) {
            loc->SetPrice(priceIt->second);
        }
        if (loc->HasObtained()) continue;

        RandomizerGet display = RG_NONE;
        if (info.playerId == selfPlayer) {
            // Match the working Shipwright reference: prefer the AP item name when
            // converting a scouted placement into SoH's native RandomizerGet table.
            // SOH-EXTREME numeric IDs remain a fallback for custom/newer items.
            display = MapApItemNameToRandomizerGet(info.itemName);
            if (display == RG_NONE) {
                display = static_cast<RandomizerGet>(MapApItemToRandomizerGet(info.itemId));
            }
            if (display == RG_ICE_TRAP || info.itemName == "Ice Trap") {
                display = GetIceTrapDisguise(apLocation);
            }
        } else {
            // A location containing another player's item cannot use that game's native model.
            // Show the Archipelago logo instead. Progression/useful AP items are colored;
            // filler/junk/trap-only items use the grayscale Archipelago logo.
            display = GetRemoteArchipelagoDisplay(info.flags);
        }

        if (display == RG_NONE) {
            // Unknown same-slot/custom items should also avoid the unrelated Triforce model.
            display = GetRemoteArchipelagoDisplay(info.flags);
        }

        if (loc->GetPlacedRandomizerGet() != display) {
            SPDLOG_INFO("[Archipelago] Placement RC {} / {} <- {} (AP item {}, RandomizerGet {}, recipient {})",
                        rcIt->second, info.locationName, info.itemName, info.itemId, static_cast<int>(display),
                        info.playerName);
            loc->SetPlacedItem(display);
        }
    }
}

bool ArchipelagoClient::ProcessItem(int64_t itemId, bool notify) {
    if (gPlayState == nullptr) {
        SPDLOG_DEBUG("[Archipelago] Deferring item {} until a save is loaded", itemId);
        return false;
    }

    Player* player = GET_PLAYER(gPlayState);
    if (player == nullptr || Player_InBlockingCsMode(gPlayState, player) ||
        (player->stateFlags1 & (PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_GETTING_ITEM | PLAYER_STATE1_CARRYING_ACTOR))) {
        return false;
    }

    RandomizerGet randoGet = RG_NONE;
    if (itemId >= AP_BASE_ITEM_MIN && itemId <= AP_BASE_ITEM_MAX) {
        switch (itemId) {
#include "ArchipelagoItemMap.inc"
            default:
                SPDLOG_WARN("[Archipelago] Base item id {} has no SoH mapping yet", itemId);
                return true;
        }
    } else {
        switch (itemId) {
            case AP_ITEM_ROLL: randoGet = RG_ROLL; break;
            case AP_ITEM_GRAB: randoGet = RG_POWER_BRACELET; break;
            case AP_ITEM_CLIMB: randoGet = RG_CLIMB; break;
            case AP_ITEM_CRAWL: randoGet = RG_CRAWL; break;
            case AP_ITEM_SPEAK:
                // Speak is progression and should be presented as an important item.
                // Apply the consolidated AP ability flags immediately, then use a native
                // Speak major-item entry purely for the hold-item presentation.
                Flags_SetRandomizerInf(RAND_INF_CAN_SPEAK_DEKU);
                Flags_SetRandomizerInf(RAND_INF_CAN_SPEAK_GERUDO);
                Flags_SetRandomizerInf(RAND_INF_CAN_SPEAK_GORON);
                Flags_SetRandomizerInf(RAND_INF_CAN_SPEAK_HYLIAN);
                Flags_SetRandomizerInf(RAND_INF_CAN_SPEAK_KOKIRI);
                Flags_SetRandomizerInf(RAND_INF_CAN_SPEAK_ZORA);
                randoGet = RG_SPEAK_HYLIAN;
                break;
            case AP_ITEM_OPEN_CHEST: randoGet = RG_OPEN_CHEST; break;
            case AP_ITEM_ENEMY_SOUL: randoGet = RG_ENEMY_SOUL; break;
            case AP_ITEM_NPC_SOUL: randoGet = RG_NPC_SOUL; break;
            case AP_ITEM_ANIMAL_SOUL: randoGet = RG_ANIMAL_SOUL; break;
            case AP_ITEM_POT_SOUL: randoGet = RG_POT_SOUL; break;
            case AP_ITEM_CRATE_SOUL: randoGet = RG_CRATE_SOUL; break;
            case AP_ITEM_GRASS_SOUL: randoGet = RG_GRASS_SOUL; break;
            case AP_ITEM_ROCK_SOUL: randoGet = RG_ROCK_SOUL; break;
            case AP_ITEM_TREE_SOUL: randoGet = RG_TREE_SOUL; break;
            case AP_ITEM_BEEHIVE_SOUL: randoGet = RG_BEEHIVE_SOUL; break;
            case AP_ITEM_SIGN_SOUL: randoGet = RG_SIGN_SOUL; break;
            case AP_ITEM_SKULLTULA_SOUL: randoGet = RG_SKULLTULA_SOUL; break;
            case AP_ITEM_BUSINESS_SCRUB_SOUL:
                Flags_SetRandomizerInf(RAND_INF_BUSINESS_SCRUB_SOUL);
                Notification::Emit({ .prefix = "Archipelago", .message = "received", .suffix = "Business Scrub Soul", .remainingTime = 4.0f });
                return true;
            case AP_ITEM_SHOVEL: randoGet = RG_SHOVEL; break;
            case AP_ITEM_FLOW_OF_TIME:
                Flags_SetRandomizerInf(RAND_INF_FLOW_OF_TIME);
                Notification::Emit({ .prefix = "Archipelago", .message = "received", .suffix = "Flow of Time", .remainingTime = 4.0f });
                return true;
            default:
                if (itemId >= AP_FIRST_SONG_NOTE && itemId <= AP_LAST_SONG_NOTE) {
                    int noteIndex = static_cast<int>(itemId - AP_FIRST_SONG_NOTE);
                    Flags_SetRandomizerInf(static_cast<RandomizerInf>(RAND_INF_SONG_NOTE_0 + noteIndex));
                    RefreshSongNotes();
                    Notification::Emit({
                        .prefix = "Archipelago",
                        .message = "received",
                        .suffix = "Song Note " + std::to_string(noteIndex + 1),
                        .remainingTime = 3.0f,
                    });
                    return true;
                }
                SPDLOG_WARN("[Archipelago] Unknown SOH-EXTREME item id {}", itemId);
                return true;
        }
    }

    if (randoGet == RG_NONE) {
        return true;
    }

    // SoH traps are consumed through pendingIceTrapCount. Giving RG_ICE_TRAP as
    // an ordinary non-major item never enters that path, which is why AP traps
    // previously arrived in the log without actually freezing/hurting Link.
    if (randoGet == RG_ICE_TRAP) {
        gSaveContext.ship.pendingIceTrapCount++;
        SPDLOG_INFO("[Archipelago] Queued Ice Trap (pending={})", gSaveContext.ship.pendingIceTrapCount);
        Notification::Emit({ .prefix = "Archipelago", .message = "received", .suffix = "Ice Trap", .remainingTime = 4.0f });
        SendTrapLink("Ice Trap");
        return true;
    }

    auto item = Rando::StaticData::RetrieveItem(randoGet);
    GetItemEntry giEntry = item.GetGIEntry_Copy();

    // Open Chest is a pure randomizer progression flag.  Do not defer it to the
    // over-the-head major-item animation path: AP can mark the network item as
    // received before that animation finishes, leaving generation/tracker state
    // ahead of the actual save flag.  Apply it synchronously here.
    //
    // Randomizer_Item_Give already implements progressive semantics:
    //   first copy  -> RAND_INF_CAN_OPEN_CHEST
    //   second copy -> RAND_INF_CAN_OPEN_LARGE_CHEST
    if (randoGet == RG_OPEN_CHEST) {
        SPDLOG_INFO("[Archipelago] Applying Open Chest immediately (small={}, large={})",
                    Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_CHEST),
                    Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_LARGE_CHEST));
        Randomizer_Item_Give(gPlayState, giEntry);
        SPDLOG_INFO("[Archipelago] Open Chest applied (small={}, large={})",
                    Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_CHEST),
                    Flags_GetRandomizerInf(RAND_INF_CAN_OPEN_LARGE_CHEST));
        Notification::Emit({
            .prefix = "Archipelago",
            .message = "received",
            .suffix = item.GetName().english,
            .remainingTime = 4.0f,
        });
        return true;
    }

    // These AP rewards are vanilla inventory/stat items even though they are looked
    // up through the randomizer table. Apply them through Item_Give unconditionally
    // so their save fields are updated regardless of IsMajorItem classification.
    // This also covers Piece of Heart (WINNER), which maps to RG_PIECE_OF_HEART.
    if (randoGet == RG_GOLD_SKULLTULA_TOKEN || randoGet == RG_PIECE_OF_HEART ||
        randoGet == RG_HEART_CONTAINER) {
        if (giEntry.itemId == ITEM_NONE) {
            SPDLOG_ERROR("[Archipelago] Vanilla stat reward {} ({}) has no vanilla ItemID",
                         itemId, item.GetName().english);
            return true;
        }

        SPDLOG_INFO("[Archipelago] Applying vanilla stat reward id {} ({}) with Item_Give (itemId={})",
                    itemId, item.GetName().english, giEntry.itemId);
        Item_Give(gPlayState, static_cast<uint8_t>(giEntry.itemId));
        Notification::Emit({
            .prefix = "Archipelago",
            .message = "received",
            .suffix = item.GetName().english,
            .remainingTime = 4.0f,
        });
        return true;
    }

    // Match normal randomizer UX more closely: only advancement/major items interrupt
    // gameplay with the hold-item animation. Refills, ammo, rupees, maps/compasses,
    // ordinary hearts, and other non-major rewards are applied immediately and shown
    // in the side notification feed instead. This is especially important for AP
    // starting inventory/replayed filler, which should not repeatedly stop the player.
    if (!item.IsMajorItem()) {
        SPDLOG_INFO("[Archipelago] Applying non-major item id {} ({}) without get-item animation",
                    itemId, item.GetName().english);

        // Native-backed randomizer entries (MOD_NONE) must use vanilla Item_Give.
        // Randomizer_Item_Give intentionally rejects any GetItemEntry whose modIndex
        // is not MOD_RANDOMIZER, so sending vanilla AP rewards through it made the
        // notification appear while silently applying nothing.  This affected, among
        // other ordinary rewards, Gold Skulltula Tokens and Pieces of Heart.
        if (giEntry.modIndex == MOD_RANDOMIZER) {
            Randomizer_Item_Give(gPlayState, giEntry);
        } else if (giEntry.itemId != ITEM_NONE) {
            Item_Give(gPlayState, static_cast<uint8_t>(giEntry.itemId));
        } else {
            SPDLOG_ERROR("[Archipelago] Non-major item {} ({}) has neither a randomizer handler nor a vanilla ItemID",
                         itemId, item.GetName().english);
            return true;
        }

        Notification::Emit({
            .prefix = "Archipelago",
            .message = "received",
            .suffix = item.GetName().english,
            .remainingTime = 4.0f,
        });
        return true;
    }

    SPDLOG_INFO("[Archipelago] Presenting major item id {} as RandomizerGet {}", itemId, static_cast<int>(randoGet));

    // Major/progression rewards still use SoH's normal over-the-head item receive path.
    // If Link is temporarily unable to receive one, keep it queued until the engine can
    // safely start the get-item animation.
    if (!GiveItemEntryWithoutActor(gPlayState, giEntry)) {
        SPDLOG_DEBUG("[Archipelago] Link cannot receive major item {} yet; retrying", itemId);
        return false;
    }
    if (notify) {
        Notification::Emit({
            .prefix = "Archipelago",
            .message = "received",
            .suffix = GetApItemDisplayName(itemId),
            .remainingTime = 4.0f,
        });
    }
    return true;
}

void ArchipelagoClient::QueueDeathLink(const std::string& source, const std::string& cause) {
    // Network callback: only copy data under the mutex. Do not read CVars or touch
    // gameplay state from APCpp's websocket thread.
    std::scoped_lock lock(queueMutex);
    pendingDeathLinks.push_back({ source, cause });
}

void ArchipelagoClient::QueueTrapLink(const std::string& source, const std::string& trapName) {
    // Network callback: only copy data under the mutex.
    std::scoped_lock lock(queueMutex);
    pendingTrapLinks.push_back({ source, trapName.empty() ? "Trap" : trapName });
}

void ArchipelagoClient::SendDeathLink() {
    if (!deathLinkEnabled || !IsAuthenticated()) return;
    const char* slot = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "SOH-EXTREME");
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    AP_Bounce bounce{};
    std::vector<std::string> tags{ "DeathLink" };
    bounce.tags = &tags;
    bounce.data = "{\"time\":" + std::to_string(now) + ",\"source\":\"" +
                  EscapeJsonString(slot == nullptr ? "SOH-EXTREME" : slot) +
                  "\",\"cause\":\"" + EscapeJsonString(slot == nullptr ? "Link" : slot) +
                  " met with a terrible fate.\"}";
    AP_SendBounce(bounce);
    SPDLOG_INFO("[Archipelago] Sent DeathLink");
}

void ArchipelagoClient::SendTrapLink(const std::string& trapName) {
    if (!trapLinkEnabled || !IsAuthenticated()) return;
    const char* slot = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "SOH-EXTREME");
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    AP_Bounce bounce{};
    std::vector<std::string> tags{ "TrapLink" };
    bounce.tags = &tags;
    bounce.data = "{\"time\":" + std::to_string(now) + ",\"source\":\"" +
                  EscapeJsonString(slot == nullptr ? "SOH-EXTREME" : slot) +
                  "\",\"trap_name\":\"" + EscapeJsonString(trapName) + "\"}";
    AP_SendBounce(bounce);
    SPDLOG_INFO("[Archipelago] Sent TrapLink: {}", trapName);
}

void ArchipelagoClient::RefreshSongNotes() {
    struct SongNotes { const char* name; int count; int quest; };
    static constexpr SongNotes songs[] = {
        {"Zelda's Lullaby", 6, QUEST_SONG_LULLABY},
        {"Epona's Song", 6, QUEST_SONG_EPONA},
        {"Saria's Song", 6, QUEST_SONG_SARIA},
        {"Sun's Song", 6, QUEST_SONG_SUN},
        {"Song of Time", 6, QUEST_SONG_TIME},
        {"Song of Storms", 6, QUEST_SONG_STORMS},
        {"Minuet of Forest", 6, QUEST_SONG_MINUET},
        {"Bolero of Fire", 8, QUEST_SONG_BOLERO},
        {"Serenade of Water", 5, QUEST_SONG_SERENADE},
        {"Requiem of Spirit", 6, QUEST_SONG_REQUIEM},
        {"Nocturne of Shadow", 7, QUEST_SONG_NOCTURNE},
        {"Prelude of Light", 6, QUEST_SONG_PRELUDE},
    };

    int offset = 0;
    for (const auto& song : songs) {
        int collected = 0;
        for (int i = 0; i < song.count; ++i) {
            if (HasNote(RAND_INF_SONG_NOTE_0, offset + i)) ++collected;
        }

        const bool complete = collected == song.count;
        const bool alreadyOwned = (gSaveContext.inventory.questItems & (1u << song.quest)) != 0;
        if (complete && !alreadyOwned) {
            SetQuestSong(song.quest);
            SPDLOG_INFO("[Archipelago] Song Notes complete: {} ({}/{}) -> song unlocked",
                        song.name, collected, song.count);
            Notification::Emit({
                .prefix = "Song Notes",
                .message = "completed",
                .suffix = song.name,
                .remainingTime = 5.0f,
            });
        }
        offset += song.count;
    }
}

bool ArchipelagoClient::IsGameplaySessionActive() const {
    return gPlayState != nullptr && gSaveContext.ship.quest.id == QUEST_RANDOMIZER;
}

void ArchipelagoClient::BeginFileSelectActivation() {
    fileSelectActivationRequested = true;
}

void ArchipelagoClient::EndFileSelectActivation() {
    fileSelectActivationRequested = false;
}

std::vector<std::string> ArchipelagoClient::GetChatMessages() {
    std::scoped_lock lock(queueMutex);
    return std::vector<std::string>(chatMessages.begin(), chatMessages.end());
}

void ArchipelagoClient::SendChatMessage(const std::string& message) {
    if (!IsAuthenticated() || !IsGameplaySessionActive()) return;
    std::string trimmed = message;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();
    if (trimmed.empty()) return;
    AP_Say(trimmed);
    SPDLOG_INFO("[Archipelago] Chat sent: {}", trimmed);
}

void ArchipelagoClient::Update() {
    if (!enabled.load()) return;

    // Title/file-select safety boundary. APCpp may stay connected so authentication
    // can complete, but NOTHING that mutates gameplay state is processed until the
    // player actually chooses an AP file or a randomizer save is active.
    if (gPlayState == nullptr) {
        saveRuntimeSynchronized = false;
        deathStateInitialized = false;

        if (!fileSelectActivationRequested) {
            return;
        }

        // The user clicked Start/New AP File. During this narrow preparation phase,
        // only receive scout metadata needed by the file-select readiness barrier.
        // Do not apply placements, items, checks, DeathLink/TrapLink, time, or inventory.
        std::deque<PendingScout> scouts;
        {
            std::scoped_lock lock(queueMutex);
            scouts.swap(pendingScouts);
        }
        for (auto& scout : scouts) {
            scoutedLocations[scout.locationId] = std::move(scout.info);
        }
        if (IsAuthenticated()) {
            RequestLocationScouts();
        }
        return;
    }

    // A non-randomizer save must never consume AP gameplay queues either.
    if (gSaveContext.ship.quest.id != QUEST_RANDOMIZER) {
        saveRuntimeSynchronized = false;
        deathStateInitialized = false;
        return;
    }

    // From 0.5.9 onward AP ownership is a property of the SAVE, not merely of
    // "being a randomizer file while the AP client happens to be connected".
    // This prevents a normal local randomizer save from consuming server items.
    if (!currentSaveIsArchipelago || saveIdentityMismatch) {
        saveRuntimeSynchronized = false;
        deathStateInitialized = false;
        return;
    }

    fileSelectActivationRequested = false;

    // Keep AP randomizer settings authoritative for the lifetime of this AP save.
    // This also repairs a local menu edit immediately instead of letting actors use
    // a local setting that disagrees with the generated multiworld.
    if (IsAuthenticated() && slotSettingsLoaded) {
        EnforceSlotSettings();
    }

    // Existing-save reconciliation happens exactly once after gameplay really exists.
    if (!saveRuntimeSynchronized && IsAuthenticated() && slotSettingsLoaded) {
        ApplySlotSettings();
        ApplyScoutedPlacements();
        ApplyPostInitSlotState();
        saveRuntimeSynchronized = true;
        SPDLOG_INFO("[Archipelago] Reconciled loaded randomizer save with authoritative AP slot state");
    }

    // Pull APCpp's presentable message queue only while an AP gameplay session is active.
    // This gives SoH a persistent in-game chat log without touching UI/game state from
    // APCpp's networking callback thread.
    for (int i = 0; i < 16 && AP_IsMessagePending(); ++i) {
        AP_Message* message = AP_GetLatestMessage();
        if (message != nullptr) {
            const std::string line = message->text;
            {
                std::scoped_lock lock(queueMutex);
                chatMessages.push_back(line);
                while (chatMessages.size() > 200) chatMessages.pop_front();
            }
            if (message->type == AP_MessageType::Chat || message->type == AP_MessageType::ServerChat) {
                Notification::Emit({ .prefix = "Archipelago", .message = line, .remainingTime = 6.0f });
            }
            SPDLOG_INFO("[Archipelago Chat] {}", line);
        }
        AP_ClearLatestMessage();
    }

    std::deque<int64_t> checked;
    std::deque<PendingScout> scouts;
    std::deque<PendingDeathLink> deaths;
    std::deque<PendingTrapLink> traps;
    PendingItem nextItem{};
    bool hasItem = false;
    {
        std::scoped_lock lock(queueMutex);
        if (!pendingItems.empty()) {
            nextItem = pendingItems.front();
            pendingItems.pop_front();
            hasItem = true;
        }
        checked.swap(pendingCheckedLocations);
        scouts.swap(pendingScouts);
        deaths.swap(pendingDeathLinks);
        traps.swap(pendingTrapLinks);
    }

    // Apply link events only on the gameplay thread. APCpp callbacks run from its
    // networking context, so touching gSaveContext/player state in the callback itself
    // would be unsafe. Ignore our own Bounce echo here as well.
    const char* ownSlotPtr = CVarGetString(CVAR_REMOTE_ARCHIPELAGO("SlotName"), "");
    const std::string ownSlot = ownSlotPtr == nullptr ? "" : ownSlotPtr;
    while (!deaths.empty() && !ownSlot.empty() && deaths.front().source == ownSlot) deaths.pop_front();
    while (!traps.empty() && !ownSlot.empty() && traps.front().source == ownSlot) traps.pop_front();

    if (!deaths.empty()) {
        if (gPlayState != nullptr && GET_PLAYER(gPlayState) != nullptr) {
            const auto& death = deaths.front();
            suppressNextDeathLinkSend = true;
            GameInteractor::RawAction::SetPlayerHealth(0);
            Notification::Emit({ .prefix = "DeathLink", .message = "received from", .suffix = death.source,
                                 .remainingTime = 5.0f });
            SPDLOG_INFO("[Archipelago] Applied DeathLink from {}: {}", death.source, death.cause);
            deaths.pop_front();
        }
        if (!deaths.empty()) {
            std::scoped_lock lock(queueMutex);
            while (!deaths.empty()) { pendingDeathLinks.push_front(std::move(deaths.back())); deaths.pop_back(); }
        }
    }

    if (!traps.empty()) {
        if (gPlayState != nullptr && GET_PLAYER(gPlayState) != nullptr) {
            const auto& trap = traps.front();
            gSaveContext.ship.pendingIceTrapCount++;
            Notification::Emit({ .prefix = "TrapLink", .message = "received", .suffix = trap.trapName,
                                 .remainingTime = 5.0f });
            SPDLOG_INFO("[Archipelago] Queued TrapLink '{}' from {}", trap.trapName, trap.source);
            traps.pop_front();
        }
        if (!traps.empty()) {
            std::scoped_lock lock(queueMutex);
            while (!traps.empty()) { pendingTrapLinks.push_front(std::move(traps.back())); traps.pop_back(); }
        }
    }

    // Present at most ONE AP item at a time. The old loop could call
    // GiveItemEntryWithoutActor multiple times in one frame and overwrite Link's
    // getItemEntry before the first animation completed.
    if (hasItem) {
        if (ProcessItem(nextItem.id, nextItem.notify)) {
            MarkItemApplied(nextItem.sequence);
        } else {
            std::scoped_lock lock(queueMutex);
            pendingItems.push_front(nextItem);
        }
    }

    for (auto& scout : scouts) {
        scoutedLocations[scout.locationId] = std::move(scout.info);
    }
    if (!scouts.empty()) {
        SPDLOG_INFO("[Archipelago] Received {} scouted placement records", scouts.size());
        ApplyScoutedPlacements();
    }

    // APCpp tells us about locations already checked on the server. Remember those so
    // the periodic local reconciliation pass does not spam duplicate sends.
    for (int64_t loc : checked) {
        reportedLocations.insert(loc);
        SPDLOG_DEBUG("[Archipelago] Server confirms location {} checked", loc);
    }

    const bool authenticated = IsAuthenticated();
    if (authenticated) {
        // Every fresh authentication gets a full local -> server reconciliation. This
        // fixes checks collected before connecting, while loading a save, or during a
        // moment where OnRandoSetCheckStatus was missed.
        if (!wasAuthenticated) {
            std::vector<std::string> linkTags;
            if (deathLinkEnabled) linkTags.emplace_back("DeathLink");
            if (trapLinkEnabled) linkTags.emplace_back("TrapLink");
            AP_UpdateTags(linkTags);
            linkTagsSynchronized = true;
            SPDLOG_INFO("[Archipelago] Link tags: DeathLink={}, TrapLink={}", deathLinkEnabled, trapLinkEnabled);
            SPDLOG_INFO("[Archipelago] Authenticated; synchronizing collected locations");
            // Do not clear server-confirmed checks here. QueueCheckedLocation()
            // may already have populated speech/fallback locations during the
            // authentication handshake; clearing them here made first-talk checks
            // repeat after reconnect/load.
            syncFrameCounter = 0;
            scoutsRequested = false;
            RequestLocationScouts();
            SyncCollectedLocations();
        } else if (++syncFrameCounter >= 60) {
            // Roughly once per second at 60 FPS. SendLocation() deduplicates locations
            // that the server/client already knows about.
            syncFrameCounter = 0;
            SyncCollectedLocations();
        }


        // If the initial LocationScouts packet was lost or arrived before the
        // server finished authentication, retry until we actually have placement
        // data. Once data is present there is no reason to keep rescouting.
        if (scoutedLocations.empty() && syncFrameCounter == 0) {
            scoutsRequested = false;
            RequestLocationScouts();
        }
    } else if (wasAuthenticated) {
        // Preserve the last server-confirmed location set across a temporary
        // disconnect. Enable() clears it when intentionally connecting to a
        // different session; retaining it here prevents duplicate first-talk
        // checks while reconnecting to the same slot.
        syncFrameCounter = 0;
        scoutsRequested = false;
    }
    wasAuthenticated = authenticated;

    // Detect a local living -> dead transition exactly once. An incoming DeathLink
    // sets suppressNextDeathLinkSend so it cannot echo back into the link group.
    if (gPlayState == nullptr) {
        deathStateInitialized = false;
    } else {
        const bool alive = gSaveContext.health > 0;
        if (!deathStateInitialized) {
            lastPlayerAlive = alive;
            deathStateInitialized = true;
        } else {
            if (lastPlayerAlive && !alive) {
                if (suppressNextDeathLinkSend) {
                    suppressNextDeathLinkSend = false;
                } else {
                    SendDeathLink();
                }
            }
            if (alive) suppressNextDeathLinkSend = false;
            lastPlayerAlive = alive;
        }
    }

    // Keep Flow of Time frozen until its progression item is received.
    if (CVarGetInteger(CVAR_RANDOMIZER_SETTING("ShuffleFlowOfTime"), 0) &&
        !Flags_GetRandomizerInf(RAND_INF_FLOW_OF_TIME) && gPlayState != nullptr) {
        static constexpr u16 kTimes[] = { 0x4555, 0x8000, 0xB555, 0x0000 };
        int selected = CVarGetInteger(CVAR_RANDOMIZER_SETTING("FrozenStartingTime"), 1);
        if (selected <= 0 || selected > 4) selected = 1;
        gSaveContext.dayTime = kTimes[selected - 1];
        gSaveContext.skyboxTime = gSaveContext.dayTime;
    }
}

void ArchipelagoClient::SyncCollectedLocations() {
    if (!IsAuthenticated() || !IsGameplaySessionActive()) return;

    auto ctx = Rando::Context::GetInstance();
    if (!ctx) return;

    if (!activeLocationsLoaded) return;

    for (const auto& [rcValue, apLocation] : rcToApLocation) {
        if (activeLocations.find(apLocation) == activeLocations.end()) continue;
        auto* location = ctx->GetItemLocation(static_cast<RandomizerCheck>(rcValue));
        if (location != nullptr && location->HasObtained()) {
            SendLocation(apLocation);
        }
    }
}


bool ArchipelagoClient::OwnsCheck(int32_t randomizerCheck) {
    if (!enabled.load()) return false;

    const int64_t apLocation = ResolveApLocationForCheck(randomizerCheck);
    if (apLocation < 0) return false;

    // Before slot data arrives, stay conservative and prevent the local randomizer from
    // awarding a fake item. Once authenticated slot data is loaded, only actual AP slot
    // locations are owned by Archipelago.
    if (!activeLocationsLoaded) return true;
    return activeLocations.find(apLocation) != activeLocations.end();
}

void ArchipelagoClient::ReportCheck(int32_t randomizerCheck) {
    if (!IsGameplaySessionActive()) return;
    const int64_t apLocation = ResolveApLocationForCheck(randomizerCheck);
    if (apLocation < 0) {
        SPDLOG_WARN("[Archipelago] RandomizerCheck {} has no AP location mapping", randomizerCheck);
        return;
    }
    if (activeLocationsLoaded && activeLocations.find(apLocation) == activeLocations.end()) {
        SPDLOG_DEBUG("[Archipelago] RC {} maps to AP location {}, but that location is not active in this slot",
                     randomizerCheck, apLocation);
        return;
    }
    SendLocation(apLocation, true);
}




static uint64_t Archipelago_NpcSpeechIdentity(const Actor* actor, int16_t sceneNum) {
    if (actor == nullptr) return 0;

    // FNV-1a over immutable spawn identity. home.pos is the actor's spawn position,
    // not the current animated position. Params separates NPC variants sharing a
    // model/actor type.
    uint64_t hash = 1469598103934665603ULL;
    auto mix = [&hash](uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            hash ^= (value >> (i * 8)) & 0xFFULL;
            hash *= 1099511628211ULL;
        }
    };

    mix(static_cast<uint16_t>(sceneNum));
    mix(static_cast<uint16_t>(actor->id));
    mix(static_cast<uint16_t>(actor->params));
    mix(static_cast<uint16_t>(static_cast<int16_t>(actor->home.pos.x)));
    mix(static_cast<uint16_t>(static_cast<int16_t>(actor->home.pos.y)));
    mix(static_cast<uint16_t>(static_cast<int16_t>(actor->home.pos.z)));
    return hash == 0 ? 1 : hash;
}

static bool Archipelago_IsManualSpeechActor(const Actor* actor) {
    if (actor == nullptr || actor->category != ACTORCAT_NPC ||
        (actor->flags & ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED)) {
        return false;
    }

    // Keep this list aligned with RegisterShuffleSpeak().  The generic
    // Archipelago speech bank is only for actors the fork itself considers a
    // manual Speak interaction.  Named/native speech checks are resolved before
    // this fallback and are unaffected by this filter.
    switch (actor->id) {
        case ACTOR_EN_DNS:
        case ACTOR_EN_DNT_JIJI:
        case ACTOR_EN_HINTNUTS:
        case ACTOR_EN_KAKASI:
        case ACTOR_EN_KAKASI2:
        case ACTOR_EN_KAKASI3:
        case ACTOR_OBJ_DEKUJR:
        case ACTOR_EN_GE1:
        case ACTOR_EN_GE3:
        case ACTOR_EN_NB:
        case ACTOR_EN_GO:
        case ACTOR_EN_GO2:
        case ACTOR_EN_GM:
        case ACTOR_EN_DU:
        case ACTOR_EN_ANI:
        case ACTOR_EN_BOM_BOWL_MAN:
        case ACTOR_EN_CS:
        case ACTOR_EN_DAIKU:
        case ACTOR_EN_DAIKU_KAKARIKO:
        case ACTOR_EN_DS:
        case ACTOR_EN_FU:
        case ACTOR_EN_GB:
        case ACTOR_EN_GIRLA:
        case ACTOR_EN_GUEST:
        case ACTOR_EN_HEISHI1:
        case ACTOR_EN_HEISHI2:
        case ACTOR_EN_HEISHI3:
        case ACTOR_EN_HEISHI4:
        case ACTOR_EN_HS:
        case ACTOR_EN_HS2:
        case ACTOR_EN_HY:
        case ACTOR_EN_IN:
        case ACTOR_EN_JS:
        case ACTOR_EN_MA1:
        case ACTOR_EN_MA2:
        case ACTOR_EN_MA3:
        case ACTOR_EN_MK:
        case ACTOR_EN_MM:
        case ACTOR_EN_MM2:
        case ACTOR_EN_MS:
        case ACTOR_EN_MU:
        case ACTOR_EN_NIW_GIRL:
        case ACTOR_EN_NIW_LADY:
        case ACTOR_EN_SSH:
        case ACTOR_EN_STH:
        case ACTOR_EN_SYATEKI_MAN:
        case ACTOR_EN_TA:
        case ACTOR_EN_TAKARA_MAN:
        case ACTOR_EN_TG:
        case ACTOR_EN_TK:
        case ACTOR_EN_PO_RELAY:
        case ACTOR_EN_TORYO:
        case ACTOR_EN_XC:
        case ACTOR_EN_ZL1:
        case ACTOR_EN_ZL2:
        case ACTOR_EN_ZL3:
        case ACTOR_EN_ZL4:
        case ACTOR_FISHING:
        case ACTOR_EN_KO:
        case ACTOR_EN_SA:
        case ACTOR_EN_MD:
        case ACTOR_EN_SKJ:
        case ACTOR_EN_DIVING_GAME:
        case ACTOR_EN_KZ:
        case ACTOR_EN_RU1:
        case ACTOR_EN_RU2:
        case ACTOR_EN_ZO:
        case ACTOR_EN_OWL:
            return true;

        case ACTOR_EN_OSSAN:
            switch (actor->params) {
                case OSSAN_TYPE_KOKIRI:
                case OSSAN_TYPE_KAKARIKO_POTION:
                case OSSAN_TYPE_BOMBCHUS:
                case OSSAN_TYPE_MARKET_POTION:
                case OSSAN_TYPE_BAZAAR:
                case OSSAN_TYPE_ADULT:
                case OSSAN_TYPE_TALON:
                case OSSAN_TYPE_INGO:
                case OSSAN_TYPE_MASK:
                case OSSAN_TYPE_GORON:
                case OSSAN_TYPE_ZORA:
                    return true;
                default:
                    return false;
            }

        // ACTOR_EN_GE2 is intentionally excluded, matching ShuffleSpeak, so the
        // player can always ask to be thrown in jail.
        default:
            return false;
    }
}

static bool Archipelago_HasExtremeSpeak() {
    // SOH-EXTREME intentionally exposes ONE AP item named "Speak".
    // Receiving it sets every underlying Ship race-speak flag, so checking ANY
    // one of those flags is the canonical runtime test for our single ability.
    return Flags_GetRandomizerInf(RAND_INF_CAN_SPEAK_DEKU) ||
           Flags_GetRandomizerInf(RAND_INF_CAN_SPEAK_GERUDO) ||
           Flags_GetRandomizerInf(RAND_INF_CAN_SPEAK_GORON) ||
           Flags_GetRandomizerInf(RAND_INF_CAN_SPEAK_HYLIAN) ||
           Flags_GetRandomizerInf(RAND_INF_CAN_SPEAK_KOKIRI) ||
           Flags_GetRandomizerInf(RAND_INF_CAN_SPEAK_ZORA);
}

static int64_t Archipelago_ResolveNpcSpeechLocation(const Actor* actor, int16_t sceneNum) {
    if (actor == nullptr) return -1;

    struct SpeechMapEntry {
        int32_t rc;
        int64_t apLocation;
    };
    static const SpeechMapEntry speechMap[] = {
#include "ArchipelagoSpeechMap.inc"
    };

    // First pass: exact scene + actor + params. This is preferred because several
    // scenes contain multiple actors of the same type.
    for (const auto& entry : speechMap) {
        auto* data = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(entry.rc));
        if (data == nullptr) continue;
        if (static_cast<int16_t>(data->GetScene()) == sceneNum &&
            static_cast<int16_t>(data->GetActorID()) == actor->id &&
            data->GetActorParams() == actor->params) {
            return entry.apLocation;
        }
    }

    // Dialogue-backed checks frequently transform their actor params at runtime.
    // If scene+actor identifies exactly ONE speech check, use it even when params
    // no longer match the spawn-table value.
    int64_t unique = -1;
    for (const auto& entry : speechMap) {
        auto* data = Rando::StaticData::GetLocation(static_cast<RandomizerCheck>(entry.rc));
        if (data == nullptr) continue;
        if (static_cast<int16_t>(data->GetScene()) == sceneNum &&
            static_cast<int16_t>(data->GetActorID()) == actor->id) {
            if (unique != -1 && unique != entry.apLocation) {
                unique = -2; // ambiguous: do not award the wrong NPC
                break;
            }
            unique = entry.apLocation;
        }
    }
    return unique >= 0 ? unique : -1;
}


void ArchipelagoClient::LoadFallbackNpcSpeechHashes(const std::vector<uint64_t>& hashes) {
    fallbackNpcSpeechHashes = hashes;
    fallbackNpcSpeechSeen.clear();
    for (uint64_t hash : fallbackNpcSpeechHashes) {
        if (hash != 0) fallbackNpcSpeechSeen.insert(hash);
    }
    SPDLOG_INFO("[Archipelago] Loaded {} generic NPC first-talk identities", fallbackNpcSpeechHashes.size());
}

std::vector<uint64_t> ArchipelagoClient::GetFallbackNpcSpeechHashes() const {
    return fallbackNpcSpeechHashes;
}

bool ArchipelagoClient::ReportFallbackNpcSpeech(const Actor* actor) {
    if (!IsGameplaySessionActive() || actor == nullptr) return false;
    if (!CVarGetInteger(CVAR_RANDOMIZER_SETTING("NpcSpeechSanity"), 0)) return false;
    if (!Archipelago_IsManualSpeechActor(actor)) return false;

    const uint64_t identity = Archipelago_NpcSpeechIdentity(actor, static_cast<int16_t>(gPlayState->sceneNum));
    if (identity == 0 || fallbackNpcSpeechSeen.find(identity) != fallbackNpcSpeechSeen.end()) {
        return false;
    }

    // Use exactly one still-unchecked generic AP location for this persisted
    // manual-talk identity. These locations are locked to non-progression filler
    // by the APWorld, so unmodelled NPC routes can never create impossible logic.
    int64_t speechLocation = -1;
    for (int64_t i = 0; i < AP_EXTREME_SPEECH_FALLBACK_COUNT; ++i) {
        const int64_t candidate = AP_EXTREME_SPEECH_FALLBACK_BASE + i;
        if (activeLocations.find(candidate) != activeLocations.end() &&
            reportedLocations.find(candidate) == reportedLocations.end()) {
            speechLocation = candidate;
            break;
        }
    }

    if (speechLocation < 0) {
        SPDLOG_WARN("[Archipelago] Generic NPC Speech fallback bank exhausted; scene={} actor={} params={}",
                    gPlayState->sceneNum, actor->id, actor->params);
        return false;
    }

    if (!ReportNpcSpeechLocation(speechLocation)) {
        return false;
    }

    fallbackNpcSpeechSeen.insert(identity);
    fallbackNpcSpeechHashes.push_back(identity);
    SPDLOG_INFO("[Archipelago] Generic NPC first-talk mapped to fallback location {} "
                "(scene={} actor={} params={} home=({}, {}, {}))",
                speechLocation, gPlayState->sceneNum, actor->id, actor->params,
                actor->home.pos.x, actor->home.pos.y, actor->home.pos.z);
    return true;
}

void ArchipelagoClient::ReportNpcSpeech(int32_t randomizerCheck) {
    if (!IsGameplaySessionActive()) return;
    if (!CVarGetInteger(CVAR_RANDOMIZER_SETTING("NpcSpeechSanity"), 0)) return;

    const int64_t baseLocation = ResolveApLocationForCheck(randomizerCheck);
    if (baseLocation < 0) return;
    ReportNpcSpeechLocation(AP_EXTREME_SPEECH_BASE + baseLocation);
}

bool ArchipelagoClient::ReportNpcSpeechLocation(int64_t speechLocation) {
    if (!IsGameplaySessionActive()) return false;
    if (!CVarGetInteger(CVAR_RANDOMIZER_SETTING("NpcSpeechSanity"), 0)) return false;
    if (!activeLocationsLoaded || activeLocations.find(speechLocation) == activeLocations.end()) return false;

    // This is the critical "first interaction" test. reportedLocations is also
    // rebuilt from Archipelago's checked-locations callback after reconnect/load,
    // so an NPC already checked on the server immediately goes back to normal dialog.
    if (reportedLocations.find(speechLocation) != reportedLocations.end()) return false;

    auto scoutIt = scoutedLocations.find(speechLocation);
    const std::string speechName =
        scoutIt != scoutedLocations.end() ? scoutIt->second.locationName : "NPC Speech";

    // SendLocation inserts into reportedLocations synchronously before handing the
    // check to APCpp, so a second A press cannot trigger the speech check twice.
    SendLocation(speechLocation, true);

    if (reportedLocations.find(speechLocation) == reportedLocations.end()) {
        // Authentication/session changed between the tests above and SendLocation.
        return false;
    }

    Notification::Emit({
        .prefix = "Archipelago",
        .message = "checked",
        .suffix = speechName,
        .remainingTime = 4.0f,
    });
    SPDLOG_INFO("[Archipelago] NPC Speech Sanity first-talk intercepted: {}", speechLocation);
    return true;
}

void ArchipelagoClient::SendLocation(int64_t locationId, bool notifyRemote) {
    if (!IsAuthenticated() || !IsGameplaySessionActive()) return;
    if (!activeLocationsLoaded) return;
    if (activeLocations.find(locationId) == activeLocations.end()) {
        SPDLOG_WARN("[Archipelago] Refusing to send inactive/invalid AP location {}", locationId);
        return;
    }

    // Location checks are idempotent on the AP server, but avoiding repeats makes the
    // server log useful and prevents a per-frame flood during reconciliation.
    if (!reportedLocations.insert(locationId).second) return;

    // A remote item is never received back through our ReceivedItems callback, so
    // the player needs feedback at the moment this location is checked.  Use the
    // scout record we already loaded for the physical placement.  Reconciliation
    // calls SendLocation() with notifyRemote=false, preventing old checks from
    // spamming notifications after reconnect/load.
    if (notifyRemote) {
        auto scoutIt = scoutedLocations.find(locationId);
        if (scoutIt != scoutedLocations.end() && scoutIt->second.playerId != AP_GetPlayerID()) {
            const auto& info = scoutIt->second;
            Notification::Emit({
                .prefix = "Archipelago",
                .message = "sent " + info.itemName + " to",
                .suffix = info.playerName,
                .remainingTime = 5.0f,
            });
            SPDLOG_INFO("[Archipelago] Sent remote item {} to {} from {}",
                        info.itemName, info.playerName, info.locationName);
        }
    }

    SPDLOG_INFO("[Archipelago] Sending checked location {}", locationId);
    AP_SendItem(locationId);
}

void ArchipelagoClient::RegisterHooks() {
    rcToApLocation = {
#include "ArchipelagoLocationMap.inc"
    };
    apLocationToRc.clear();
    for (const auto& [rc, apLocation] : rcToApLocation) {
        apLocationToRc[apLocation] = rc;
    }
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameFrameUpdate>([]() {
        ArchipelagoClient::GetInstance().Update();
    });

    // NPC Speech Sanity mirrors shuffled signs:
    // first A press = AP check, later presses = normal dialogue.
    //
    // Mapped NPCs use their real speech location (fully randomized).
    // Flavor/unmapped NPCs use a persistent generic fallback location, but only
    // when Ship's own ShuffleSpeak code considers that actor manually speakable.
    // This prevents unrelated ACTORCAT_NPC actors from silently consuming checks.
    COND_VB_SHOULD(VB_SKIP_TALKING, true, {
        if (!*should || gPlayState == nullptr ||
            !CVarGetInteger(CVAR_RANDOMIZER_SETTING("NpcSpeechSanity"), 0)) {
            return;
        }

        Player* player = GET_PLAYER(gPlayState);
        Actor* actor = player != nullptr ? player->talkActor : nullptr;
        if (actor == nullptr || actor->category != ACTORCAT_NPC ||
            (actor->flags & ACTOR_FLAG_TALK_OFFER_AUTO_ACCEPTED)) {
            return;
        }

        if (RAND_GET_OPTION(RSK_SHUFFLE_NPC_SOUL) &&
            !Flags_GetRandomizerInf(RAND_INF_NPC_SOUL)) {
            return;
        }
        if (RAND_GET_OPTION(RSK_SHUFFLE_SPEAK) && !Archipelago_HasExtremeSpeak()) {
            return;
        }

        int64_t speechLocation = -1;

        auto rando = OTRGlobals::Instance->gRandomizer;
        if (rando != nullptr) {
            Rando::Location* location =
                rando->GetCheckObjectFromActor(actor->id, gPlayState->sceneNum, actor->params);
            if (location == nullptr || location->GetRandomizerCheck() == RC_UNKNOWN_CHECK) {
                location = rando->GetCheckObjectFromActor(actor->id, gPlayState->sceneNum, actor->textId);
            }

            if (location != nullptr && location->GetRandomizerCheck() != RC_UNKNOWN_CHECK) {
                const int64_t baseLocation =
                    ArchipelagoClient::GetInstance().ResolveApLocationForCheck(
                        static_cast<int32_t>(location->GetRandomizerCheck()));
                if (baseLocation >= 0) {
                    const int64_t candidate = AP_EXTREME_SPEECH_BASE + baseLocation;
                    if (ArchipelagoClient::GetInstance().ReportNpcSpeechLocation(candidate)) {
                        *should = false;
                        return;
                    }
                    // Already checked mapped NPC -> normal dialogue.
                    if (ArchipelagoClient::GetInstance().reportedLocations.find(candidate) !=
                        ArchipelagoClient::GetInstance().reportedLocations.end()) {
                        return;
                    }
                }
            }
        }

        if (speechLocation < 0) {
            speechLocation =
                Archipelago_ResolveNpcSpeechLocation(actor, static_cast<int16_t>(gPlayState->sceneNum));
        }
        if (speechLocation >= 0) {
            if (ArchipelagoClient::GetInstance().ReportNpcSpeechLocation(speechLocation)) {
                *should = false;
                return;
            }
            if (ArchipelagoClient::GetInstance().reportedLocations.find(speechLocation) !=
                ArchipelagoClient::GetInstance().reportedLocations.end()) {
                return;
            }
        }

        // No native speech location exists for this NPC (for example ordinary
        // Kokiri/Market flavor NPCs). Only true manual-Speak actors are eligible
        // for the persistent generic first-talk bank.
        if (Archipelago_IsManualSpeechActor(actor) &&
            ArchipelagoClient::GetInstance().ReportFallbackNpcSpeech(actor)) {
            *should = false;
        }
    });

    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnRandoSetCheckStatus>(
        [](RandomizerCheck rc, RandomizerCheckStatus status) {
            // RCSHOW_SAVED is also an obtained check. This matters when loading a file:
            // those checks may never transition through RCSHOW_COLLECTED this session.
            if (status != RCSHOW_COLLECTED && status != RCSHOW_SAVED) return;

            if (status == RCSHOW_COLLECTED) {
                ArchipelagoClient::GetInstance().ReportCheck(static_cast<int32_t>(rc));
            } else {
                auto& client = ArchipelagoClient::GetInstance();
                auto it = client.rcToApLocation.find(static_cast<int32_t>(rc));
                if (it != client.rcToApLocation.end()) client.SendLocation(it->second, false);
            }
        });
}

static void InitArchipelagoClient() {
    auto& client = ArchipelagoClient::GetInstance();
    client.RegisterHooks();
    if (CVarGetInteger(CVAR_REMOTE_ARCHIPELAGO("Enabled"), 0)) {
        client.Enable();
    }
}

static RegisterShipInitFunc initArchipelagoClient(InitArchipelagoClient);


extern "C" bool Archipelago_IsAuthenticatedForFileSelect(void) {
    auto& client = ArchipelagoClient::GetInstance();
    if (!client.IsAuthenticated()) return false;
    client.BeginFileSelectActivation();

    // Starting an AP file is now a synchronization barrier, not merely an auth check.
    // Do not enter name-entry/create the save until settings, active locations, exact
    // shop prices, and all physical AP placements have arrived.
    client.EnsureLocationScouts();
    return client.IsReadyForFileSelect();
}

extern "C" bool Archipelago_ShouldHandleCheck(int32_t randomizerCheck) {
    return ArchipelagoClient::GetInstance().OwnsCheck(randomizerCheck);
}

extern "C" void Archipelago_ReportCheck(int32_t randomizerCheck) {
    ArchipelagoClient::GetInstance().ReportCheck(randomizerCheck);
}

extern "C" const char* Archipelago_GetRemoteItemDescription(int32_t randomizerCheck) {
    static thread_local std::string description;
    description = ArchipelagoClient::GetInstance().GetRemoteItemDescription(randomizerCheck);
    return description.c_str();
}

extern "C" void Archipelago_RefreshPlacementForCheck(int32_t randomizerCheck) {
    ArchipelagoClient::GetInstance().RefreshPlacementForCheck(randomizerCheck);
}

extern "C" void Archipelago_InitSaveFile(void) {
    auto& client = ArchipelagoClient::GetInstance();

    // AP is authoritative. Apply the complete slot settings snapshot to both CVars
    // and the live Rando Context BEFORE the normal randomizer save initializer reads
    // any setting. This is what makes hidden/freestanding rupees, pots, shops, keys,
    // starting inventory, bridge rules, souls, etc. match the generated AP slot.
    // Set the quest type first. Most shuffle registration functions gate on IS_RANDO;
    // they must see an active randomizer quest when ApplySlotSettings refreshes hooks.
    gSaveContext.ship.quest.id = QUEST_RANDOMIZER;
    client.ApplySlotSettings();
    Randomizer_InitSaveFile();
    // Some native save initialization paths normalize options/derived CVars. Re-apply
    // the exact AP snapshot after starting inventory/event initialization as a final
    // authority barrier before the first room can spawn actors.
    client.ApplySlotSettings();

    // The file-select readiness barrier guarantees scouts are already here, so replace
    // native placements/prices before the first scene actors cache their models.
    client.ApplyScoutedPlacements();
    client.ApplyPostInitSlotState();
    client.PrepareNewSaveItemReplay();
    client.EndFileSelectActivation();
    SPDLOG_INFO("[Archipelago] Initialized new SOH-EXTREME save with synchronized AP settings/placements");
}
