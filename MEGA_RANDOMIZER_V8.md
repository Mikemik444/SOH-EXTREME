OoT Mega Randomizer - SoH v8
============================

This package is the Ship of Harkinian `soh` source tree. Extract its contents into:
  E:\test\OoT-Mega-Randomizer-Ship-Edition\Shipwright\soh
and replace existing files.

v8 changes
----------
- Barren-world actor spawning expanded:
  - Enemy Soul: enemies do not initialize until obtained.
  - Animal Soul: cows, Cuccos, dogs, fish, bugs, butterflies, frogs and horses do not initialize until obtained.
  - NPC Soul: NPC-category actors do not initialize until obtained (animals remain controlled by Animal Soul).
  - Pot Soul: normal, flying and wall pots are hidden until obtained.
  - Crate Soul: small/large crates are hidden until obtained.
  - Grass Soul: cuttable grass actors are hidden until obtained.
  - Rock Soul: rocks and supported boulder actors are hidden until obtained.
  - Tree Soul: En_Wood02 tree actors are hidden until obtained.
  - Beehive Soul: beehives are hidden until obtained.
  - Sign Soul: normal signs are hidden until obtained.
- Sign Soul now also gates the alternate shuffled-sign implementations used by En_A_Obj, En_Wonder_Talk and En_Wonder_Talk2. Their randomized sign check is killed/blocked until Sign Soul is obtained, preventing the non-square-sign reward bypass.
- NPC-soul logic now gates randomizer locations whose owning actor is one of OoT's ACTORCAT_NPC actor profiles. This keeps many NPC reward checks and tracker reachability aligned with runtime hiding.
- Added a configurable `Mega Souls` section to the Item Tracker. Enabled category souls are displayed and become active as their RandomizerInf flags are collected. It defaults to the main Item Tracker window and can be changed under Randomizer -> Item Tracker -> General Settings.

Important scope note
--------------------
This version does NOT implement NPC placement/location shuffling. NPC Soul hiding/gating is real, but moving NPCs between compatible spawn locations requires a separate compatibility/scene-script system and is intentionally not faked here.

After obtaining a soul while inside an area, leave and re-enter the room/scene so actors controlled by that soul are spawned again.

Build
-----
From the outer Shipwright directory:

  cmake -S . -B "build\x64" -G "Visual Studio 17 2022" -T v143 -A x64 -DCMAKE_BUILD_TYPE:STRING=Release
  cmake --build .\build\x64 --target GenerateSohOtr --config Release -- /m:1
  cmake --build .\build\x64 --config Release -- /m:1
  copy /Y "build\x64\soh\soh.o2r" "x64\Release\soh.o2r"

Generate a new seed after installing v8 so the solver uses the updated soul requirements.
