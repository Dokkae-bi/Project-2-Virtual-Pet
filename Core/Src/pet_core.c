/**
 * @file    pet_core.c
 * @brief   Virtual pet model. See pet_core.h for the limits, rates and effects.
 *
 * No HAL calls, no hardware access, no blocking delays. Everything is driven by
 * the millisecond timestamp the caller passes in, so this file compiles and runs
 * the same on the STM32 and on a PC.
 */
#include "pet_core.h"

#include <stddef.h>

/* Clamp into the documented [PET_STAT_MIN, PET_STAT_MAX] range. */
static int16_t clamp_stat(int32_t value)
{
    if (value < PET_STAT_MIN) { return (int16_t)PET_STAT_MIN; }
    if (value > PET_STAT_MAX) { return (int16_t)PET_STAT_MAX; }
    return (int16_t)value;
}

/* Add a signed delta to a statistic and store the clamped result. Done in
   int32_t first so nothing overflows or relies on int16_t wrapping. */
static void apply_delta(int16_t *stat, int32_t delta)
{
    *stat = clamp_stat((int32_t)(*stat) + delta);
}

/*
 * Move one statistic on by dt milliseconds.
 *
 * accum carries leftover time between calls so partial periods aren't lost.
 * Because of that, the pet's evolution depends only on how much time has passed,
 * never on how often pet_tick() is called.
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

void pet_init(pet_t *pet, uint32_t now_ms)
{
    if (pet == NULL) { return; }

    /* Clear the whole struct first so no field is left undefined. */
    *pet = (pet_t){0};

    pet->food         = PET_INIT_FOOD;
    pet->happiness    = PET_INIT_HAPPINESS;
    pet->state        = PET_ALIVE;
    pet->last_action  = PET_ACTION_NONE;
    pet->last_tick_ms = now_ms;

    /* age_ms, the accumulators and the action counters all start at zero. */
}

void pet_restart(pet_t *pet, uint32_t now_ms)
{
    pet_init(pet, now_ms);
}

bool pet_is_interactive(const pet_t *pet)
{
    return (pet != NULL) && (pet->state == PET_ALIVE);
}

uint32_t pet_age_day(const pet_t *pet)
{
    if (pet == NULL) { return 1u; }
    return pet->age_ms / (PET_MS_PER_GAME_HOUR * PET_HOURS_PER_DAY) + 1u;
}

uint8_t pet_age_hour(const pet_t *pet)
{
    if (pet == NULL) { return 0u; }
    return (uint8_t)((pet->age_ms / PET_MS_PER_GAME_HOUR) % PET_HOURS_PER_DAY);
}

void pet_tick(pet_t *pet, uint32_t now_ms)
{
    if (pet == NULL) { return; }

    /* Subtracting two uint32_t timestamps still works when the millisecond
       counter wraps (about every 49.7 days), which is why elapsed time is
       worked out this way rather than comparing the values directly. */
    const uint32_t dt = now_ms - pet->last_tick_ms;
    pet->last_tick_ms = now_ms;

    /* A dead pet, or one that has run away, is frozen until pet_restart(). */
    if (!pet_is_interactive(pet)) { return; }

    pet->age_ms += dt;

    advance_by_time(&pet->food,      &pet->food_accum_ms,      dt,
                    PET_FOOD_MS_PER_UNIT,      -1);
    advance_by_time(&pet->happiness, &pet->happiness_accum_ms, dt,
                    PET_HAPPINESS_MS_PER_UNIT, -1);

    /* Check the terminal conditions after updating the statistics. Food is
       tested first, so a pet that hits both limits on the same tick counts as
       having starved. */
    if (pet->food <= PET_STAT_MIN) {
        pet->state = PET_DEAD;
    } else if (pet->happiness <= PET_STAT_MIN) {
        pet->state = PET_RUNAWAY;
    }
}

/* Bookkeeping shared by every accepted action. Returns false, having changed
   nothing, if the pet can't be interacted with. */
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

    /* Clamp the ADC portion rather than rejecting it, so a noisy or out of range
       reading just gives a smaller or bigger meal. */
    if (portion < PET_FEED_PORTION_MIN) { portion = PET_FEED_PORTION_MIN; }
    if (portion > PET_FEED_PORTION_MAX) { portion = PET_FEED_PORTION_MAX; }

    apply_delta(&pet->food,      +(int32_t)portion * PET_FEED_FOOD_PER_PORTION);
    apply_delta(&pet->happiness, +PET_FEED_HAPPINESS_BONUS);
    return true;
}

bool pet_play(pet_t *pet, uint32_t now_ms)
{
    if (!begin_action(pet, PET_ACTION_PLAY, now_ms)) { return false; }

    apply_delta(&pet->happiness, +PET_PLAY_HAPPINESS_GAIN);
    apply_delta(&pet->food,      -PET_PLAY_FOOD_COST);
    return true;
}

bool pet_clean(pet_t *pet, uint32_t now_ms)
{
    if (!begin_action(pet, PET_ACTION_CLEAN, now_ms)) { return false; }

    apply_delta(&pet->happiness, +PET_CLEAN_HAPPINESS_GAIN);
    return true;
}

pet_mood_t pet_mood(const pet_t *pet)
{
    if (pet == NULL) { return PET_MOOD_MISERABLE; }

    /* A pet that isn't alive is shown in its worst mood, which is what drives
       the death and runaway animations. */
    if (!pet_is_interactive(pet)) { return PET_MOOD_MISERABLE; }

    if (pet->happiness <= PET_MOOD_MISERABLE_AT_HAPPINESS) { return PET_MOOD_MISERABLE; }
    if (pet->food      <= PET_MOOD_HUNGRY_AT_FOOD)          { return PET_MOOD_HUNGRY;    }
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
