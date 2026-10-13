/**
 * @file    pet_anim.h
 * @brief   Animation sequencer and state-to-animation mapping.
 *
 * No hardware dependency, same as pet_core. The caller passes in the current
 * time, so frame timing depends only on how much time has elapsed and not on how
 * often these functions are called. Nothing here ever blocks, which is what lets
 * the interface stay responsive while an animation plays.
 */
#ifndef PET_ANIM_H
#define PET_ANIM_H

#include <stdint.h>
#include <stdbool.h>

#include "pet_core.h"

/* Which animation is playing. */
typedef enum {
    PET_ANIM_IDLE = 0,      /* loops; the mood picks which sprite */
    PET_ANIM_FEED,          /* one-shot, started by the FEED action */
    PET_ANIM_PLAY,          /* one-shot, started by the PLAY action */
    PET_ANIM_CLEAN,         /* one-shot, started by the CLEAN action */
    PET_ANIM_DEATH,         /* loops; the pet starved */
    PET_ANIM_RUNAWAY,       /* loops; the pet left */
    PET_ANIM_RESTART,       /* one-shot, after a restart */
    PET_ANIM_COUNT          /* keep last */
} pet_anim_id_t;

/* Fixed description of one animation. */
typedef struct {
    uint8_t  frames;        /* number of frames */
    uint16_t frame_ms;      /* how long each frame is held */
    bool     loop;          /* true = start over at the end, false = hold */
} pet_anim_def_t;

/* Current playback state. */
typedef struct {
    pet_anim_id_t id;
    uint8_t       frame;
    uint32_t      elapsed_ms;    /* time spent in the current frame */
    uint32_t      last_update_ms;
    bool          finished;      /* set once a one-shot animation ends */
    uint32_t      play_count;    /* completed passes, for tests and debug */
} pet_anim_t;

/* Frame count and period for every animation. Never returns NULL. */
const pet_anim_def_t *pet_anim_def(pet_anim_id_t id);

/* Name, for tests and debug output. */
const char *pet_anim_name(pet_anim_id_t id);

/* Start an animation, restarting it if it's already the current one. */
void pet_anim_start(pet_anim_t *anim, pet_anim_id_t id, uint32_t now_ms);

/* Advance playback to now_ms. Call from the main loop; safe at any rate. */
void pet_anim_update(pet_anim_t *anim, uint32_t now_ms);

/* true once a one-shot animation has reached its last frame. */
bool pet_anim_finished(const pet_anim_t *anim);

/* Current frame index, always within the animation's frame count. */
uint8_t pet_anim_frame(const pet_anim_t *anim);

/* The animation that goes with a completed action. */
pet_anim_id_t pet_anim_for_action(pet_action_t action);

/*
 * Which animation should be on screen right now. Priority, highest first:
 *
 *   1. death, then runaway - a finished life beats everything else
 *   2. an action animation that is still playing
 *   3. idle, with the sprite chosen by pet_mood()
 *
 * current is the animation in flight, or NULL if there isn't one.
 */
pet_anim_id_t pet_anim_select(const pet_t *pet, const pet_anim_t *current);

#endif /* PET_ANIM_H */
