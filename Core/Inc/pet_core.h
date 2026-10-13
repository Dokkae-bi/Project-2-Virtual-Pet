/**
 * @file    pet_core.h
 * @brief   Virtual pet model (ECGR 4101/5101 Project 2).
 *
 * Owns the pet's state and all the rules for it. No hardware dependency at all:
 * no HAL, no GPIO, no display, no blocking delay. The caller passes in the
 * current time in milliseconds, so the model is deterministic and can be tested
 * on a PC - see tests/test_pet_core.c.
 *
 * The main loop just asks this module what is true about the pet right now.
 */
#ifndef PET_CORE_H
#define PET_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* Both statistics are clamped to this closed range, so neither neglect nor
   over-feeding can push them outside it. */
#define PET_STAT_MIN                    (0)     /* happiness floor / hunger floor */
#define PET_STAT_MAX                    (100)   /* happiness ceiling / hunger cap */

/* Evolution rates: one unit of change per this many milliseconds. These are the
   main tuning knobs for the game - lower values reach the crisis states sooner,
   which helps when showing death and runaway during a short presentation.

   With these values a completely neglected pet starves in 90 s (25 -> 100 hunger
   at 1200 ms/unit) while its happiness is still falling, so neglect ends in
   death. Feeding to keep hunger down makes happiness the limiting factor
   instead, which is how the runaway ending gets reached. */
#define PET_HUNGER_MS_PER_UNIT          1200u   /* +1 hunger    every 1.2 s */
#define PET_HAPPINESS_MS_PER_UNIT       2000u   /* -1 happiness every 2.0 s */

/* Action effects. */
#define PET_FEED_PORTION_MIN            1u      /* smallest portion the ADC can pick */
#define PET_FEED_PORTION_MAX            5u      /* largest portion */
#define PET_FEED_HUNGER_PER_PORTION     12      /* hunger removed per portion unit */
#define PET_FEED_HAPPINESS_BONUS        4       /* a good meal is a bit cheering */
#define PET_PLAY_HAPPINESS_GAIN         22
#define PET_PLAY_HUNGER_COST            9       /* playing works up an appetite */
#define PET_CLEAN_HAPPINESS_GAIN        14

/* Starting figures, on power-up and after a restart. */
#define PET_INIT_HUNGER                 25      /* slightly peckish at boot */
#define PET_INIT_HAPPINESS              75

/* Mood thresholds, used to pick the idle sprite. */
#define PET_MOOD_MISERABLE_AT_HAPPINESS 25
#define PET_MOOD_HUNGRY_AT_HUNGER       70
#define PET_MOOD_DELIGHTED_AT_HAPPINESS 75

/* Life states. ALIVE is the only one where the pet accepts input. */
typedef enum {
    PET_ALIVE = 0,
    PET_DEAD,       /* hunger reached PET_STAT_MAX */
    PET_RUNAWAY     /* happiness reached PET_STAT_MIN */
} pet_state_t;

/* The three required interactions. */
typedef enum {
    PET_ACTION_NONE = 0,
    PET_ACTION_FEED,
    PET_ACTION_PLAY,
    PET_ACTION_CLEAN,
    PET_ACTION_COUNT                /* keep last */
} pet_action_t;

/* Coarse mood, used to pick the idle sprite. */
typedef enum {
    PET_MOOD_DELIGHTED = 0,
    PET_MOOD_CONTENT,
    PET_MOOD_HUNGRY,
    PET_MOOD_MISERABLE
} pet_mood_t;

/* Full behavioural state. Small and copyable, so in firmware it lives in one
   static instance with no dynamic allocation. */
typedef struct {
    int16_t      hunger;            /* 0 = fully fed,  PET_STAT_MAX = starving */
    int16_t      happiness;         /* 0 = runs away,  PET_STAT_MAX = delighted */

    pet_state_t  state;

    uint32_t     age_ms;            /* total simulated lifetime */
    uint32_t     last_tick_ms;      /* timestamp of the last pet_tick() */
    uint32_t     hunger_accum_ms;   /* leftover time between calls */
    uint32_t     happiness_accum_ms;

    pet_action_t last_action;       /* most recent accepted action */
    uint32_t     last_action_ms;
    uint16_t     action_count[PET_ACTION_COUNT];
} pet_t;

/* Functions that change the pet take now_ms and return true when the request was
   accepted. They refuse while the pet is dead or has run away, which is what
   makes the restart procedure necessary. */

void pet_init(pet_t *pet, uint32_t now_ms);

/* Advance the simulation to now_ms. Call it from the main loop as often as
   practical - everything is driven by elapsed time, so the outcome doesn't
   depend on how often this runs. */
void pet_tick(pet_t *pet, uint32_t now_ms);

/* Feed the pet. portion is clamped to [PET_FEED_PORTION_MIN, MAX]. */
bool pet_feed(pet_t *pet, uint8_t portion, uint32_t now_ms);

/* Play with the pet: happiness up, and it gets hungrier. */
bool pet_play(pet_t *pet, uint32_t now_ms);

/* Clean the pet: happiness up. */
bool pet_clean(pet_t *pet, uint32_t now_ms);

/* Restart after death or runaway, back to the starting figures. */
void pet_restart(pet_t *pet, uint32_t now_ms);

/* false once the pet is dead or has run away; the UI uses this to lock input. */
bool pet_is_interactive(const pet_t *pet);

/* Current mood, for choosing the idle sprite. */
pet_mood_t pet_mood(const pet_t *pet);

/* Names, for host tests and for on-screen debug text. */
const char *pet_state_name(pet_state_t state);
const char *pet_mood_name(pet_mood_t mood);
const char *pet_action_name(pet_action_t action);

#endif /* PET_CORE_H */
