# OoT Mega Randomizer - SoH v7

This is a source replacement for the contents of `Shipwright\soh`.

## v7 fixes
- Fixes the `GET_PLAYER` compile error by including `macros.h` in `MegaSouls.cpp`.
- Enemy Soul now removes normal enemy actors at actor initialization while the soul is missing, instead of leaving frozen visible enemies.
- Animal Soul now removes the covered animal actors at actor initialization while the soul is missing, instead of leaving frozen visible animals.
- After obtaining Enemy Soul or Animal Soul, leave/re-enter the current room so actors that were skipped can spawn.
- Adds central Glitchless-logic gating for soul-backed randomized location types so the solver/playthrough/tracker cannot count those checks before the matching soul:
  - Pot Soul -> pots
  - Crate Soul -> crates / NL crates / small crates
  - Grass Soul -> grass
  - Rock Soul -> rocks + boulders (v7 still uses the existing combined Rock Soul)
  - Tree Soul -> trees / NL trees
  - Beehive Soul -> beehives
  - Sign Soul -> signs
  - Animal Soul -> cows, fish, butterfly-fairy checks
- Existing Enemy Soul gating in `Logic::CanKillEnemy` remains active.

## Important current limitation
NPC Soul still blocks NPC interaction at runtime, but not every `RCTYPE_STANDARD` NPC/event reward can be identified safely from RC type alone. A later pass should classify individual NPC checks so every NPC-derived check also receives the NPC Soul solver requirement.

This package is source only. Build it inside the full Shipwright checkout.
