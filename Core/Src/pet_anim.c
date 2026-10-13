/**
 ******************************************************************************
 * @file    pet_anim.c
 * @brief   Non-blocking animation sequencer. See pet_anim.h.
 *
 * All timing is derived from the millisecond timestamp the caller passes in, so
 * playback is independent of call frequency and never blocks the main loop.
 ******************************************************************************
 */
#include "pet_anim.h"

#include <stddef.h>   /* NULL */

/*
 * Frame periods are chosen against the measured sprite transfer time: a 64x64
 * sprite takes about 37 ms to push over SPI, so a 100 ms frame period leaves
 * roughly two thirds of each frame for input handling and the rest of the UI.
 */
static const pet_anim_def_t g_anims[PET_ANIM_COUNT] = {
    /* frames, frame_ms, loop */
    { 4, 250, true  },   /* IDLE     - slow breathing, loops                  */
    { 5, 120, false },   /* FEED     - berry enters and is absorbed           */
    { 5, 100, false },   /* PLAY     - excited jiggle                         */
    { 5, 120, false },   /* CLEAN    - sparkle wash                           */
    { 4, 300, true  },   /* DEATH    - collapse, then stays collapsed         */
    { 4, 300, true  },   /* RUNAWAY  - slides away and keeps drifting         */
    { 3, 180, false },   /* RESTART  - droplet re-forms into the slime        */
};

const pet_anim_def_t *pet_anim_def(pet_anim_id_t id)
{
    if ((id < 0) || (id >= PET_ANIM_COUNT)) {
        id = PET_ANIM_IDLE;
    }
    return &g_anims[id];
}

const char *pet_anim_name(pet_anim_id_t id)
{
    switch (id) {
        case PET_ANIM_IDLE:    return "IDLE";
        case PET_ANIM_FEED:    return "FEED";
        case PET_ANIM_PLAY:    return "PLAY";
        case PET_ANIM_CLEAN:   return "CLEAN";
        case PET_ANIM_DEATH:   return "DEATH";
        case PET_ANIM_RUNAWAY: return "RUNAWAY";
        case PET_ANIM_RESTART: return "RESTART";
        default:               return "?";
    }
}

void pet_anim_start(pet_anim_t *anim, pet_anim_id_t id, uint32_t now_ms)
{
    if (anim == NULL) { return; }

    anim->id             = id;
    anim->frame          = 0;
    anim->elapsed_ms     = 0;
    anim->last_update_ms = now_ms;
    anim->finished       = false;
    anim->play_count     = 0;
}

void pet_anim_update(pet_anim_t *anim, uint32_t now_ms)
{
    if (anim == NULL) { return; }

    const pet_anim_def_t *def = pet_anim_def(anim->id);

    /* Subtracting two uint32_t timestamps stays correct across the 32-bit
     * millisecond counter wrapping, exactly as pet_tick() does. */
    const uint32_t dt = now_ms - anim->last_update_ms;
    anim->last_update_ms = now_ms;

    if (anim->finished || (def->frames == 0u)) { return; }

    anim->elapsed_ms += dt;

    /* A while loop, not an if: a long gap (a paused debugger, a slow full-screen
     * redraw) must consume the right number of frames rather than lose time. */
    while (anim->elapsed_ms >= def->frame_ms) {
        anim->elapsed_ms -= def->frame_ms;

        if ((uint8_t)(anim->frame + 1u) < def->frames) {
            anim->frame++;
        } else if (def->loop) {
            anim->frame = 0;
            anim->play_count++;
        } else {
            anim->finished = true;
            anim->play_count++;
            break;
        }
    }
}

bool pet_anim_finished(const pet_anim_t *anim)
{
    return (anim == NULL) || anim->finished;
}

uint8_t pet_anim_frame(const pet_anim_t *anim)
{
    if (anim == NULL) { return 0u; }
    const pet_anim_def_t *def = pet_anim_def(anim->id);
    if (anim->frame >= def->frames) { return (uint8_t)(def->frames - 1u); }
    return anim->frame;
}

pet_anim_id_t pet_anim_for_action(pet_action_t action)
{
    switch (action) {
        case PET_ACTION_FEED:  return PET_ANIM_FEED;
        case PET_ACTION_PLAY:  return PET_ANIM_PLAY;
        case PET_ACTION_CLEAN: return PET_ANIM_CLEAN;
        default:               return PET_ANIM_IDLE;
    }
}

/** true for the animations that play once and then hand back to idle. */
static bool is_one_shot(pet_anim_id_t id)
{
    return (id == PET_ANIM_FEED) || (id == PET_ANIM_PLAY) ||
           (id == PET_ANIM_CLEAN) || (id == PET_ANIM_RESTART);
}

pet_anim_id_t pet_anim_select(const pet_t *pet, const pet_anim_t *current)
{
    if (pet == NULL) { return PET_ANIM_IDLE; }

    /* A finished life outranks any animation that happens to be in flight. */
    if (pet->state == PET_DEAD)    { return PET_ANIM_DEATH; }
    if (pet->state == PET_RUNAWAY) { return PET_ANIM_RUNAWAY; }

    if ((current != NULL) && !current->finished &&
        is_one_shot(current->id)) {
        return current->id;
    }

    /* Idle. The mood, and therefore which idle sprites and palette are used, is
     * chosen separately by pet_mood(). */
    return PET_ANIM_IDLE;
}
