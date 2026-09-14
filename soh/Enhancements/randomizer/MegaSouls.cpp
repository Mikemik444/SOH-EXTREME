#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Wood02/z_en_wood02.h"
extern PlayState* gPlayState;
}

// Mega Randomizer category-soul runtime enforcement.
// For the barren-world rules, actors controlled by a soul are prevented from
// initializing at all. After obtaining a soul, leave/re-enter the room to make
// those actors spawn normally.
static bool MegaHas(RandomizerInf inf) {
    return Flags_GetRandomizerInf(inf);
}

extern "C" bool MegaSoul_CanUseCrate(void) {
    return !IS_RANDO || !RAND_GET_OPTION(RSK_SHUFFLE_CRATE_SOUL) || MegaHas(RAND_INF_CRATE_SOUL);
}

static bool IsMegaAnimalActor(s16 id) {
    switch (id) {
        case ACTOR_EN_COW:
        case ACTOR_EN_NIW:
        case ACTOR_EN_DOG:
        case ACTOR_EN_FISH:
        case ACTOR_EN_INSECT:
        case ACTOR_EN_BUTTE:
        case ACTOR_EN_FR:
        case ACTOR_EN_HORSE:
        case ACTOR_EN_HORSE_NORMAL:
            return true;
        default:
            return false;
    }
}

static bool IsMegaPotActor(s16 id) {
    switch (id) {
        case ACTOR_OBJ_TSUBO:
        case ACTOR_EN_TUBO_TRAP:
        case ACTOR_EN_WALL_TUBO:
            return true;
        default:
            return false;
    }
}

static bool IsMegaCrateActor(s16 id) {
    return id == ACTOR_OBJ_KIBAKO || id == ACTOR_OBJ_KIBAKO2;
}

static bool IsMegaRockActor(s16 id) {
    return id == ACTOR_EN_ISHI || id == ACTOR_OBJ_BOMBIWA || id == ACTOR_OBJ_HAMISHI;
}

static bool IsMegaGrassActor(s16 id) {
    return id == ACTOR_EN_KUSA;
}

static bool IsMegaScrubActor(s16 id) {
    // ACTOR_EN_DNS is the business/sanity Deku Scrub used by the randomized
    // scrub checks. ACTOR_EN_SHOPNUTS is its above-ground sales form.
    return id == ACTOR_EN_DNS || id == ACTOR_EN_SHOPNUTS;
}

static bool IsMegaBushActor(const Actor* actor) {
    if (actor == nullptr || actor->id != ACTOR_EN_WOOD02) {
        return false;
    }
    const s16 type = actor->params & 0xFF;
    return type >= WOOD_BUSH_GREEN_SMALL && type <= WOOD_BUSH_BLACK_LARGE_SPAWNED;
}

static bool IsMegaTreeActor(const Actor* actor) {
    if (actor == nullptr || actor->id != ACTOR_EN_WOOD02) {
        return false;
    }
    const s16 type = actor->params & 0xFF;
    return type >= WOOD_TREE_CONICAL_LARGE && type <= WOOD_TREE_KAKARIKO_ADULT;
}

static bool IsMegaBeehiveActor(s16 id) {
    return id == ACTOR_OBJ_COMB;
}

static bool IsMegaSignActor(s16 id) {
    return id == ACTOR_EN_KANBAN;
}

static bool IsMegaHiddenGrottoActor(const Actor* actor) {
    // Door_Ana params 0x100/0x200 mark grottos that begin hidden and must
    // normally be revealed by Song of Storms or bombs/hammer. Open holes have neither bit.
    return actor != nullptr && actor->id == ACTOR_DOOR_ANA && (actor->params & 0x0300) != 0;
}

