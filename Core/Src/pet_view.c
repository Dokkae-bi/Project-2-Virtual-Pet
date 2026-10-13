/**
 ******************************************************************************
 * @file    pet_view.c
 * @brief   Sprite selection and frame composition. See pet_view.h.
 ******************************************************************************
 */
#include "pet_view.h"

#include <stddef.h>   /* NULL */

/* Generated RGB565 assets (see tools/png_to_rgb565.py). */
#include "slime_content.h"
#include "slime_delighted.h"
#include "slime_hungry.h"
#include "slime_miserable.h"
#include "slime_eating.h"
#include "slime_dead.h"
#include "slime_runaway.h"

/* ---------------------------------------------------------------------------
 * Artwork selection
 * ------------------------------------------------------------------------- */

const uint16_t *pet_view_sprite_for_mood(pet_mood_t mood)
{
    switch (mood) {
        case PET_MOOD_DELIGHTED: return slime_delighted;
        case PET_MOOD_HUNGRY:    return slime_hungry;
        case PET_MOOD_MISERABLE: return slime_miserable;
        case PET_MOOD_CONTENT:
        default:                 return slime_content;
    }
}

const uint16_t *pet_view_sprite_for_anim(pet_anim_id_t anim)
{
    switch (anim) {
        /* Only the states with their own drawn artwork appear here. PLAY, CLEAN
         * and RESTART are the mood sprite put through a transform, which is why
         * they need no separate drawings. */
        case PET_ANIM_FEED:    return slime_eating;
        case PET_ANIM_DEATH:   return slime_dead;
        case PET_ANIM_RUNAWAY: return slime_runaway;
        default:               return NULL;
    }
}

const uint16_t *pet_view_sprite(const pet_t *pet, const pet_anim_t *anim)
{
    if (pet == NULL) {
        return slime_content;
    }

    const pet_anim_id_t showing = pet_anim_select(pet, anim);

    const uint16_t *specific = pet_view_sprite_for_anim(showing);
    if (specific != NULL) {
        return specific;
    }

    return pet_view_sprite_for_mood(pet_mood(pet));
}

uint8_t pet_view_frame(const pet_anim_t *anim)
{
    return (anim == NULL) ? 0u : pet_anim_frame(anim);
}

/* ---------------------------------------------------------------------------
 * Frame composition
 * ------------------------------------------------------------------------- */

void pet_view_compose_stage(uint16_t *fb, uint16_t fb_w, uint16_t fb_h,
                            const pet_t *pet, const pet_anim_t *anim)
{
    if ((fb == NULL) || (fb_w == 0U) || (fb_h == 0U)) {
        return;
    }

    const pet_anim_id_t showing = pet_anim_select(pet, anim);
    const pet_xform_t *xf = pet_frame_xform(showing, pet_view_frame(anim));

    pet_render_sprite_scaled(fb, fb_w, fb_h,
                             pet_view_sprite(pet, anim), PET_SPRITE_W, PET_SPRITE_H,
                             xf, PET_STAGE_COLOR);
}
