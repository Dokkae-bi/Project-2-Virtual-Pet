/**
 ******************************************************************************
 * @file    pet_core.c
 * @brief   Platform-independent virtual pet model. See pet_core.h for the
 *          documented limits, rates and action effects.
 *
 * No HAL calls, no hardware access, no blocking delays. All evolution is driven
 * by the millisecond timestamp the caller passes in, so this file compiles and
 * runs unchanged on both the STM32 and a host PC.
 ******************************************************************************
 */
#include "pet_core.h"

#include <stddef.h>   /* NULL */

/* ---------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/** Clamp a value into the documented [PET_STAT_MIN, PET_STAT_MAX] range. */
static int16_t clamp_stat(int32_t value)
{
    if (value < PET_STAT_MIN) { return (int16_t)PET_STAT_MIN; }
    if (value > PET_STAT_MAX) { return (int16_t)PET_STAT_MAX; }
    return (int16_t)value;
}

/**
 * @brief Apply a signed delta to a statistic, storing the clamped result.
 *
 * The arithmetic is done in int32_t first so there is no intermediate overflow
 * and no reliance on int16_t wrap-around.
 */
static void apply_delta(int16_t *stat, int32_t delta)
{
    *stat = clamp_stat((int32_t)(*stat) + delta);
}

/**
 * @brief Advance one statistic by dt milliseconds of elapsed time.
 *
 * @param accum           carries leftover time between calls so that partial
 *                        periods are never lost
 * @param units_per_period signed change applied each time a period completes
 *
 * Tracking elapsed time this way means the pet's evolution depends only on how
 * much wall-clock time has passed, never on how often pet_tick() is called.
 */
