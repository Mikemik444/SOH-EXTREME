from typing import ClassVar
import dataclasses
import logging
from BaseClasses import Item, ItemClassification
from worlds.oot_soh import SohWorld, SohWebWorld, SohSettings
from worlds.oot_soh.Items import SohItem
from worlds.oot_soh.Locations import LocTag, SohLocation
from rule_builder.rules import Rule, Has, And
from worlds.oot_soh.Enums import Ages
from worlds.AutoWorld import LogicMixin
from .Options import SohExtremeOptions, extreme_option_groups
from .ForkLocations import FORK_LOCATIONS, FORK_LOCATION_NAME_TO_ID

logger = logging.getLogger("SOH_EXTREME")

EXTREME_ITEMS = {
    "Roll": 9500000,
    "Grab / Power Bracelet": 9500001,
    "Climb": 9500002,
    "Crawl": 9500003,
    "Speak": 9500004,
    "Open Chest": 9500005,
    "Enemy Soul": 9500006,
    "NPC Soul": 9500007,
    "Animal Soul": 9500008,
    "Pot Soul": 9500009,
    "Crate Soul": 9500010,
    "Grass / Bush Soul": 9500011,
    "Rock / Boulder Soul": 9500012,
    "Tree Soul": 9500013,
    "Beehive Soul": 9500014,
    "Sign Soul": 9500015,
    "Skulltula Soul": 9500016,
    "Business Scrub Soul": 9500017,
    "Shovel": 9500018,
    "Flow of Time": 9500019,
}
for i in range(74):
    EXTREME_ITEMS[f"Song Note {i + 1:02d}"] = 9500020 + i



@dataclasses.dataclass()
class RequireExistingLocationItem(Rule, game="SOH-EXTREME"):
    """Compose a hard SOH-EXTREME item gate with an existing resolved SoH rule.

    Archipelago 0.6.7 has no ``And.from_resolved`` helper.  Stock SoH has
    already resolved its access rule by the time this rule is installed, so we
    explicitly create an ``And.Resolved`` node containing that existing rule
    and the resolved Extreme item requirement.  This preserves cache
    dependencies and human-readable explanations for Universal Tracker.
    """
    location_name: str
    item_name: str
    count: int = 1

    def _instantiate(self, world):
        existing = world.get_location(self.location_name).access_rule
        required = Has(self.item_name, self.count).resolve(world)

        if existing.always_false:
            return existing
        if existing.always_true:
            return required
        if required.always_false:
            return required
        if required.always_true:
            return existing

        # ``And.Resolved`` is the 0.6.7-supported way to combine already
        # resolved rules.  NestedRule.Resolved accepts the child tuple first.
        return And.Resolved(
            (existing, required),
            player=world.player,
            caching_enabled=getattr(world, "rule_caching_enabled", False),
        )


class SohExtremeItem(SohItem):
    game = "SOH-EXTREME"

class SohExtremeLocation(SohLocation):
    game = "SOH-EXTREME"


class SohExtremeLogicState(LogicMixin):
    """Register SOH-EXTREME players with the stock SoH CollectionState logic.

    The official SoH APWorld initializes its age/heart state only for players
    whose game name is exactly "Ship of Harkinian".  SOH-EXTREME reuses
    those rules, so its players must be added to the same state dictionaries.
    """
    def init_mixin(self, parent):
        players = list(parent.get_game_players("SOH-EXTREME") + parent.get_game_groups("SOH-EXTREME"))
        for player in players:
            self._soh_stale[player] = True
            self._soh_child_reachable_regions[player] = set()
            self._soh_adult_reachable_regions[player] = set()
            self._soh_child_blocked_regions[player] = set()
            self._soh_adult_blocked_regions[player] = set()
            self._soh_age[player] = Ages.null
            self.soh_piece_of_heart_count[player] = 0
            self.soh_heart_count[player] = parent.worlds[player].options.starting_hearts.value

        # Tracks virtual stock-song progression created by collecting every
        # individual note for a song.  The stock SoH access rules still ask for
        # the normal song item names, while the SOH-EXTREME client grants the
        # quest song after all of that song's notes have arrived.
        self._soh_extreme_virtual_songs = {player: set() for player in players}

    def copy_mixin(self, new_state):
        new_state._soh_extreme_virtual_songs = {
            player: songs.copy() for player, songs in self._soh_extreme_virtual_songs.items()
        }
        # CollectionState.copy() chains every registered LogicMixin copy hook as
        #     ret = function(self, ret)
        # so every hook MUST return the copied state.  Returning None here breaks
        # whichever world mixin runs after SOH-EXTREME (for example Super Metroid).
        return new_state

class SohExtremeWebWorld(SohWebWorld):
    option_groups = extreme_option_groups

