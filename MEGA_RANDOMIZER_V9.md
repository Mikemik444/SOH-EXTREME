OoT Mega Randomizer - SoH v9

Changes from v8:
- Bushes are now classified from En_Wood02 params. Bushes require Grass Soul; trees require Tree Soul.
- Added Skulltula Soul setting, item, persistent flag, item-pool insertion, solver logic, runtime hiding, and item tracker entry.
- Gold Skulltula checks require Skulltula Soul. Gold Skulltula and token actors do not initialize before the soul is owned.
- NPC hiding is safer: NPC initialization is no longer cancelled. NPCs initialize for scene bookkeeping, then are rendered invisible and cannot be talked to until NPC Soul is owned. This avoids the Kokiri Forest transition crash caused by blanket NPC init suppression.
- Existing Mega Souls tracker section now includes Skulltula Soul.

Important:
- Generate a new seed after installing v9.
- Re-enter an area after receiving a soul so suppressed/hidden actors can appear normally.
- NPC placement shuffle is still not implemented.
