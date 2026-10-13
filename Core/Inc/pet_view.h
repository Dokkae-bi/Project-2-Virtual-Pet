/**
 * @file    pet_view.h
 * @brief   Picks which sprite to draw and builds one finished frame.
 *
 * Sits on top of the logic modules:
 *
 *   pet_core    what the pet's state is
 *   pet_anim    which animation should be playing
 *   pet_render  how a frame of that animation is scaled
 *   pet_view    which image to use and the finished stage frame
 *
 * It only picks a pointer and fills a buffer, so sprite selection can be tested
 * on a PC without the board.
 */
#ifndef PET_VIEW_H
#define PET_VIEW_H

#include <stdint.h>

#include "pet_core.h"
#include "pet_anim.h"
#include "pet_render.h"

/* Size of every sprite the converter produces. */
#define PET_SPRITE_W   64u
#define PET_SPRITE_H   64u

/*
 * Stage background in RGB565. Has to match the colour the converter composited
 * the sprites onto (--stage, default #1E2A32) or the sprite's rectangular edges
 * show up against the stage.
 */
#define PET_STAGE_COLOR  0x1946u

/* Idle sprite for a mood. Never NULL. */
const uint16_t *pet_view_sprite_for_mood(pet_mood_t mood);

/*
 * Sprite belonging to a specific animation, or NULL when that animation just
 * uses the mood sprite with a transform (PET, CLEAN and RESTART do).
 */
const uint16_t *pet_view_sprite_for_anim(pet_anim_id_t anim);

/*
 * The sprite to draw right now. Same priority as pet_anim_select(): death and
 * runaway first, then an animation with its own artwork, then the mood.
 */
const uint16_t *pet_view_sprite(const pet_t *pet, const pet_anim_t *anim);

/* Current frame index, or 0 if no animation is running. */
uint8_t pet_view_frame(const pet_anim_t *anim);

/*
 * Build one complete frame into fb: background first, then the chosen sprite
 * with the current frame's transform. The result is what gets passed to
 * ILI9341_DrawBitmap().
 */
void pet_view_compose_stage(uint16_t *fb, uint16_t fb_w, uint16_t fb_h,
                            const pet_t *pet, const pet_anim_t *anim);

#endif /* PET_VIEW_H */