class SohExtremeWorld(SohWorld):
    """SOH-EXTREME: Ship of Harkinian with Mega Randomizer abilities, souls, notes, time, and traps."""
    game = "SOH-EXTREME"
    web = SohExtremeWebWorld()
    options: SohExtremeOptions
    options_dataclass = SohExtremeOptions
    # Used by Archipelago's Generate Template Options / Options Creator.
    # SohExtremeOptions inherits every official SohOptions field and appends the Extreme fields.
    settings: ClassVar[SohSettings]
    # AutoWorldRegister requires BOTH mappings to be present directly on any
    # class that declares a new game name. Inheriting location_name_to_id is not
    # enough and causes the APWorld to fail during import/template generation.
    item_name_to_id = dict(SohWorld.item_name_to_id) | EXTREME_ITEMS
    location_name_to_id = dict(SohWorld.location_name_to_id) | FORK_LOCATION_NAME_TO_ID

    # Preserve all stock SoH item/location groups too. AutoWorldRegister rebuilds
    # these for every newly registered game class.
    item_name_groups = {k: set(v) for k, v in SohWorld.item_name_groups.items() if k != "Everything"} | {
        "Extreme Abilities": {"Roll", "Grab / Power Bracelet", "Climb", "Crawl", "Speak", "Open Chest", "Shovel", "Flow of Time"},
        "Extreme Souls": {name for name in EXTREME_ITEMS if name.endswith("Soul")},
        "Song Notes": {name for name in EXTREME_ITEMS if name.startswith("Song Note ")},
    }
    location_name_groups = {
        k: set(v) for k, v in SohWorld.location_name_groups.items() if k != "Everywhere"
    } | {
        "SOH-EXTREME Fork Locations": set(FORK_LOCATION_NAME_TO_ID),
        "Wonder Items": {loc.name for loc in FORK_LOCATIONS if loc.family == "wonder"},
    }

    # Reuse the official SoH host.yaml settings group. Without this, the subclass
    # would look for a non-existent `soh_extreme_options` settings section and
    # base SoH generation can later crash when it accesses self.settings.
    settings_key = SohWorld.settings_key


    def _fork_location_enabled(self, loc) -> bool:
        o = self.options
        if loc.family == "rock": return bool(o.shuffle_rocks.value)
        if loc.family == "boulder": return o.shuffle_boulders.value == 3 or (o.shuffle_boulders.value == 1 and loc.dungeon) or (o.shuffle_boulders.value == 2 and not loc.dungeon)
        if loc.family == "bush": return bool(o.shuffle_bushes.value)
        if loc.family == "icicle": return bool(o.shuffle_icicles.value)
        if loc.family == "red_ice": return bool(o.shuffle_red_ice.value)
        if loc.family == "sign": return o.shuffle_signs.value == 3 or (o.shuffle_signs.value == 1 and loc.dungeon) or (o.shuffle_signs.value == 2 and not loc.dungeon)
        if loc.family == "beggar": return bool(o.shuffle_beggar.value)
        if loc.family == "chest_minigame": return bool(o.shuffle_chest_minigame.value)
        if loc.family == "wonder": return o.shuffle_wonder_items.value == 3 or (o.shuffle_wonder_items.value == 1 and loc.dungeon) or (o.shuffle_wonder_items.value == 2 and not loc.dungeon)
        # Start With silver rupees satisfies the puzzle state instead of exposing the individual rupees as AP checks.
        if loc.family == "silver": return o.shuffle_silver.value in (1, 2)
        if loc.family == "butterfly_fairy": return bool(o.shuffle_butterfly_fairies.value)
        return False

    def create_regions(self) -> None:
        super().create_regions()
        added = 0
        for loc in FORK_LOCATIONS:
            if not self._fork_location_enabled(loc):
                continue
            region = self.multiworld.get_region(str(loc.region), self.player)
            region.locations.append(SohExtremeLocation(self.player, loc.name, loc.address, region))
            added += 1
        logger.info("SOH-EXTREME added %d fork-native AP locations", added)

    def generate_early(self) -> None:
        super().generate_early()

        # The Extreme Soul options gate these object types in the game.  If a
        # corresponding sanity option is left Off, the Extreme item pool can be
        # larger than the stock SoH check pool.  Enable the matching stock check
        # family so the Soul has checks to gate and so every Extreme item has a
        # legitimate Archipelago location slot.
        #
        # Choice values in the stock SoH APWorld:
        #   shuffle_pots / shuffle_crates / shuffle_grass: 0=off, 3=all
        #   shuffle_trees / shuffle_beehives: Toggle, 0=off, 1=on
        if self.options.shuffle_pot_soul.value and self.options.shuffle_pots.value == 0:
            self.options.shuffle_pots.value = 3
        if self.options.shuffle_crate_soul.value and self.options.shuffle_crates.value == 0:
            self.options.shuffle_crates.value = 3
        if self.options.shuffle_grass_bush_soul.value and self.options.shuffle_grass.value == 0:
            self.options.shuffle_grass.value = 3
        if self.options.shuffle_tree_soul.value and self.options.shuffle_trees.value == 0:
            self.options.shuffle_trees.value = 1
        if self.options.shuffle_beehive_soul.value and self.options.shuffle_beehives.value == 0:
            self.options.shuffle_beehives.value = 1

    def create_item(self, name: str, create_as_event: bool = False, classification: ItemClassification = None):
        if name in EXTREME_ITEMS:
            return SohExtremeItem(name, classification or ItemClassification.progression,
                                  None if create_as_event else EXTREME_ITEMS[name], self.player)
        base = super().create_item(name, create_as_event, classification)
        return SohExtremeItem(base.name, base.classification, base.code, base.player)

    SONG_ITEMS = {
        "Zelda's Lullaby", "Epona's Song", "Saria's Song", "Sun's Song",
        "Song of Time", "Song of Storms", "Minuet of Forest", "Bolero of Fire",
        "Serenade of Water", "Requiem of Spirit", "Nocturne of Shadow", "Prelude of Light",
    }

    # Must match ArchipelagoClient::RefreshSongNotes() in the SOH fork.
    # The 74 AP note items are contiguous, and each tuple gives the normal SoH
    # song item unlocked once every note in that slice has been collected.
    SONG_NOTE_LAYOUT = (
        ("Zelda's Lullaby", 6),
        ("Epona's Song", 6),
        ("Saria's Song", 6),
        ("Sun's Song", 6),
        ("Song of Time", 6),
        ("Song of Storms", 6),
        ("Minuet of Forest", 6),
        ("Bolero of Fire", 8),
        ("Serenade of Water", 5),
        ("Requiem of Spirit", 6),
        ("Nocturne of Shadow", 7),
        ("Prelude of Light", 6),
    )

    SONG_NOTE_GROUPS = {}
    _song_note_offset = 0
    for _song_name, _song_note_count in SONG_NOTE_LAYOUT:
        SONG_NOTE_GROUPS[_song_name] = tuple(
            f"Song Note {i + 1:02d}"
            for i in range(_song_note_offset, _song_note_offset + _song_note_count)
        )
        _song_note_offset += _song_note_count
    del _song_note_offset, _song_name, _song_note_count

    @classmethod
    def _song_for_note(cls, note_name: str):
        for song_name, notes in cls.SONG_NOTE_GROUPS.items():
            if note_name in notes:
                return song_name, notes
        return None, None

    def _refresh_virtual_song_for_note(self, state, note_name: str) -> bool:
        """Mirror the client's note->song unlock in Archipelago logic.

        Stock SoH rules use the twelve normal song names.  With individual note
        shuffle those normal song items are removed from the pool, so once all
        notes for one song have been collected we add one logical copy of the
        stock song to ``prog_items``.  It is removed again if fill simulation
        removes a required note.
        """
        song_name, notes = self._song_for_note(note_name)
        if song_name is None:
            return False

        virtual = state._soh_extreme_virtual_songs.setdefault(self.player, set())
        complete = all(state.has(required_note, self.player) for required_note in notes)

        if complete and song_name not in virtual:
            # IMPORTANT: do not mutate state.prog_items directly here. The SoH
            # rules use CachedRuleBuilder, and direct Counter mutation does not
            # invalidate Has(song) dependencies that may already be cached false.
            # Route the virtual song through CollectionState.collect so the normal
            # world collect hook and rule-cache invalidation run exactly as if the
            # stock song item had been collected.
            virtual.add(song_name)
            state.collect(self.create_item(song_name, create_as_event=True), True)
            state._soh_stale[self.player] = True
            return True

        if not complete and song_name in virtual:
            # Same reason as above: use CollectionState.remove so cached rules
            # depending on the stock song are invalidated during fill simulation.
            virtual.remove(song_name)
            state.remove(self.create_item(song_name, create_as_event=True))
            state._soh_stale[self.player] = True
            return True

        return False

    def collect(self, state, item: Item) -> bool:
        changed = super().collect(state, item)
        if self.options.song_note_shuffle.value and item.name.startswith("Song Note "):
            changed = self._refresh_virtual_song_for_note(state, item.name) or changed
        return changed

    def remove(self, state, item: Item) -> bool:
        changed = super().remove(state, item)
        if self.options.song_note_shuffle.value and item.name.startswith("Song Note "):
            changed = self._refresh_virtual_song_for_note(state, item.name) or changed
        return changed

    HEALTH_REPLACEMENT_ITEMS = {
        "Piece of Heart", "Heart Container", "Piece of Heart (WINNER)",
    }

    def _remove_pool_item(self, item) -> None:
        self.item_pool.remove(item)
        if item in self.multiworld.itempool:
            self.multiworld.itempool.remove(item)

    def _remove_replaceable_items(self, count: int, replace_songs: bool = False) -> None:
        """Make room for Extreme items without removing real SoH progression.

        Stock SoH keeps progression/useful pool entries in ``self.item_pool``, but
        ``create_filler_item_pool`` appends junk and Ice Traps directly to
        ``multiworld.itempool``.  The older SOH-EXTREME build only searched
        ``self.item_pool``, so it could not see most of the available filler and
        eventually started precollecting important stock items.

        This version replaces, in order:
          1. the 12 full-song items when Individual Song Notes are enabled;
          2. filler/trap/non-progression entries from the *actual global pool*;
          3. optional heart upgrades only if an unusual configuration still needs
             a few more slots.

        Dungeon keys, equipment, rewards, overworld door keys, Skeleton Key, etc.
        are never precollected or deleted by this function.
        """
        removed = 0

        def remove_item(item) -> None:
            nonlocal removed
            if item in self.multiworld.itempool:
                self.multiworld.itempool.remove(item)
            if item in self.item_pool:
                self.item_pool.remove(item)
            removed += 1

        # Individual notes replace the normal twelve songs first.
        if replace_songs:
            for item in list(reversed(self.multiworld.itempool)):
                if removed >= count:
                    break
                if item.player == self.player and item.name in self.SONG_ITEMS:
                    remove_item(item)

        # IMPORTANT: scan multiworld.itempool, not just self.item_pool.
        # SoH's create_filler_item_pool() puts its filler and traps directly here.
        for item in list(reversed(self.multiworld.itempool)):
            if removed >= count:
                break
            if item.player != self.player:
                continue
            if item.classification & ItemClassification.progression:
                continue
            remove_item(item)

        # Last-resort optional health slots.  These are still preferable to
        # touching any required key/equipment/reward progression.
        if removed < count:
            for item in list(reversed(self.multiworld.itempool)):
                if removed >= count:
                    break
                if item.player != self.player or item.name not in self.HEALTH_REPLACEMENT_ITEMS:
                    continue
                remove_item(item)

        if removed != count:
            raise Exception(
                f"SOH-EXTREME needs {count} pool slots but only found {removed} replaceable slots. "
                "Enable the corresponding object sanity checks or use a less restrictive stock item pool."
            )

    # SOH-EXTREME logic bridge.  These are not tracker-only rules: generation,
    # spoiler spheres, hints and Universal Tracker all consume the same rules.
    # They mirror the fork's central MegaSoulAllowsLocation()/interaction gates.
    def set_rules(self) -> None:
        super().set_rules()
        o = self.options

        animal_tags = LocTag.Cow | LocTag.Overworld_Fish | LocTag.Grotto_Fish | LocTag.Pond_Fish
        grass_tags = LocTag.Overworld_Grass | LocTag.Grotto_Grass | LocTag.Dungeon_Grass
        npc_tags = (LocTag.Shop | LocTag.Scrub | LocTag.Merchant | LocTag.Trade_Location |
                    LocTag.Shooting_Minigame | LocTag.House_of_Skulltula_Reward)

        # Large chests in the fork use the second Progressive Open Chest level.
        large_chest_names = {
            name for name in self.included_locations
            if any(k in str(name) for k in (
                'Map Chest', 'Compass Chest', 'Boss Key Chest', 'Bow Chest',
                'Boomerang Chest', 'Slingshot Chest', 'Iron Boots Chest',
                'Hover Boots Chest', 'Longshot Chest', 'Mirror Shield Chest',
                'Silver Gauntlets Chest', 'Hookshot Chest'))
        }

        def require(location, item_name: str, count: int = 1):
            # Preserve the stock SoH rule and compose the Extreme requirement
            # using the same rule-builder graph Universal Tracker understands.
            self.set_rule(location, RequireExistingLocationItem(location.name, item_name, count))

        for location in self.get_locations():
            data = self.included_locations.get(location.name)
            tags = data.tags if data is not None and data.tags is not None else LocTag(0)

            # Chest-opening ability is a hard gameplay gate in the fork.
            if o.shuffle_open_chest.value and (tags & LocTag.Chest):
                count = 2 if o.shuffle_open_chest.value == 2 and location.name in large_chest_names else 1
                require(location, 'Open Chest', count)

            if o.shuffle_pot_soul.value and (tags & LocTag.Pot):
                require(location, 'Pot Soul')
            if o.shuffle_crate_soul.value and (tags & LocTag.Crate):
                require(location, 'Crate Soul')
            if o.shuffle_grass_bush_soul.value and (tags & grass_tags):
                require(location, 'Grass / Bush Soul')
            if o.shuffle_tree_soul.value and (tags & LocTag.Tree):
                require(location, 'Tree Soul')
            if o.shuffle_roll.value and (tags & LocTag.Tree):
                require(location, 'Roll')
            if o.shuffle_beehive_soul.value and (tags & LocTag.Bee_Hive):
                require(location, 'Beehive Soul')
            if o.shuffle_skulltula_soul.value and (tags & LocTag.Gold_Skulltula):
                require(location, 'Skulltula Soul')
            if o.shuffle_enemy_soul.value and (tags & (LocTag.Gold_Skulltula | LocTag.Boss)):
                require(location, 'Enemy Soul')
            if o.shuffle_animal_soul.value and (tags & animal_tags):
                require(location, 'Animal Soul')

            # Business scrubs are their own soul class in SOH-EXTREME.
            if o.shuffle_business_scrub_soul.value and (tags & LocTag.Scrub):
                require(location, 'Business Scrub Soul')

            # The single AP Speak item grants every native Speak_* flag.  Native
            # speak checks also require NPC Soul when that option is active.
            if o.shuffle_speak.value and (tags & npc_tags):
                require(location, 'Speak')
            if o.shuffle_npc_soul.value and (tags & npc_tags):
                require(location, 'NPC Soul')

        # Native one-off gates which are not represented by stock SoH LocTags.
        specials = {
            'LLR Freestanding PoH': [('Grab / Power Bracelet', o.shuffle_grab.value), ('Crawl', o.shuffle_crawl.value)],
            'Graveyard Dampe Gravedigging Tour': [('Shovel', o.shuffle_shovel.value)],
            'Spirit Temple MQ Crawlspace Boulder': [('Crawl', o.shuffle_crawl.value)],
        }
        for location_name, requirements in specials.items():
            try:
                location = self.get_location(location_name)
            except Exception:
                continue
            for item_name, enabled in requirements:
                if enabled:
                    require(location, item_name)

    def create_items(self) -> None:
        super().create_items()
        extras: list[str] = []
        o = self.options
        toggles = [
            (o.shuffle_roll, "Roll"),
            (o.shuffle_grab, "Grab / Power Bracelet"),
            (o.shuffle_climb, "Climb"),
            (o.shuffle_crawl, "Crawl"),
            (o.shuffle_speak, "Speak"),
            (o.shuffle_enemy_soul, "Enemy Soul"),
            (o.shuffle_npc_soul, "NPC Soul"),
            (o.shuffle_animal_soul, "Animal Soul"),
            (o.shuffle_pot_soul, "Pot Soul"),
            (o.shuffle_crate_soul, "Crate Soul"),
            (o.shuffle_grass_bush_soul, "Grass / Bush Soul"),
            (o.shuffle_rock_boulder_soul, "Rock / Boulder Soul"),
            (o.shuffle_tree_soul, "Tree Soul"),
            (o.shuffle_beehive_soul, "Beehive Soul"),
            (o.shuffle_sign_soul, "Sign Soul"),
            (o.shuffle_skulltula_soul, "Skulltula Soul"),
            (o.shuffle_business_scrub_soul, "Business Scrub Soul"),
            (o.shuffle_shovel, "Shovel"),
            (o.shuffle_flow_of_time, "Flow of Time"),
        ]
        extras.extend(name for enabled, name in toggles if enabled.value)
        if o.shuffle_open_chest.value == 1:
            extras.append("Open Chest")
        elif o.shuffle_open_chest.value == 2:
            extras.extend(["Open Chest", "Open Chest"])
        if o.song_note_shuffle.value:
            extras.extend(f"Song Note {i + 1:02d}" for i in range(74))

        if extras:
            self._remove_replaceable_items(len(extras), replace_songs=bool(o.song_note_shuffle.value))
            new_items = [self.create_item(name) for name in extras]
            self.item_pool.extend(new_items)
            self.multiworld.itempool.extend(new_items)

    def fill_slot_data(self):
        data = super().fill_slot_data()
        o = self.options
        # Publish the exact server-side location set for this generated slot.
        # The C++ client uses this to send LocationScouts only for locations that
        # actually exist under the selected shuffle options.
        active_locations = sorted({
            int(location.address)
            for location in self.multiworld.get_locations(self.player)
            if isinstance(location.address, int)
        })

        # Translate every official SoH slot option that has an equivalent in this
        # SOH-EXTREME fork into the fork's actual gRando.Settings CVar name/value.
        # This gives the native client one authoritative settings snapshot to apply
        # before Randomizer_InitSaveFile() instead of relying on whatever happened
        # to be selected in the local randomizer menu.
        cvars = {}
        def cv(name, value):
            cvars[name] = int(value)

        # World access / bridge / trials.
        cv("ClosedForest", data["closed_forest"])
        cv("DoorOfTime", data["door_of_time"])
        cv("ZorasFountain", data["zoras_fountain"])
        cv("SleepingWaterfall", data["sleeping_waterfall"])
        cv("JabuJabu", data["jabu_jabu"])
        cv("LockOverworldDoors", data["lock_overworld_doors"])
        cv("FortressCarpenters", data["fortress_carpenters"])
        cv("RainbowBridge", 8 if data["rainbow_bridge"] == 7 else data["rainbow_bridge"])
        cv("BridgeRewardOptions", data["rainbow_bridge_greg_modifier"])
        cv("StoneCount", data["rainbow_bridge_stones_required"])
        cv("MedallionCount", data["rainbow_bridge_medallions_required"])
        cv("RewardCount", data["rainbow_bridge_dungeon_rewards_required"])
        cv("DungeonCount", data["rainbow_bridge_dungeons_required"])
        cv("TokenCount", data["rainbow_bridge_skull_tokens_required"])
        cv("GanonTrial", data["ganons_trials"])
        cv("GanonTrialCount", data["ganons_trials_count"])
        cv("MedallionLockedTrials", data["medallion_locked_trials"])

        # Goal / songs / core item shuffles.
        cv("TriforceHuntTotalPieces", data["triforce_hunt_pieces_total"] if data["triforce_hunt"] else 0)
        cv("ShuffleSongs", data["shuffle_songs"])
        cv("ShuffleTokens", data["shuffle_skull_tokens"])
        cv("GsExpectSunsSong", data["skulls_sun_song"])
        cv("ShuffleKokiriSword", data["shuffle_kokiri_sword"])
        cv("ShuffleMasterSword", data["shuffle_master_sword"])
        cv("ShuffleChildWallet", data["shuffle_childs_wallet"])
        cv("IncludeTycoonWallet", data["shuffle_tycoon_wallet"])
        cv("ShuffleOcarinas", data["shuffle_ocarinas"])
        cv("ShuffleOcarinaButtons", data["shuffle_ocarina_buttons"])
        cv("ShuffleSwim", data["shuffle_swim"])
        cv("ShuffleGerudoToken", data["shuffle_gerudo_membership_card"])
        cv("ShuffleWeirdEgg", data["shuffle_weird_egg"])
        cv("ShuffleFishingPole", data["shuffle_fishing_pole"])
        cv("ShuffleDekuStickBag", data["shuffle_deku_stick_bag"])
        cv("ShuffleDekuNutBag", data["shuffle_deku_nut_bag"])

        # Location families.  Fishsanity and dungeon rewards use different enum
        # layouts in this fork, so translate them instead of copying raw values.
        cv("ShuffleRocks", o.shuffle_rocks.value)
        cv("ShuffleBoulders", o.shuffle_boulders.value)
        cv("ShuffleBushes", o.shuffle_bushes.value)
        cv("ShuffleIcicles", o.shuffle_icicles.value)
        cv("ShuffleRedIce", o.shuffle_red_ice.value)
        cv("ShuffleSigns", o.shuffle_signs.value)
        cv("ShuffleBeggar", o.shuffle_beggar.value)
        cv("ShuffleChestMinigame", o.shuffle_chest_minigame.value)
        cv("ShuffleWonderItems", o.shuffle_wonder_items.value)
        cv("ShuffleSilver", o.shuffle_silver.value)
        cv("ShuffleButterflyFairies", o.shuffle_butterfly_fairies.value)
        # Enemy-drop sanity has no stable RandomizerCheck table in this fork, so AP mode must
        # explicitly turn it off instead of inheriting a hidden local value.
        cv("ShuffleEnemyDrops", 0)
        cv("ShuffleFreestanding", data["shuffle_freestanding_items"])
        cv("Shopsanity", 1 if data["shuffle_shops"] else 0)
        cv("ShopsanityCount", data["shuffle_shops_item_amount"])
        cv("Fishsanity", {0: 0, 1: 2, 2: 3, 3: 4}.get(data["shuffle_fish"], 0))
        cv("ShuffleScrubs", data["shuffle_scrubs"])
        cv("ShuffleBeehives", data["shuffle_beehives"])
        cv("ShuffleCows", data["shuffle_cows"])
        cv("ShufflePots", data["shuffle_pots"])
        cv("ShuffleCrates", data["shuffle_crates"])
        cv("ShuffleTrees", data["shuffle_trees"])
        cv("ShuffleMerchants", data["shuffle_merchants"])
        cv("ShuffleFrogSongRupees", data["shuffle_frog_song_rupees"])
        cv("ShuffleAdultTrade", data["shuffle_adult_trade_items"])
        cv("ShuffleFountainFairies", data["shuffle_fountain_fairies"])
        cv("ShuffleStoneFairies", data["shuffle_stone_fairies"])
        cv("ShuffleBeanFairies", data["shuffle_bean_fairies"])
        cv("ShuffleFairySpots", data["shuffle_song_fairies"])
        cv("ShuffleGrass", data["shuffle_grass"])
        cv("ShuffleDungeonReward", {0: 0, 1: 1, 2: 3, 3: 4, 4: 5}.get(data["shuffle_dungeon_rewards"], 1))

        # Dungeon items / Ganon BK / key rings.
        cv("StartingMapsCompasses", data["maps_and_compasses"])
        gbk = data["ganons_castle_boss_key"]
        cv("ShuffleGanonBossKey", {0: 0, 1: 5, 2: 5, 3: 6, 4: 7, 5: 8, 6: 9, 7: 10}.get(gbk, 0))
        cv("GbkRewardOptions", data["ganons_castle_boss_key_greg_modifier"])
        cv("GbkStoneCount", data["ganons_castle_boss_key_stones_required"])
        cv("GbkMedallionCount", data["ganons_castle_boss_key_medallions_required"])
        cv("GbkRewardCount", data["ganons_castle_boss_key_dungeon_rewards_required"])
        cv("GbkDungeonCount", data["ganons_castle_boss_key_dungeons_required"])
        cv("GbkTokenCount", data["ganons_castle_boss_key_skull_tokens_required"])
        cv("Keysanity", data["small_key_shuffle"])
        cv("GerudoKeys", data["gerudo_fortress_key_shuffle"])
        cv("BossKeysanity", data["boss_key_shuffle"])
        cv("ShuffleKeyRings", data["key_rings"])
        cv("ShuffleKeyRingsRandomCount", data["key_rings_count"])
        for ap_key, cvar_key in (
            ("gerudo_fortress_key_ring", "ShuffleKeyRingsGerudoFortress"),
            ("forest_temple_key_ring", "ShuffleKeyRingsForestTemple"),
            ("fire_temple_key_ring", "ShuffleKeyRingsFireTemple"),
            ("water_temple_key_ring", "ShuffleKeyRingsWaterTemple"),
            ("spirit_temple_key_ring", "ShuffleKeyRingsSpiritTemple"),
            ("shadow_temple_key_ring", "ShuffleKeyRingsShadowTemple"),
            ("bottom_of_the_well_key_ring", "ShuffleKeyRingsBottomOfTheWell"),
            ("gerudo_training_ground_key_ring", "ShuffleKeyRingsGTG"),
            ("ganons_castle_key_ring", "ShuffleKeyRingsGanonsCastle"),
        ):
            # Local selection is ternary No/Random/Yes. AP has already resolved
            # random/count into a final boolean per dungeon.
            cv(cvar_key, 2 if data[ap_key] else 0)

        # Quest skips and starting inventory.
        cv("BigPoeTargetCount", data["big_poe_target_count"])
        cv("SkipEponaRace", data["skip_epona_race"])
        cv("SkipScarecrowsSong", data["skip_scarecrows_song"])
        cv("LinksPocket", data["start_with_links_pocket"])
        cv("StartingKokiriSword", data["start_with_kokiri_sword"])
        cv("StartingDekuShield", data["start_with_deku_shield"])
        cv("StartingMasterSword", data["start_with_master_sword"])
        cv("StartingOcarina", data["start_with_ocarina"])
        cv("StartingSticks", data["start_with_stick_ammo"])
        cv("StartingNuts", data["start_with_nut_ammo"])
        cv("StartingBeans", data["start_with_magic_beans"])
        for ap_key, cvar_key in (
            ("start_with_zeldas_lullaby", "StartingZeldasLullaby"),
            ("start_with_eponas_song", "StartingEponasSong"),
            ("start_with_sarias_song", "StartingSariasSong"),
            ("start_with_suns_song", "StartingSunsSong"),
            ("start_with_song_of_time", "StartingSongOfTime"),
            ("start_with_song_of_storms", "StartingSongOfStorms"),
            ("start_with_minuet", "StartingMinuetOfForest"),
            ("start_with_bolero", "StartingBoleroOfFire"),
            ("start_with_serenade", "StartingSerenadeOfWater"),
            ("start_with_requiem", "StartingRequiemOfSpirit"),
            ("start_with_nocturne", "StartingNocturneOfShadow"),
            ("start_with_prelude", "StartingPreludeOfLight"),
        ):
            cv(cvar_key, data[ap_key])
        cv("FullWallets", data["full_wallets"])
        cv("BombchuBag", data["bombchu_bag"])
        cv("EnableBombchuDrops", data["bombchu_drops"])
        cv("BlueFireArrows", data["blue_fire_arrows"])
        cv("SunlightArrows", data["sunlight_arrows"])
        cv("RocsFeather", data["rocs_feather"])
        cv("InfiniteUpgrades", data["infinite_upgrades"])
        cv("SkeletonKey", data["skeleton_key"])
        cv("SlingBowBeehives", data["slingbow_break_beehives"])
        cv("StartingAge", data["starting_age"])
        cv("SelectedStartingAge", data["starting_age"])
        cv("Shuffle100GSReward", data["shuffle_100_gs_reward"])
        cv("StartingHearts", max(0, data["starting_hearts"] - 1))

        # Boss souls in current SoH split Ganon's soul out into its own setting.
        cv("ShuffleBossSouls", 1 if data["shuffle_boss_souls"] else 0)
        cv("ShuffleGanonsSoul", 3 if data["shuffle_boss_souls"] == 2 else 0)

        # Traps / logic / hints.
        cv("BaseIceTraps", 1)
        cv("AdditionalIceTraps", data["ice_trap_count"])
        cv("IceTrapPercent", data["ice_trap_filler_replacement"])
        cv("LogicRules", 1 if data["no_logic"] else 0)
        for ap_key, cvar_key in (
            ("hint_clarity", "HintClarity"), ("gossip_stone_hints", "GossipStoneHints"),
            ("tot_altar_hint", "AltarHint"), ("ganondorf_hint", "GanondorfHint"),
            ("sheik_la_hint", "SheikLAHint"), ("boss_key_hint", "BossKeyHint"),
            ("dampe_diary_hint", "DampeHint"), ("greg_hint", "GregHint"),
            ("saria_hint", "SariaHint"), ("mido_hint", "MidoHint"),
            ("frog_game_hint", "FrogsHint"), ("ocarina_of_time_hint", "OoTHint"),
            ("big_goron_hint", "BiggoronHint"), ("big_poe_hint", "BigPoesHint"),
            ("chicken_hint", "ChickensHint"), ("malon_hint", "MalonHint"),
            ("horseback_archery_hint", "HBAHint"), ("fishing_pole_hint", "FishingPoleHint"),
            ("scrub_hints", "ScrubText"), ("merchant_hints", "MerchantText"),
            ("gs_10_hint", "10GSHint"), ("gs_20_hint", "20GSHint"),
            ("gs_30_hint", "30GSHint"), ("gs_40_hint", "40GSHint"),
            ("gs_50_hint", "50GSHint"), ("gs_100_hint", "100GSHint"),
            ("mask_shop_hint", "MaskShopHint"),
        ):
            cv(cvar_key, data[ap_key])

        # Options which were split/removed in newer SoH builds. Preserve equivalent
        # local behavior using their replacement settings.
        if data["skip_child_zelda"]:
            cv("StartingZeldasLetter", 1)
            cv("ShuffleWeirdEgg", 2)  # Skip Talon path in current fork.
        if data["complete_mask_quest"]:
            cv("ShuffleMasks", 1)
            if data["complete_mask_quest"] == 1:
                for cvar_key in ("StartingKeatonMask", "StartingSkullMask", "StartingSpookyMask",
                                 "StartingBunnyHood", "StartingGoronMask", "StartingZoraMask",
                                 "StartingGerudoMask", "StartingMaskOfTruth"):
                    cv(cvar_key, 1)

        # Official Archipelago-SoH 1.4.x does not expose MQ regions. Never let an AP save
        # inherit local MQ selections, because that creates actors/checks the server cannot own.
        cv("MQDungeons", 0)
        for cvar_key in ("MQDekuTree", "MQDodongosCavern", "MQJabuJabu", "MQForestTemple",
                         "MQFireTemple", "MQWaterTemple", "MQSpiritTemple", "MQShadowTemple",
                         "MQBottomOfTheWell", "MQIceCavern", "MQGerudoTrainingGround", "MQGanonsCastle"):
            cv(cvar_key, 0)

        # SOH-EXTREME additions are part of the same authoritative snapshot.
        for key, value in {
            "ShuffleRoll": o.shuffle_roll.value, "ShuffleGrab": o.shuffle_grab.value,
            "ShuffleClimb": o.shuffle_climb.value, "ShuffleCrawl": o.shuffle_crawl.value,
            "ShuffleSpeak": o.shuffle_speak.value, "ShuffleOpenChest": o.shuffle_open_chest.value,
            "ShuffleEnemySoul": o.shuffle_enemy_soul.value, "ShuffleNpcSoul": o.shuffle_npc_soul.value,
            "ShuffleAnimalSoul": o.shuffle_animal_soul.value, "ShufflePotSoul": o.shuffle_pot_soul.value,
            "ShuffleCrateSoul": o.shuffle_crate_soul.value, "ShuffleGrassSoul": o.shuffle_grass_bush_soul.value,
            "ShuffleRockSoul": o.shuffle_rock_boulder_soul.value, "ShuffleTreeSoul": o.shuffle_tree_soul.value,
            "ShuffleBeehiveSoul": o.shuffle_beehive_soul.value, "ShuffleSignSoul": o.shuffle_sign_soul.value,
            "ShuffleSkulltulaSoul": o.shuffle_skulltula_soul.value,
            "ShuffleBusinessScrubSoul": o.shuffle_business_scrub_soul.value,
            "ShuffleShovel": o.shuffle_shovel.value, "NpcSpeechSanity": o.npc_speech_sanity.value,
            "SongNoteShuffle": o.song_note_shuffle.value, "ShuffleFlowOfTime": o.shuffle_flow_of_time.value,
            "FrozenStartingTime": o.frozen_starting_time.value, "TimeSpeedAfterUnlock": o.time_speed_after_unlock.value,
            "ExtremeTrapPool": o.extreme_trap_pool.value, "ExtremeIceTraps": o.extreme_ice_traps.value,
            "ExtremeFireTraps": o.extreme_fire_traps.value, "ExtremeSlowTraps": o.extreme_slow_traps.value,
            "ExtremeMagicSuckTraps": o.extreme_magic_suck_traps.value,
            "ExtremeHealthDrainTraps": o.extreme_health_drain_traps.value,
            "ExtremeTrapPercent": o.extreme_trap_filler_replacement.value,
        }.items():
            cv(key, value)

        # Exact AP shop prices keyed by numeric location id. The native client applies
        # these after scouting so shop actors display/charge the server-generated price.
        shop_prices_by_id = {}
        for loc, price in self.shop_prices.items():
            loc_id = self.location_name_to_id.get(str(loc))
            if isinstance(loc_id, int):
                shop_prices_by_id[str(loc_id)] = int(price)

        data.update({
            "extreme_soh_cvars": cvars,
            "extreme_shop_prices": shop_prices_by_id,
            "extreme_kakariko_gate_open": int(bool(data["kakariko_gate"])),
            "extreme_active_locations": active_locations,
            "shuffle_rocks": o.shuffle_rocks.value,
            "shuffle_boulders": o.shuffle_boulders.value,
            "shuffle_bushes": o.shuffle_bushes.value,
            "shuffle_icicles": o.shuffle_icicles.value,
            "shuffle_red_ice": o.shuffle_red_ice.value,
            "shuffle_signs": o.shuffle_signs.value,
            "shuffle_beggar": o.shuffle_beggar.value,
            "shuffle_chest_minigame": o.shuffle_chest_minigame.value,
            "shuffle_wonder_items": o.shuffle_wonder_items.value,
            "shuffle_silver": o.shuffle_silver.value,
            "shuffle_butterfly_fairies": o.shuffle_butterfly_fairies.value,
            "shuffle_roll": o.shuffle_roll.value,
            "shuffle_grab": o.shuffle_grab.value,
            "shuffle_climb": o.shuffle_climb.value,
            "shuffle_crawl": o.shuffle_crawl.value,
            "shuffle_speak": o.shuffle_speak.value,
            "shuffle_open_chest": o.shuffle_open_chest.value,
            "shuffle_enemy_soul": o.shuffle_enemy_soul.value,
            "shuffle_npc_soul": o.shuffle_npc_soul.value,
            "shuffle_animal_soul": o.shuffle_animal_soul.value,
            "shuffle_pot_soul": o.shuffle_pot_soul.value,
            "shuffle_crate_soul": o.shuffle_crate_soul.value,
            "shuffle_grass_bush_soul": o.shuffle_grass_bush_soul.value,
            "shuffle_rock_boulder_soul": o.shuffle_rock_boulder_soul.value,
            "shuffle_tree_soul": o.shuffle_tree_soul.value,
            "shuffle_beehive_soul": o.shuffle_beehive_soul.value,
            "shuffle_sign_soul": o.shuffle_sign_soul.value,
            "shuffle_skulltula_soul": o.shuffle_skulltula_soul.value,
            "shuffle_business_scrub_soul": o.shuffle_business_scrub_soul.value,
            "shuffle_shovel": o.shuffle_shovel.value,
            "npc_speech_sanity": o.npc_speech_sanity.value,
            "song_note_shuffle": o.song_note_shuffle.value,
            "shuffle_flow_of_time": o.shuffle_flow_of_time.value,
            "frozen_starting_time": o.frozen_starting_time.value,
            "time_speed_after_unlock": o.time_speed_after_unlock.value,
            "extreme_trap_pool": o.extreme_trap_pool.value,
            "extreme_ice_traps": o.extreme_ice_traps.value,
            "extreme_fire_traps": o.extreme_fire_traps.value,
            "extreme_slow_traps": o.extreme_slow_traps.value,
            "extreme_magic_suck_traps": o.extreme_magic_suck_traps.value,
            "extreme_health_drain_traps": o.extreme_health_drain_traps.value,
            "extreme_trap_filler_replacement": o.extreme_trap_filler_replacement.value,
        })
        return data
