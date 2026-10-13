/**
 ******************************************************************************
 * @file    pet_view.h
 * @brief   Chooses which sprite to draw and composes one completed stage frame.
 *
 * This is the glue between the three logic layers and the artwork:
 *
 *     pet_core  -> what is true about the pet
 *     pet_anim  -> which animation should be playing
 *     pet_render-> how a frame of that animation is transformed
 *     pet_view  -> which artwork to use, and the composed frame
 *
 * No hardware dependency, so the artwork selection is testable on a host PC:
 * we can assert that a dead pet really does get the dead sprite.
 ******************************************************************************
 */
#ifndef PET_VIEW_H
#define PET_VIEW_H

#include <stdint.h>

#include "pet_core.h"
#include "pet_anim.h"
#include "pet_render.h"

/** Every generated sprite is this size (see tools/png_to_rgb565.py). */
#define PET_SPRITE_W   64u
#define PET_SPRITE_H   64u

/**
 * Stage background, RGB565. This MUST match the colour the asset converter
 * composites sprites onto (--stage, default #1E2A32), otherwise the sprite's
 * rectangular edges become visible against the stage.
 */
#define PET_STAGE_COLOR  0x1946u

/** Idle artwork for a mood. Never returns NULL. */
const uint16_t *pet_view_sprite_for_mood(pet_mood_t mood);

/**
 * Artwork that belongs to a specific animation, or NULL when the animation is
 * drawn using the mood sprite plus a transform (PLAY, CLEAN, RESTART).
 */
const uint16_t *pet_view_sprite_for_anim(pet_anim_id_t anim);

/**
 * @brief The sprite to draw right now.
 *
 * A finished life wins over everything (death, then runaway); an action
 * animation that supplied its own artwork wins next (feeding); otherwise the
 * current mood decides. Mirrors the priority in pet_anim_select().
 */
const uint16_t *pet_view_sprite(const pet_t *pet, const pet_anim_t *anim);

/** Current frame index, or 0 when no animation is in flight. */
uint8_t pet_view_frame(const pet_anim_t *anim);

/**
 * @brief Compose one complete stage frame into a pixel buffer.
 *
 * Fills the buffer with PET_STAGE_COLOR and draws the selected sprite through
 * the current frame's transform. The result is exactly what gets pushed to the
 * display, so the caller only has to hand it to ILI9341_DrawBitmap().
 */
void pet_view_compose_stage(uint16_t *fb, uint16_t fb_w, uint16_t fb_h,
                            const pet_t *pet, const pet_anim_t *anim);

#endif /* PET_VIEW_H */
