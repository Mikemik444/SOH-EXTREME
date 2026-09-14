# OoT Mega Randomizer - SoH v6

This source tree is a direct Ship of Harkinian `soh` modification. There is no Java launcher or external mega config.

Implemented directly in SoH in this revision:
- Shuffle Roll: Roll is a real shuffled progression item; runtime blocks starting a roll until obtained; tree-bonk logic requires it.
- Global Enemy Soul: real shuffled item, persistent flag, fill-pool item, logic gate, and runtime enemy update gate.
- Global NPC Soul: real shuffled item, persistent flag, fill-pool item, logic gate for shuffled speech and runtime NPC-talk gate.
- Global Animal Soul: real shuffled item and runtime gate for cows, cuccos, dogs, fish, bugs, butterflies, frogs, and horses.
- Pot, Crate, Grass, Rock, Tree, Beehive and Sign Souls: real shuffled items with persistent flags and logic gates. Runtime gates are active for pots, crates, grass, rocks/boulders, and beehives.
- Enemy Drop Shuffle: native SoH randomizer setting that replaces ordinary enemy random-drop table selection with a fully randomized vanilla drop-table entry.
- Existing SoH sanity systems remain available: pots, crates, grass, rocks, trees, beehives, signs, cows, fish, fairies, wonder items, freestanding, songs, Speak, Open Chest, Climb, Crawl, Grab, Bean Souls and Boss Souls.

Important current limits:
- Soul mode is GLOBAL per category in v6; per-species/per-individual souls are not yet implemented.
- Enemy Drop Shuffle randomizes physical drops; individual enemy-drop-as-randomizer-location checks are not yet implemented in this revision.
- Animal/NPC placement shuffle and every-NPC-talk-as-a-location are not yet implemented here.
- Tree and Sign souls currently gate randomizer logic; runtime hard-blocking is not added yet because those actor classes contain mixed-purpose actors and need safer per-instance classification.

Build by replacing your existing `Shipwright\soh` folder with this folder, then rebuild Shipwright as before.
