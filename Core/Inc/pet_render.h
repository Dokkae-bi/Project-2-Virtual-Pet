/**
 ******************************************************************************
 * @file    pet_render.h
 * @brief   Sprite transform engine - derives animation frames from a single
 *          base sprite by scaling, so the artwork does not need one drawing per
 *          frame.
 *
 * A slime's motion is squash and stretch, which is a scale transform of one
 * image. Rendering frames this way means:
 *   - one base sprite per state instead of five or six drawings each
 *   - every frame is pixel-identical in style, because it came from the same
 *     source, rather than relying on an image generator to stay consistent
 *   - far less flash, and more frames of motion than we could afford to draw
 *
 * Like pet_core and pet_anim this module has no hardware dependency: it fills a
 * caller-supplied pixel buffer, so it is fully testable on a host PC.
 ******************************************************************************
 */
#ifndef PET_RENDER_H
#define PET_RENDER_H

#include <stdint.h>

#include "pet_anim.h"

/**
 * @brief A scale-and-offset transform for one animation frame.
 *
 * Scale is Q8 fixed point: 256 means "unchanged", 128 is half size, 512 is
 * double. Scaling is anchored on the sprite's bottom centre so a squash keeps
 * the pet's feet planted on the ground rather than floating it.
 */
typedef struct {
    uint16_t scale_x;    /* Q8, 256 = 1.0 */
    uint16_t scale_y;    /* Q8, 256 = 1.0 */
    int16_t  offset_x;   /* pixels added after scaling, for drift */
    int16_t  offset_y;   /* negative moves the pet up, for a jump */
} pet_xform_t;

/**
 * @brief Render a scaled sprite into a destination buffer.
 *
 * The destination is filled with @p bg first, then the transformed sprite is
 * drawn bottom-centred (plus the transform's offsets). Every source pixel is
 * guaranteed to be covered because the mapping runs from destination back to
 * source - forward mapping would leave holes whenever the sprite is enlarged.
 *
 * @param dst destination pixels, dst_w * dst_h entries
 * @param src source pixels, src_w * src_h entries
 */
void pet_render_sprite_scaled(uint16_t *dst, uint16_t dst_w, uint16_t dst_h,
                              const uint16_t *src, uint16_t src_w, uint16_t src_h,
                              const pet_xform_t *xf, uint16_t bg);

/**
 * @brief The transform for one frame of an animation.
 *
 * Never returns NULL; an out-of-range animation or frame is clamped, so a
 * caller cannot index past a table.
 */
const pet_xform_t *pet_frame_xform(pet_anim_id_t anim, uint8_t frame);

#endif /* PET_RENDER_H */
