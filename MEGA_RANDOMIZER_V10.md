# OoT Mega Randomizer SoH v10

Adds a real shuffled **Shovel** progression item for hidden grottos.

## Shovel behavior
- New `Shuffle Shovel` option in Randomizer -> Shuffles -> Additional Items.
- Shovel is inserted into the normal randomizer item pool and persisted with a RandomizerInf flag.
- Hidden `Door_Ana` grotto actors (bomb/hammer/Storms-style hidden holes) do not initialize before Shovel, so the hole is absent from the world.
- Naturally-open grotto holes remain available without Shovel.
- Hidden-grotto entrance logic requires Shovel **in addition to** the grotto's normal reveal/access requirement.
- The logic gate is centralized in `Entrance::GetConditionsMet()` using `originalConnectedRegion`, so shuffled entrances keep the requirement on the physical source entrance.
- Because the normal region solver uses those entrance conditions, seed generation, playthrough spheres, and the in-game check tracker all account for Shovel.
- Shovel is visible in the Item Tracker under `Mega Progression`.

## Carried forward from v9
- Bushes use Grass Soul while trees use Tree Soul.
- Skulltula Soul.
- Category soul location logic and item tracking.
- Barren actor hiding for soul-gated categories.
