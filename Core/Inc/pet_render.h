/**
 * @file    pet_render.h
 * @brief   Builds animation frames by scaling a single sprite.
 *
 * Animation frames are all the same base image scaled to different sizes, rather
 * than a separate drawing per frame. A slime's movement is mostly squash and
 * stretch, so this saves a lot of flash and keeps the frames consistent.
 *
 * No hardware dependency - it draws into a pixel buffer the caller supplies, so
 * the geometry can be checked on a PC.
 */
#ifndef PET_RENDER_H
#define PET_RENDER_H

#include <stdint.h>

#include "pet_anim.h"

/*
 * Scale for one frame, in Q8 fixed point: 256 = unchanged, 512 = double size,
 * 128 = half. Scaling is anchored on the sprite's bottom centre so squashing
 * keeps the feet on the ground. The offsets shift the sprite after scaling,
 * which is what makes the jump and the sideways drift work.
 */
typedef struct {
    uint16_t scale_x;
    uint16_t scale_y;
    int16_t  offset_x;
    int16_t  offset_y;
} pet_xform_t;

/*
 * Draw a scaled copy of src into dst, filling the rest of dst with bg.
 * The loop goes from destination pixel back to source pixel so every pixel gets
 * written; mapping it the other way leaves gaps when enlarging.
 */
void pet_render_sprite_scaled(uint16_t *dst, uint16_t dst_w, uint16_t dst_h,
                              const uint16_t *src, uint16_t src_w, uint16_t src_h,
                              const pet_xform_t *xf, uint16_t bg);

/*
 * Transform for one frame of an animation. Out of range values are clamped so a
 * caller can't index past a table. Never returns NULL.
 */
const pet_xform_t *pet_frame_xform(pet_anim_id_t anim, uint8_t frame);

#endif /* PET_RENDER_H */