static void RegisterMegaSouls() {
    bool shouldRegister = IS_RANDO;

    COND_VB_SHOULD(VB_PLAYER_CAN_ROLL, shouldRegister, {
        if (RAND_GET_OPTION(RSK_SHUFFLE_ROLL) && !MegaHas(RAND_INF_HAS_ROLL)) {
            *should = false;
        }
    });

    // Runtime fallback for talk requests. Most sign/NPC actors are hidden by
    // ShouldActorInit below, but this also protects actors created dynamically.
    COND_VB_SHOULD(VB_SPEAK, shouldRegister, {
        Player* player = GET_PLAYER(gPlayState);
        Actor* talkActor = player != nullptr ? player->talkActor : nullptr;
        if (talkActor == nullptr) {
            return;
        }
        if (RAND_GET_OPTION(RSK_SHUFFLE_SIGN_SOUL) && !MegaHas(RAND_INF_SIGN_SOUL) &&
            IsMegaSignActor(talkActor->id)) {
            *should = false;
            return;
        }
        if (RAND_GET_OPTION(RSK_SHUFFLE_NPC_SOUL) && !MegaHas(RAND_INF_NPC_SOUL) &&
            talkActor->category == ACTORCAT_NPC && !IsMegaAnimalActor(talkActor->id)) {
            *should = false;
        }
    });

    COND_VB_SHOULD(VB_TREE_DROP_ITEM, shouldRegister, {
        if (RAND_GET_OPTION(RSK_SHUFFLE_TREE_SOUL) && !MegaHas(RAND_INF_TREE_SOUL)) {
            *should = false;
        }
    });

    // Bushes share En_Wood02 with trees. They must still initialize so their
    // collider exists, but without Grass Soul they must not yield their check.
    COND_VB_SHOULD(VB_BUSH_DROP_ITEM, shouldRegister, {
        if (RAND_GET_OPTION(RSK_SHUFFLE_GRASS_SOUL) && !MegaHas(RAND_INF_GRASS_SOUL)) {
            *should = false;
        }
    });

    // Barren-world actor gating. Actors are absent rather than frozen/visible.
    COND_HOOK(ShouldActorInit, shouldRegister, [](void* actorRef, bool* result) {
        Actor* actor = static_cast<Actor*>(actorRef);
        if (actor == nullptr) {
            return;
        }

        // Deku Scrubs are intentionally NOT part of generic Enemy Soul.
        // Their existence is controlled exclusively by Scrub Soul below.
        if (RAND_GET_OPTION(RSK_SHUFFLE_ENEMY_SOUL) && !MegaHas(RAND_INF_ENEMY_SOUL) &&
            actor->category == ACTORCAT_ENEMY && !IsMegaScrubActor(actor->id)) {
            *result = false;
            return;
        }

        // Scrubs use existential Soul behavior: without Scrub Soul they do not
        // exist in the scene at all. Unlike crates and rocks/boulders there is
        // no black-tainted locked model for scrubs.
        if (RAND_GET_OPTION(RSK_SHUFFLE_BUSINESS_SCRUB_SOUL) &&
            !MegaHas(RAND_INF_BUSINESS_SCRUB_SOUL) && IsMegaScrubActor(actor->id)) {
            *result = false;
            return;
        }

        // Obj_Mure owns arrays of spawned fish/bug/butterfly/grass children.  Rejecting an
        // individual child from ShouldActorInit leaves Obj_Mure holding a pointer to an actor
        // whose initialization was cancelled; its group behaviour later dereferences that
        // stale pointer (the 0.7.8 Zora's River crash).  Gate the group parent instead.
        if (actor->id == ACTOR_OBJ_MURE) {
            const s16 mureType = actor->params & 0x1F;
            if (RAND_GET_OPTION(RSK_SHUFFLE_ANIMAL_SOUL) && !MegaHas(RAND_INF_ANIMAL_SOUL) &&
                (mureType == 2 || mureType == 3 || mureType == 4)) {
                *result = false;
                return;
            }
            if (RAND_GET_OPTION(RSK_SHUFFLE_GRASS_SOUL) && !MegaHas(RAND_INF_GRASS_SOUL) && mureType == 0) {
                *result = false;
                return;
            }
        }

        if (RAND_GET_OPTION(RSK_SHUFFLE_ANIMAL_SOUL) && !MegaHas(RAND_INF_ANIMAL_SOUL) &&
            IsMegaAnimalActor(actor->id)) {
            *result = false;
            return;
        }

        if (RAND_GET_OPTION(RSK_SHUFFLE_POT_SOUL) && !MegaHas(RAND_INF_POT_SOUL) && IsMegaPotActor(actor->id)) {
            *result = false;
            return;
        }

        // Crates intentionally remain in the scene without Crate Soul, matching
        // Rock/Boulder Soul semantics. Obj_Kibako/Obj_Kibako2 already call
        // MegaSoul_CanUseCrate(), so they stay solid but cannot be lifted or
        // broken until the soul is owned. ShuffleCrates.cpp dark-taints them.
        // Soul semantics are existential: without Grass/Bush Soul, both grass
        // and bush actors are absent from the scene.
        if (RAND_GET_OPTION(RSK_SHUFFLE_GRASS_SOUL) && !MegaHas(RAND_INF_GRASS_SOUL) &&
            (IsMegaGrassActor(actor->id) || IsMegaBushActor(actor))) {
            *result = false;
            return;
        }

        // Rock/Boulder Soul is different from the existential actor Souls:
        // rocks must remain in the scene so they continue to cover/block checks
        // and grotto holes. Their actor update functions reject lifting/breaking
        // until the Soul is obtained.

        if (RAND_GET_OPTION(RSK_SHUFFLE_TREE_SOUL) && !MegaHas(RAND_INF_TREE_SOUL) && IsMegaTreeActor(actor)) {
            *result = false;
            return;
        }

        if (RAND_GET_OPTION(RSK_SHUFFLE_BEEHIVE_SOUL) && !MegaHas(RAND_INF_BEEHIVE_SOUL) &&
            IsMegaBeehiveActor(actor->id)) {
            *result = false;
            return;
        }

        if (RAND_GET_OPTION(RSK_SHUFFLE_SKULLTULA_SOUL) && !MegaHas(RAND_INF_SKULLTULA_SOUL) &&
            (actor->id == ACTOR_EN_SW || actor->id == ACTOR_EN_SI)) {
            *result = false;
            return;
        }

        if (RAND_GET_OPTION(RSK_SHUFFLE_SIGN_SOUL) && !MegaHas(RAND_INF_SIGN_SOUL) && IsMegaSignActor(actor->id)) {
            *result = false;
            return;
        }
    });

    // NPCs are scene-sensitive, so do not cancel their initialization. Hiding them after init
    // keeps scripts and room bookkeeping alive while the NPC Soul talk/logic gates prevent use.
    // Re-enter the area after obtaining NPC Soul to restore normal rendering.
    COND_HOOK(OnActorInit, shouldRegister, [](void* actorRef) {
        Actor* actor = static_cast<Actor*>(actorRef);
        if (actor == nullptr) {
            return;
        }
        if (RAND_GET_OPTION(RSK_SHUFFLE_NPC_SOUL) && !MegaHas(RAND_INF_NPC_SOUL) &&
            actor->category == ACTORCAT_NPC && !IsMegaAnimalActor(actor->id) && actor->id != ACTOR_EN_SW) {
            actor->draw = nullptr;
            actor->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        }
    });
}

static RegisterShipInitFunc registerMegaSouls(RegisterMegaSouls, { "IS_RANDO" });
