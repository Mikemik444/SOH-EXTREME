from dataclasses import dataclass, fields
from Options import Toggle, Choice, Range, OptionGroup
from worlds.oot_soh.Options import SohOptions, soh_option_groups

class ShuffleRoll(Toggle):
    """Shuffle the ability to roll."""
    display_name = "Shuffle Roll"

class ShuffleGrab(Toggle):
    """Shuffle Link's basic grab/lift ability (Power Bracelet tier)."""
    display_name = "Shuffle Grab / Power Bracelet"

class ShuffleClimb(Toggle):
    """Shuffle the ability to climb climbable surfaces."""
    display_name = "Shuffle Climb"

class ShuffleCrawl(Toggle):
    """Shuffle the ability to crawl through crawlspaces."""
    display_name = "Shuffle Crawl"

class ShuffleSpeak(Toggle):
    """Shuffle the ability to speak to NPCs."""
    display_name = "Shuffle Speak"

class ShuffleOpenChest(Choice):
    """Shuffle chest-opening ability. Progressive requires a second copy for large chests."""
    display_name = "Shuffle Open Chest"
    option_off = 0
    option_on = 1
    option_progressive = 2
    default = 0

class ShuffleEnemySoul(Toggle):
    display_name = "Shuffle Enemy Soul"
class ShuffleNpcSoul(Toggle):
    display_name = "Shuffle NPC Soul"
class ShuffleAnimalSoul(Toggle):
    display_name = "Shuffle Animal Soul"
class ShufflePotSoul(Toggle):
    display_name = "Shuffle Pot Soul"
class ShuffleCrateSoul(Toggle):
    display_name = "Shuffle Crate Soul"
class ShuffleGrassBushSoul(Toggle):
    display_name = "Shuffle Grass / Bush Soul"
class ShuffleRockBoulderSoul(Toggle):
    display_name = "Shuffle Rock / Boulder Soul"
class ShuffleTreeSoul(Toggle):
    display_name = "Shuffle Tree Soul"
class ShuffleBeehiveSoul(Toggle):
    display_name = "Shuffle Beehive Soul"
class ShuffleSignSoul(Toggle):
    display_name = "Shuffle Sign Soul"
class ShuffleSkulltulaSoul(Toggle):
    display_name = "Shuffle Skulltula Soul"
class ShuffleBusinessScrubSoul(Toggle):
    display_name = "Shuffle Business Scrub Soul"
class ShuffleShovel(Toggle):
    display_name = "Shuffle Shovel"
class NpcSpeechSanity(Toggle):
    display_name = "NPC Speech Sanity"

class SongNoteShuffle(Choice):
    display_name = "Song Note Shuffle"
    option_off = 0
    option_individual_notes = 1
    default = 0

class ShuffleFlowOfTime(Toggle):
    display_name = "Shuffle Flow of Time"

class FrozenStartingTime(Choice):
    display_name = "Frozen Starting Time"
    option_random = 0
    option_dawn = 1
    option_day = 2
    option_dusk = 3
    option_night = 4
    default = 0

class TimeSpeedAfterUnlock(Choice):
    display_name = "Time Speed After Unlock"
    option_normal = 0
    option_2x = 1
    option_3x = 2
    option_4x = 3
    default = 0

class ExtremeTrapPool(Choice):
    display_name = "Extreme Trap Pool"
    option_off = 0
    option_ice_only = 1
    option_expanded = 2
    default = 0
class ExtremeIceTraps(Toggle):
    display_name = "Extreme Ice Traps"
class ExtremeFireTraps(Toggle):
    display_name = "Extreme Fire Traps"
class ExtremeSlowTraps(Toggle):
    display_name = "Extreme Slow Traps"
class ExtremeMagicSuckTraps(Toggle):
    display_name = "Extreme Magic Suck Traps"
class ExtremeHealthDrainTraps(Toggle):
    display_name = "Extreme Health Drain Traps"
class ExtremeTrapFillerReplacement(Range):
    display_name = "Extreme Trap Filler Replacement %"
    range_start = 0
    range_end = 100
    default = 0


# Native location families added by the SOH-EXTREME Ship fork but not present in
# the stock Archipelago-SoH option dataclass.  These are first-class AP options
# so an AP save never inherits an invisible local randomizer value.
class ShuffleRocks(Toggle):
    display_name = "Shuffle Rocks"
class ShuffleBoulders(Choice):
    display_name = "Shuffle Boulders"
    option_off = 0
    option_dungeons = 1
    option_overworld = 2
    option_all = 3
    default = 0
class ShuffleBushes(Toggle):
    display_name = "Shuffle Bushes"
class ShuffleIcicles(Toggle):
    display_name = "Shuffle Icicles / Stalagmites"
class ShuffleRedIce(Toggle):
    display_name = "Shuffle Red Ice"
class ShuffleSigns(Choice):
    display_name = "Shuffle Signs"
    option_off = 0
    option_dungeons = 1
    option_overworld = 2
    option_all = 3
    default = 0
class ShuffleBeggar(Toggle):
    display_name = "Shuffle Market Beggar"
class ShuffleChestMinigame(Toggle):
    display_name = "Shuffle Treasure Chest Game"
class ShuffleWonderItems(Choice):
    display_name = "Shuffle Wonder Items"
    option_off = 0
    option_dungeons = 1
    option_overworld = 2
    option_all = 3
    default = 0
class ShuffleSilver(Choice):
    display_name = "Shuffle Silver Rupees"
    option_off = 0
    option_on = 1
    option_wallet = 2
    option_start_with = 3
    default = 0