static void advance_by_time(int16_t *stat, uint32_t *accum, uint32_t dt,
                            uint32_t ms_per_unit, int32_t units_per_period)
{
    *accum += dt;

    while (*accum >= ms_per_unit) {
        *accum -= ms_per_unit;
        apply_delta(stat, units_per_period);
    }
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------- */

void pet_init(pet_t *pet, uint32_t now_ms)
{
    if (pet == NULL) { return; }

    /* Zero the whole structure first so that no field is left undefined. */
    *pet = (pet_t){0};

    pet->hunger       = PET_INIT_HUNGER;
    pet->happiness    = PET_INIT_HAPPINESS;
    pet->state        = PET_ALIVE;
    pet->last_action  = PET_ACTION_NONE;
    pet->last_tick_ms = now_ms;

    /* age_ms, both accumulators and every action counter start at zero. */
}

void pet_restart(pet_t *pet, uint32_t now_ms)
{
    pet_init(pet, now_ms);
}

bool pet_is_interactive(const pet_t *pet)
{
    return (pet != NULL) && (pet->state == PET_ALIVE);
}

/* ---------------------------------------------------------------------------
 * Time evolution
 * ------------------------------------------------------------------------- */

void pet_tick(pet_t *pet, uint32_t now_ms)
{
    if (pet == NULL) { return; }

    /* Subtracting two uint32_t timestamps stays correct when the 32-bit
     * millisecond counter wraps (roughly every 49.7 days), which is why the
     * elapsed time is computed this way rather than comparing absolute values. */
    const uint32_t dt = now_ms - pet->last_tick_ms;
    pet->last_tick_ms = now_ms;

    /* A dead pet, or one that has run away, is frozen: its statistics stop
     * changing until pet_restart() is called. */
    if (!pet_is_interactive(pet)) { return; }

    pet->age_ms += dt;

    advance_by_time(&pet->hunger,    &pet->hunger_accum_ms,    dt,
                    PET_HUNGER_MS_PER_UNIT,    +1);
    advance_by_time(&pet->happiness, &pet->happiness_accum_ms, dt,
                    PET_HAPPINESS_MS_PER_UNIT, -1);

    /* Terminal conditions are evaluated after the statistics have been updated.
     * Hunger is tested first, so a pet that reaches both limits on the same tick
     * is reported as having starved. */
    if (pet->hunger >= PET_STAT_MAX) {
        pet->state = PET_DEAD;
    } else if (pet->happiness <= PET_STAT_MIN) {
        pet->state = PET_RUNAWAY;
    }
}

/* ---------------------------------------------------------------------------
 * Interactions
 * ------------------------------------------------------------------------- */

/**
 * @brief Shared bookkeeping for every accepted action.
 *
 * Returns false, having changed nothing, when the pet cannot be interacted
 * with. That refusal is what makes the restart procedure necessary.
 */
static bool begin_action(pet_t *pet, pet_action_t action, uint32_t now_ms)
{
    if (!pet_is_interactive(pet)) { return false; }

    pet->last_action    = action;
    pet->last_action_ms = now_ms;

    if ((action > PET_ACTION_NONE) && (action < PET_ACTION_COUNT)) {
        pet->action_count[action]++;
    }
    return true;
}

bool pet_feed(pet_t *pet, uint8_t portion, uint32_t now_ms)
{
    if (!begin_action(pet, PET_ACTION_FEED, now_ms)) { return false; }

    /* The ADC-derived portion is clamped into the documented range rather than
     * rejected, so a noisy or out-of-range reading degrades gracefully. */
    if (portion < PET_FEED_PORTION_MIN) { portion = PET_FEED_PORTION_MIN; }
    if (portion > PET_FEED_PORTION_MAX) { portion = PET_FEED_PORTION_MAX; }

    apply_delta(&pet->hunger,    -(int32_t)portion * PET_FEED_HUNGER_PER_PORTION);
    apply_delta(&pet->happiness, +PET_FEED_HAPPINESS_BONUS);
    return true;
}

bool pet_play(pet_t *pet, uint32_t now_ms)
{
    if (!begin_action(pet, PET_ACTION_PLAY, now_ms)) { return false; }

    apply_delta(&pet->happiness, +PET_PLAY_HAPPINESS_GAIN);
    apply_delta(&pet->hunger,    +PET_PLAY_HUNGER_COST);
    return true;
}

bool pet_clean(pet_t *pet, uint32_t now_ms)
{
    if (!begin_action(pet, PET_ACTION_CLEAN, now_ms)) { return false; }

    apply_delta(&pet->happiness, +PET_CLEAN_HAPPINESS_GAIN);
    return true;
}

/* ---------------------------------------------------------------------------
 * Presentation helpers
 * ------------------------------------------------------------------------- */

pet_mood_t pet_mood(const pet_t *pet)
{
    if (pet == NULL) { return PET_MOOD_MISERABLE; }

    /* A pet that is no longer alive is shown in its worst mood, which is what
     * drives the death and runaway animations. */
    if (!pet_is_interactive(pet)) { return PET_MOOD_MISERABLE; }

    if (pet->happiness <= PET_MOOD_MISERABLE_AT_HAPPINESS) { return PET_MOOD_MISERABLE; }
    if (pet->hunger    >= PET_MOOD_HUNGRY_AT_HUNGER)       { return PET_MOOD_HUNGRY;    }
    if (pet->happiness >= PET_MOOD_DELIGHTED_AT_HAPPINESS) { return PET_MOOD_DELIGHTED; }
    return PET_MOOD_CONTENT;
}

const char *pet_state_name(pet_state_t state)
{
    switch (state) {
        case PET_ALIVE:   return "ALIVE";
        case PET_DEAD:    return "DEAD";
        case PET_RUNAWAY: return "RUNAWAY";
        default:          return "?";
    }
}

const char *pet_mood_name(pet_mood_t mood)
{
    switch (mood) {
        case PET_MOOD_DELIGHTED: return "DELIGHTED";
        case PET_MOOD_CONTENT:   return "CONTENT";
        case PET_MOOD_HUNGRY:    return "HUNGRY";
        case PET_MOOD_MISERABLE: return "MISERABLE";
        default:                 return "?";
    }
}

const char *pet_action_name(pet_action_t action)
{
    switch (action) {
        case PET_ACTION_NONE:  return "NONE";
        case PET_ACTION_FEED:  return "FEED";
        case PET_ACTION_PLAY:  return "PLAY";
        case PET_ACTION_CLEAN: return "CLEAN";
        default:               return "?";
    }
}
