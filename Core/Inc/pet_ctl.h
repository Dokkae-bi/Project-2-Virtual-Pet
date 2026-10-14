/**
 * @file    pet_ctl.h
 * @brief   Applies actions to the pet and keeps its animation in step.
 *
 * One place that turns "the player chose Feed" into the model change plus the
 * matching animation. The menu calls it, and so can any peripheral that triggers
 * an action directly - so the pet behaves the same whichever input caused it.
 *
 * Wraps pet_core and pet_anim together into one object so the main loop has a
 * single thing to initialise and tick. No hardware dependency.
 */
#ifndef PET_CTL_H
#define PET_CTL_H

#include <stdint.h>
#include <stdbool.h>

#include "pet_core.h"
#include "pet_anim.h"

typedef struct {
    pet_t      pet;
    pet_anim_t anim;
} pet_ctl_t;

/* Start a fresh pet, idle. */
void pet_ctl_init(pet_ctl_t *ctl, uint32_t now_ms);

/*
 * Advance the pet and the animation. Call it from the main loop as often as
 * practical - both are elapsed-time based, so the result doesn't depend on how
 * often this runs.
 *
 * Also puts the animation back to idle once a one-shot action has finished, so
 * the next action starts from a clean state.
 */
void pet_ctl_tick(pet_ctl_t *ctl, uint32_t now_ms);

/*
 * Apply one of the three interactions.
 *
 * portion only matters for FEED, where it comes from the analog input; it is
 * clamped by pet_feed() so a noisy or out of range reading is harmless.
 *
 * Returns true if the pet accepted it. A refused action (the pet is dead or has
 * run away) does not start an animation either - it must not look like
 * something happened.
 */
bool pet_ctl_action(pet_ctl_t *ctl, pet_action_t action, uint8_t portion,
                    uint32_t now_ms);

/* Wake without a care action (menu Wake). Returns the pet to idle. */
void pet_ctl_wake(pet_ctl_t *ctl, uint32_t now_ms);

/* Bring a finished pet back, and play the re-forming animation. */
void pet_ctl_restart(pet_ctl_t *ctl, uint32_t now_ms);

#endif /* PET_CTL_H */
