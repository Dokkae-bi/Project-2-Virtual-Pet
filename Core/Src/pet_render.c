/**
 * @file    pet_render.c
 * @brief   Frame transforms. See pet_render.h.
 */
#include "pet_render.h"

#include <stddef.h>

/*
 * One table per animation, with the same frame counts as pet_anim.c.
 * Values are Q8 scale factors, 256 = normal size.
 *
 * IDLE    slow breathing, so only the height changes
 * FEED    squashes down and spreads out as the berry goes in
 * PLAY    jump: stretches upward while rising off the ground
 * CLEAN   small wobble either side of rest
 * DEATH   collapses flat into a puddle
 * RUNAWAY drifts sideways, so repeating it reads as moving away
 * RESTART grows back up from a droplet
 */
static const pet_xform_t X_IDLE[4] = {
    { 256, 256, 0,  0 },
    { 256, 262, 0,  0 },
    { 256, 256, 0,  0 },
    { 256, 250, 0,  0 },
};

static const pet_xform_t X_FEED[5] = {
    { 256, 256, 0, 0 },
    { 262, 246, 0, 0 },
    { 272, 232, 0, 0 },
    { 262, 246, 0, 0 },
    { 256, 256, 0, 0 },
};

static const pet_xform_t X_PLAY[5] = {
    { 256, 256,  0,  0 },
    { 248, 292,  0, -4 },
    { 244, 320,  0, -8 },
    { 248, 292,  0, -4 },
    { 256, 256,  0,  0 },
};

static const pet_xform_t X_CLEAN[5] = {
    { 256, 256, 0, 0 },
    { 264, 250, 0, 0 },
    { 256, 256, 0, 0 },
    { 264, 250, 0, 0 },
    { 256, 256, 0, 0 },
};

static const pet_xform_t X_DEATH[4] = {
    { 256, 256, 0, 0 },
    { 274, 222, 0, 0 },
    { 294, 194, 0, 0 },
    { 308, 178, 0, 0 },
};

static const pet_xform_t X_RUNAWAY[4] = {
    { 256, 256,  0, 0 },
    { 248, 252,  5, 0 },
    { 240, 250, 10, 0 },
    { 232, 248, 15, 0 },
};

static const pet_xform_t X_RESTART[3] = {
    { 112, 112, 0, 0 },
    { 184, 184, 0, 0 },
    { 256, 256, 0, 0 },
};

const pet_xform_t *pet_frame_xform(pet_anim_id_t anim, uint8_t frame)
{
    const pet_xform_t *table;
    uint8_t count;

    switch (anim) {
        case PET_ANIM_FEED:    table = X_FEED;    count = 5; break;
        case PET_ANIM_PLAY:    table = X_PLAY;    count = 5; break;
        case PET_ANIM_CLEAN:   table = X_CLEAN;   count = 5; break;
        case PET_ANIM_DEATH:   table = X_DEATH;   count = 4; break;
        case PET_ANIM_RUNAWAY: table = X_RUNAWAY; count = 4; break;
        case PET_ANIM_RESTART: table = X_RESTART; count = 3; break;
        case PET_ANIM_IDLE:
        default:               table = X_IDLE;    count = 4; break;
    }

    if (frame >= count) {
        frame = (uint8_t)(count - 1U);
    }
    return &table[frame];
}

void pet_render_sprite_scaled(uint16_t *dst, uint16_t dst_w, uint16_t dst_h,
                              const uint16_t *src, uint16_t src_w, uint16_t src_h,
                              const pet_xform_t *xf, uint16_t bg)
{
    if ((dst == NULL) || (dst_w == 0U) || (dst_h == 0U)) {
        return;
    }

    /* Sprites are opaque and sit on the bottom edge, so the part they don't
       cover has to be repainted or the previous frame shows through. */
    const uint32_t dst_px = (uint32_t)dst_w * dst_h;
    for (uint32_t i = 0; i < dst_px; i++) {
        dst[i] = bg;
    }

    if ((src == NULL) || (xf == NULL) || (src_w == 0U) || (src_h == 0U)) {
        return;
    }
    if ((xf->scale_x == 0U) || (xf->scale_y == 0U)) {
        return;
    }

    /* Size the sprite ends up at, in whole pixels. */
    const int32_t nw = ((int32_t)src_w * (int32_t)xf->scale_x) >> 8;
    const int32_t nh = ((int32_t)src_h * (int32_t)xf->scale_y) >> 8;
    if ((nw <= 0) || (nh <= 0)) {
        return;
    }

    /* Place it on the bottom centre of the canvas, then apply the offset. */
    const int32_t ox = ((int32_t)dst_w - nw) / 2 + xf->offset_x;
    const int32_t oy = ((int32_t)dst_h - nh)     + xf->offset_y;

    for (int32_t dy = 0; dy < nh; dy++) {
        int32_t sy = ((int32_t)dy << 8) / (int32_t)xf->scale_y;
        if (sy >= (int32_t)src_h) { sy = (int32_t)src_h - 1; }

        const int32_t py = oy + dy;
        if ((py < 0) || (py >= (int32_t)dst_h)) { continue; }

        for (int32_t dx = 0; dx < nw; dx++) {
            int32_t sx = ((int32_t)dx << 8) / (int32_t)xf->scale_x;
            if (sx >= (int32_t)src_w) { sx = (int32_t)src_w - 1; }

            const int32_t px = ox + dx;
            if ((px < 0) || (px >= (int32_t)dst_w)) { continue; }

            dst[(uint32_t)py * dst_w + (uint32_t)px] =
                src[(uint32_t)sy * src_w + (uint32_t)sx];
        }
    }
}
