SOH-EXTREME Archipelago v5.7 - Frozen Time + Kakariko Wonder + Roc Collision Logic

Apply on top of v5.6.1.

Fixes:
- Flow of Time now gates stock SoH Day/Night Cycle event locations in the APWorld.
  This prevents Archipelago generation from treating night as reachable while time is frozen.
- Fork-native night Wonder checks still explicitly require Flow of Time.
- Fork-native AP requirements are now accumulated and ANDed together instead of later gates overwriting earlier ones.
- Native SoH Region::ApplyTimePass() refuses to grant day+night access while Flow of Time is shuffled but not owned.
  This fixes the in-game Check Tracker showing night checks as available before Flow of Time.
- Native AccessReset/ResetAllLocations honor FrozenStartingTime (Dusk/Night starts at logical night; Dawn/Day at logical day).
- Kakariko "Wonder Under Construction" requires Roc's Feather when Roc's Feather is enabled, both in native logic and AP logic.
- Roc's Feather no longer forces a 5.0 forward velocity on activation; it preserves natural horizontal motion and only adds the vertical jump impulse. This avoids tunneling/clipping through thin building collision during the jump.
- Includes the v5.6 ArchipelagoClient files so remote-player send notifications remain present.

Install APWorld:
  apworld/soh_extreme_logic_0.5.0.apworld
as:
  C:\ProgramData\Archipelago\custom_worlds\soh_extreme.apworld

Generate a NEW seed after installing the APWorld because access rules changed.

Replace source files:
  soh/Enhancements/randomizer/location_access.cpp
  soh/Enhancements/randomizer/location_access/overworld/kakariko.cpp
  soh/Enhancements/randomizer/RocsFeather.cpp
  soh/Network/Archipelago/ArchipelagoClient.cpp
  soh/Network/Archipelago/ArchipelagoClient.h

Build:
  cmake --build build-vs --config Release --parallel 8
