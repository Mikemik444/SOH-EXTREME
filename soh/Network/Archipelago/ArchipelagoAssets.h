#pragma once

#include "align_asset_macro.h"

// Collision-proof SOH-EXTREME Archipelago cross-game item icons.
// The 32 suffix is part of the archive key so stale rgba16/rgba32 files from
// older patches cannot overwrite the resource during GenerateSohOtr.
#define dgArchipelagoItemImportantTex "__OTR__textures/parameter_static/gArchipelagoRemoteImportant32"
static const ALIGN_ASSET(2) char gArchipelagoItemImportantTex[] = dgArchipelagoItemImportantTex;

#define dgArchipelagoItemNormalTex "__OTR__textures/parameter_static/gArchipelagoRemoteNormal32"
static const ALIGN_ASSET(2) char gArchipelagoItemNormalTex[] = dgArchipelagoItemNormalTex;

// Official Archipelago-SoH 3D item model resource (ported from aMannus/Shipwright archipelago branch).
#define dgArchipelagoOfficialItemDL "__OTR__objects/object_archipelago_item/gArchipelagoItemDL"
static const ALIGN_ASSET(2) char gArchipelagoOfficialItemDL[] = dgArchipelagoOfficialItemDL;