class ShuffleButterflyFairies(Toggle):
    display_name = "Shuffle Butterfly Fairies"

@dataclass
class SohExtremeOptions(SohOptions):
    shuffle_rocks: ShuffleRocks
    shuffle_boulders: ShuffleBoulders
    shuffle_bushes: ShuffleBushes
    shuffle_icicles: ShuffleIcicles
    shuffle_red_ice: ShuffleRedIce
    shuffle_signs: ShuffleSigns
    shuffle_beggar: ShuffleBeggar
    shuffle_chest_minigame: ShuffleChestMinigame
    shuffle_wonder_items: ShuffleWonderItems
    shuffle_silver: ShuffleSilver
    shuffle_butterfly_fairies: ShuffleButterflyFairies
    shuffle_roll: ShuffleRoll
    shuffle_grab: ShuffleGrab
    shuffle_climb: ShuffleClimb
    shuffle_crawl: ShuffleCrawl
    shuffle_speak: ShuffleSpeak
    shuffle_open_chest: ShuffleOpenChest
    shuffle_enemy_soul: ShuffleEnemySoul
    shuffle_npc_soul: ShuffleNpcSoul
    shuffle_animal_soul: ShuffleAnimalSoul
    shuffle_pot_soul: ShufflePotSoul
    shuffle_crate_soul: ShuffleCrateSoul
    shuffle_grass_bush_soul: ShuffleGrassBushSoul
    shuffle_rock_boulder_soul: ShuffleRockBoulderSoul
    shuffle_tree_soul: ShuffleTreeSoul
    shuffle_beehive_soul: ShuffleBeehiveSoul
    shuffle_sign_soul: ShuffleSignSoul
    shuffle_skulltula_soul: ShuffleSkulltulaSoul
    shuffle_business_scrub_soul: ShuffleBusinessScrubSoul
    shuffle_shovel: ShuffleShovel
    npc_speech_sanity: NpcSpeechSanity
    song_note_shuffle: SongNoteShuffle
    shuffle_flow_of_time: ShuffleFlowOfTime
    frozen_starting_time: FrozenStartingTime
    time_speed_after_unlock: TimeSpeedAfterUnlock
    extreme_trap_pool: ExtremeTrapPool
    extreme_ice_traps: ExtremeIceTraps
    extreme_fire_traps: ExtremeFireTraps
    extreme_slow_traps: ExtremeSlowTraps
    extreme_magic_suck_traps: ExtremeMagicSuckTraps
    extreme_health_drain_traps: ExtremeHealthDrainTraps
    extreme_trap_filler_replacement: ExtremeTrapFillerReplacement

# SohWebWorld has already been created by the time this module is imported.
# Archipelago's WebWorld metaclass mutates soh_option_groups in-place by adding
# the common "Item & Location Options" group. Reusing that injected group in a
# second WebWorld causes a duplicate-options assertion, so copy only SoH's
# game-specific groups. Archipelago will inject the common group for this world.
_base_option_groups = [group for group in soh_option_groups if group.name != "Item & Location Options"]

extreme_option_groups = list(_base_option_groups) + [
    OptionGroup("SOH-EXTREME Fork Location Shuffles", [
        ShuffleRocks, ShuffleBoulders, ShuffleBushes, ShuffleIcicles, ShuffleRedIce,
        ShuffleSigns, ShuffleBeggar, ShuffleChestMinigame, ShuffleWonderItems,
        ShuffleSilver, ShuffleButterflyFairies,
    ]),
    OptionGroup("SOH-EXTREME Abilities & Souls", [
        ShuffleRoll, ShuffleGrab, ShuffleClimb, ShuffleCrawl, ShuffleSpeak, ShuffleOpenChest,
        ShuffleEnemySoul, ShuffleNpcSoul, ShuffleAnimalSoul, ShufflePotSoul, ShuffleCrateSoul,
        ShuffleGrassBushSoul, ShuffleRockBoulderSoul, ShuffleTreeSoul, ShuffleBeehiveSoul,
        ShuffleSignSoul, ShuffleSkulltulaSoul, ShuffleBusinessScrubSoul, ShuffleShovel,
    ]),
    OptionGroup("SOH-EXTREME World Features", [
        NpcSpeechSanity, SongNoteShuffle, ShuffleFlowOfTime, FrozenStartingTime, TimeSpeedAfterUnlock,
    ]),
    OptionGroup("SOH-EXTREME Traps", [
        ExtremeTrapPool, ExtremeIceTraps, ExtremeFireTraps, ExtremeSlowTraps,
        ExtremeMagicSuckTraps, ExtremeHealthDrainTraps, ExtremeTrapFillerReplacement,
    ]),
]


# Archipelago's template generator reads options_dataclass. Because this dataclass
# subclasses the official SoH SohOptions dataclass, every stock SoH 1.4.2 option
# remains present in generated templates. Fail loudly if that ever stops being true.
_BASE_OPTION_NAMES = {field.name for field in fields(SohOptions)}
_EXTREME_OPTION_NAMES = {field.name for field in fields(SohExtremeOptions)} - _BASE_OPTION_NAMES
_missing_base_options = _BASE_OPTION_NAMES - {field.name for field in fields(SohExtremeOptions)}
if _missing_base_options:
    raise RuntimeError(
        "SOH-EXTREME lost inherited Ship of Harkinian options: "
        + ", ".join(sorted(_missing_base_options))
    )
