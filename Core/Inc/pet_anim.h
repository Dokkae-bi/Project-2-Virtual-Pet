/**
 ******************************************************************************
 * @file    pet_anim.h
 * @brief   Non-blocking animation sequencer and state-to-animation mapping.
 *
 * Like pet_core, this module has NO hardware dependency. The caller supplies the
 * current time, so frame timing is driven by elapsed time and the result does
 * not depend on how often it is called. That is what keeps the interface
 * responsive while an animation plays: nothing here ever blocks.
 ******************************************************************************
 */
#ifndef PET_ANIM_H
#define PET_ANIM_H

#include <stdint.h>
#include <stdbool.h>

#include "pet_core.h"

/** Which animation is playing. */
typedef enum {
    PET_ANIM_IDLE = 0,      /* loops forever; the mood picks which sprites */
    PET_ANIM_FEED,          /* one-shot, started by the FEED action        */
    PET_ANIM_PLAY,          /* one-shot, started by the PLAY action        */
    PET_ANIM_CLEAN,         /* one-shot, started by the CLEAN action       */
    PET_ANIM_DEATH,         /* loops; the pet has starved                  */
    PET_ANIM_RUNAWAY,       /* loops; the pet has left                    */
    PET_ANIM_RESTART,       /* one-shot, plays after a restart             */
    PET_ANIM_COUNT          /* sentinel - keep last                       */
} pet_anim_id_t;

/** Fixed description of one animation. */
typedef struct {
    uint8_t  frames;        /* number of frames                       */
    uint16_t frame_ms;      /* milliseconds each frame is held        */
    bool     loop;          /* true = restart at the end, false = hold */
} pet_anim_def_t;

/** Instantaneous playback state. */
typedef struct {
    pet_anim_id_t id;
    uint8_t       frame;         /* current frame index              */
    uint32_t      elapsed_ms;    /* time spent inside the current frame */
    uint32_t      last_update_ms;
    bool          finished;      /* set once a one-shot animation ends  */
    uint32_t      play_count;    /* completed passes, for tests/debug    */
} pet_anim_t;

/* ---------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/** Frame counts and periods for every animation. Never returns NULL. */
const pet_anim_def_t *pet_anim_def(pet_anim_id_t id);

/** Human-readable name, for host tests and debug output. */
const char *pet_anim_name(pet_anim_id_t id);

/** Begin an animation, restarting it if it is already the current one. */
void pet_anim_start(pet_anim_t *anim, pet_anim_id_t id, uint32_t now_ms);

/**
 * Advance playback to now_ms. Call from the main loop; safe to call at any rate.
 */
void pet_anim_update(pet_anim_t *anim, uint32_t now_ms);

/** true once a one-shot animation has reached its last frame. */
bool pet_anim_finished(const pet_anim_t *anim);

/** Current frame index, always within the animation's frame count. */
uint8_t pet_anim_frame(const pet_anim_t *anim);

/** The animation that belongs to a completed action. */
pet_anim_id_t pet_anim_for_action(pet_action_t action);

/**
 * @brief Decide which animation should be on screen right now.
 *
 * Priority, highest first:
 *   1. death, then runaway - a finished life overrides everything
 *   2. an action animation that is still playing
 *   3. idle, whose sprite is then chosen by pet_mood()
 *
 * @param current the animation in flight, or NULL if none.
 */
pet_anim_id_t pet_anim_select(const pet_t *pet, const pet_anim_t *current);

#endif /* PET_ANIM_H */
