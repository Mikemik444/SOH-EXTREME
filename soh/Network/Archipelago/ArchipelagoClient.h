#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


class ArchipelagoClient {
  public:
    static ArchipelagoClient& GetInstance();

    void Enable();
    void Disable();
    void Toggle();
    void Update();

    bool IsEnabled() const { return enabled.load(); }
    bool IsAuthenticated() const;
    bool IsConnectionRefused() const;
    std::string GetStatusText() const;

    void SendLocation(int64_t locationId, bool notifyRemote = false);
    void ReportCheck(int32_t randomizerCheck);
    void ReportNpcSpeech(int32_t randomizerCheck);
    bool ReportNpcSpeechLocation(int64_t speechLocation);
    bool ReportFallbackNpcSpeech(const struct Actor* actor);
    void LoadFallbackNpcSpeechHashes(const std::vector<uint64_t>& hashes);
    std::vector<uint64_t> GetFallbackNpcSpeechHashes() const;
    void RegisterHooks();
    bool OwnsCheck(int32_t randomizerCheck);
    void ApplyScoutedPlacements();
    void RefreshPlacementForCheck(int32_t randomizerCheck);
    void EnsureLocationScouts();
    std::string GetRemoteItemDescription(int32_t randomizerCheck);
    bool IsReadyForFileSelect() const;
    void ApplySlotSettings();
    void EnforceSlotSettings();
    void ApplyPostInitSlotState();
    void PrepareNewSaveItemReplay();
    void BeginFileSelectActivation();
    void EndFileSelectActivation();
    bool IsGameplaySessionActive() const;
    std::vector<std::string> GetChatMessages();
    void SendChatMessage(const std::string& message);

    // Per-save Archipelago receipt state.  This is serialized by SaveManager so
    // each save resumes the server ReceivedItems stream at exactly the point
    // represented by that save's inventory.
    void LoadSaveMetadata(bool isArchipelagoSave, uint64_t receivedItemCount, const std::string& server,
                          const std::string& slot, const std::string& cachedSettingsJson);
    bool IsCurrentSaveArchipelago() const { return currentSaveIsArchipelago; }
    uint64_t GetAppliedItemCount() const { return appliedItemCount; }
    std::string GetSaveServer() const { return saveServer; }
    std::string GetSaveSlot() const { return saveSlot; }
    std::string GetCachedSlotSettingsJson() const { return cachedSlotSettingsJson; }

  private:
    ArchipelagoClient() = default;
    ~ArchipelagoClient() = default;
    ArchipelagoClient(const ArchipelagoClient&) = delete;
    ArchipelagoClient& operator=(const ArchipelagoClient&) = delete;

    void BeginItemReplay();
    void QueueItem(int64_t itemId, bool notify);
    void QueueCheckedLocation(int64_t locationId);
    void QueueDeathLink(const std::string& source, const std::string& cause);
    void QueueTrapLink(const std::string& source, const std::string& trapName);
    void SendDeathLink();
    void SendTrapLink(const std::string& trapName);
    void QueueLocationInfo(int64_t locationId, int64_t itemId, int playerId, int flags, const std::string& itemName,
                           const std::string& playerName, const std::string& locationName);
    bool ProcessItem(int64_t itemId, bool notify, uint64_t sequence);
    void MarkItemApplied(uint64_t sequence);
    void FinalizeMajorItemReceipt(int modIndex, int itemId, int getItemId);
    std::string GetReceivedCountCVar() const;
    int32_t MapApItemToRandomizerGet(int64_t itemId) const;
    void RequestLocationScouts();
    int64_t ResolveApLocationForCheck(int32_t randomizerCheck);
    void ApplySlotSetting(const std::string& key, int value);
    void RefreshSongNotes();
    void RegisterCallbacks();
    void SyncCollectedLocations();
    void SetActiveLocationsFromJson(const std::string& raw);
    void SetSlotSettingsFromJson(const std::string& raw);
    void SetShopPricesFromJson(const std::string& raw);
    struct PendingItem {
        int64_t id;
        bool notify;
        uint64_t sequence = 0;
    };
    struct ScoutedLocation {
        int64_t itemId = 0;
        int playerId = 0;
        int flags = 0;
        std::string itemName;
        std::string playerName;
        std::string locationName;
    };
    struct PendingScout {
        int64_t locationId = 0;
        ScoutedLocation info;
    };
    struct PendingDeathLink {
        std::string source;
        std::string cause;
    };
    struct PendingTrapLink {
        std::string source;
        std::string trapName;
    };

    std::atomic<bool> enabled{ false };
    std::mutex queueMutex;
    std::deque<PendingItem> pendingItems;
    std::deque<int64_t> pendingCheckedLocations;
    std::deque<PendingScout> pendingScouts;
    std::deque<PendingDeathLink> pendingDeathLinks;
    std::deque<PendingTrapLink> pendingTrapLinks;
    std::deque<std::string> chatMessages;
    std::unordered_map<int, int64_t> rcToApLocation;
    std::unordered_map<int64_t, int> apLocationToRc;
    std::unordered_map<int64_t, ScoutedLocation> scoutedLocations;
    std::unordered_set<int64_t> reportedLocations;
    std::unordered_set<int64_t> activeLocations;
    std::unordered_map<std::string, int> slotSettings;
    std::unordered_map<int64_t, uint16_t> shopPrices;
    bool activeLocationsLoaded = false;
    bool slotSettingsLoaded = false;
    bool shopPricesLoaded = false;
    bool kakarikoGateOpen = false;
    bool deathLinkEnabled = false;
    bool trapLinkEnabled = false;
    bool linkTagsSynchronized = false;
    bool deathStateInitialized = false;
    bool lastPlayerAlive = true;
    bool suppressNextDeathLinkSend = false;
    size_t expectedScoutCount = 0;
    bool wasAuthenticated = false;
    uint32_t syncFrameCounter = 0;
    bool scoutsRequested = false;
    // Reset whenever no gameplay save is active. The next loaded AP save gets a full
    // authoritative settings/placement/state reconciliation before normal play continues.
    bool saveRuntimeSynchronized = false;
    bool fileSelectActivationRequested = false;
    uint64_t incomingItemOrdinal = 0;
    uint64_t appliedItemCount = 0;
    // Major AP items are not committed when the get-item animation merely starts.
    // They become durable only when SoH fires OnItemReceive after actually granting the item.
    bool awaitingMajorItemReceipt = false;
    uint64_t awaitingMajorSequence = 0;
    int64_t awaitingMajorApItemId = 0;
    int awaitingMajorModIndex = 0;
    int awaitingMajorItemId = 0;
    int awaitingMajorGetItemId = 0;
    std::vector<int64_t> receivedItemSnapshot;
    bool currentSaveIsArchipelago = false;
    bool saveMetadataLoaded = false;
    bool saveIdentityMismatch = false;
    std::string saveServer;
    std::vector<uint64_t> fallbackNpcSpeechHashes;
    std::unordered_set<uint64_t> fallbackNpcSpeechSeen;
    std::string saveSlot;
    std::string cachedSlotSettingsJson;
};
