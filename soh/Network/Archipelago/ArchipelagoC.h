#pragma once

#include <stdbool.h>
#include <stdint.h>

// File-select-only pseudo quest. It is converted to QUEST_RANDOMIZER when the
// save is actually created, so the rest of SoH continues using its normal
// randomizer code paths.
#ifndef QUEST_ARCHIPELAGO
#define QUEST_ARCHIPELAGO (QUEST_BOSSRUSH + 1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool Archipelago_IsAuthenticatedForFileSelect(void);
void Archipelago_InitSaveFile(void);
bool Archipelago_ShouldHandleCheck(int32_t randomizerCheck);
void Archipelago_ReportCheck(int32_t randomizerCheck);
void Archipelago_RefreshPlacementForCheck(int32_t randomizerCheck);
// Returns "<item> for <player>" for a scouted remote AP placement.
// Empty string means the check is not a known remote placement.
const char* Archipelago_GetRemoteItemDescription(int32_t randomizerCheck);

#ifdef __cplusplus
}
#endif
