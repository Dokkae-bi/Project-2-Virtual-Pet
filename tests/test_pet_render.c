/**
 ******************************************************************************
 * @file    test_pet_render.c
 * @brief   Host test suite for the sprite transform engine.
 *
 * The engine fills plain pixel buffers, so its geometry is verifiable without
 * the board: we can assert exactly which source pixel lands where.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_render.c \
 *        Core/Src/pet_render.c Core/Src/pet_anim.c Core/Src/pet_core.c \
 *        -o /tmp/test_pet_render && /tmp/test_pet_render
 ******************************************************************************
 */
#include <stdio.h>
#include <string.h>

#include "pet_render.h"
#include "pet_anim.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) {                                                       \
            g_pass++;                                                     \
            printf("  [PASS] %s\n", (msg));                               \
        } else {                                                          \
            g_fail++;                                                     \
            printf("  [FAIL] %s   (%s:%d)\n", (msg), __FILE__, __LINE__);  \
        }                                                                 \
    } while (0)

static void section(const char *name) { printf("\n%s\n", name); }

#define BG 0xBEEFu

/* --------------------------------------------------------------------------- */

static void test_identity(void)
{
    uint16_t src[4 * 4], dst[4 * 4];
    const pet_xform_t one = { 256, 256, 0, 0 };
    int i;

    for (i = 0; i < 16; i++) { src[i] = (uint16_t)(0x1000 + i); }

    section("Identity transform (scale 256 = unchanged)");
    pet_render_sprite_scaled(dst, 4, 4, src, 4, 4, &one, BG);
    CHECK(memcmp(dst, src, sizeof(src)) == 0,
          "a 1:1 transform reproduces the source exactly");
}

static void test_bottom_anchor(void)
{
    uint16_t src[4 * 4], dst[4 * 4 * 2];   /* 4 wide, 8 tall */
    const pet_xform_t one = { 256, 256, 0, 0 };
    int i;

    for (i = 0; i < 16; i++) { src[i] = (uint16_t)(0x2000 + i); }

    section("Bottom anchor and background fill");
    pet_render_sprite_scaled(dst, 4, 8, src, 4, 4, &one, BG);

    CHECK(dst[0] == BG && dst[7] == BG && dst[15] == BG,
          "the area above the sprite is painted with the stage colour");
    CHECK(memcmp(&dst[4 * 4], src, sizeof(src)) == 0,
          "the sprite sits flush against the bottom edge");
}

static void test_upscale_no_holes(void)
{
    uint16_t src[2 * 2], dst[4 * 4];
    const pet_xform_t x2 = { 512, 512, 0, 0 };

    src[0] = 0x1111; src[1] = 0x2222; src[2] = 0x3333; src[3] = 0x4444;

    section("Upscaling fills every destination pixel (no holes)");
    pet_render_sprite_scaled(dst, 4, 4, src, 2, 2, &x2, BG);

    {
        int holes = 0, i;
        for (i = 0; i < 16; i++) { if (dst[i] == BG) { holes++; } }
        CHECK(holes == 0, "a 2x enlargement leaves no unpainted pixel");
    }
    /* Nearest neighbour: each source pixel becomes a 2x2 block. */
    CHECK(dst[0] == 0x1111 && dst[1] == 0x1111 && dst[4] == 0x1111,
          "the first source pixel becomes a 2x2 block");
    CHECK(dst[3] == 0x2222 && dst[15] == 0x4444,
          "the last source pixel lands in the far corner");
}

static void test_downscale(void)
{
    uint16_t src[4 * 4], dst[4 * 4];
    const pet_xform_t half = { 128, 128, 0, 0 };
    int x, y;

    for (y = 0; y < 4; y++) {
        for (x = 0; x < 4; x++) { src[y * 4 + x] = (uint16_t)(0x3000 + y * 10 + x); }
    }

    section("Downscaling halves the sprite and centres it");
    pet_render_sprite_scaled(dst, 4, 4, src, 4, 4, &half, BG);

    /* 4x4 at half scale is 2x2, anchored bottom-centre: columns 1-2, rows 2-3. */
    CHECK(dst[0] == BG && dst[3] == BG, "the outer columns are background");
    CHECK(dst[2 * 4 + 1] == src[0] && dst[2 * 4 + 2] == src[2],
          "the surviving pixels are evenly sampled from the source");
    CHECK(dst[3 * 4 + 1] == src[2 * 4 + 0] && dst[3 * 4 + 2] == src[2 * 4 + 2],
          "the reduced sprite sits on the bottom edge");
}

static void test_offset(void)
{
    uint16_t src[4 * 4], base[4 * 8], lifted[4 * 8];
    const pet_xform_t one = { 256, 256, 0,  0 };
    const pet_xform_t up  = { 256, 256, 0, -4 };
    int i;

    for (i = 0; i < 16; i++) { src[i] = (uint16_t)(0x4000 + i); }

    section("Vertical offset (used for the jump in PLAY)");

    /* On a 4x8 canvas a 4x4 sprite is bottom-anchored into rows 4-7. */
    pet_render_sprite_scaled(base, 4, 8, src, 4, 4, &one, BG);
    CHECK(memcmp(&base[16], src, sizeof(src)) == 0,
          "with no offset the sprite occupies the bottom rows");

    /* A -4 offset lifts it by exactly four rows, into rows 0-3. */
    pet_render_sprite_scaled(lifted, 4, 8, src, 4, 4, &up, BG);
    CHECK(memcmp(&lifted[0], src, sizeof(src)) == 0,
          "a -4 offset lifts the sprite by exactly four rows");
    CHECK(lifted[16] == BG,
          "the rows it vacated are repainted with the stage colour");
}

static void test_degenerate_cases(void)
{
    uint16_t src[4 * 4], dst[4 * 4];
    const pet_xform_t zero = { 0, 256, 0, 0 };
    const pet_xform_t huge = { 256, 256, 400, 400 };   /* entirely off-canvas */
    int i;

    for (i = 0; i < 16; i++) { src[i] = 0x5555; }

    section("Degenerate inputs do not crash or corrupt the buffer");
    pet_render_sprite_scaled(dst, 4, 4, src, 4, 4, &zero, BG);
    CHECK(dst[0] == BG && dst[15] == BG, "a zero scale fills with background only");

    pet_render_sprite_scaled(dst, 4, 4, src, 4, 4, &huge, BG);
    CHECK(dst[0] == BG && dst[15] == BG, "an off-canvas offset paints nothing");

    pet_render_sprite_scaled(dst, 4, 4, NULL, 4, 4, NULL, BG);
    CHECK(dst[0] == BG, "a NULL source is refused rather than dereferenced");
}

static void test_frame_tables(void)
{
    int a;

    section("Every animation has a frame transform for every frame");
    for (a = 0; a < PET_ANIM_COUNT; a++) {
        const pet_anim_def_t *def = pet_anim_def((pet_anim_id_t)a);
        int f, complete = 1;
        for (f = 0; f < def->frames; f++) {
            if (pet_frame_xform((pet_anim_id_t)a, (uint8_t)f) == NULL) { complete = 0; }
        }
        CHECK(complete, pet_anim_name((pet_anim_id_t)a));
    }

    section("Out-of-range frames clamp instead of reading past the table");
    CHECK(pet_frame_xform(PET_ANIM_IDLE, 200) == pet_frame_xform(PET_ANIM_IDLE, 3),
          "an over-large frame index clamps to the last frame");
    CHECK(pet_frame_xform((pet_anim_id_t)99, 0) == pet_frame_xform(PET_ANIM_IDLE, 0),
          "an unknown animation falls back to IDLE");
}

/*
 * This is the requirement stated in the brief: "An animation must visibly change
 * across time; displaying one static image with different text is not
 * sufficient." These checks assert that consecutive frames always differ, and
 * that the spread across an animation is large enough to actually be seen.
 */
static void test_animations_visibly_change(void)
{
    int a;

    section("Animations visibly change across time (project requirement)");
    for (a = 0; a < PET_ANIM_COUNT; a++) {
        const pet_anim_def_t *def = pet_anim_def((pet_anim_id_t)a);
        int f, max_spread = 0;
        int consecutive_differs = 1;

        for (f = 0; f < def->frames; f++) {
            const pet_xform_t *x = pet_frame_xform((pet_anim_id_t)a, (uint8_t)f);
            int g;

            for (g = 0; g < def->frames; g++) {
                const pet_xform_t *y = pet_frame_xform((pet_anim_id_t)a, (uint8_t)g);
                int d;
                d = (int)x->scale_x  - (int)y->scale_x;  if (d < 0) d = -d; if (d > max_spread) max_spread = d;
                d = (int)x->scale_y  - (int)y->scale_y;  if (d < 0) d = -d; if (d > max_spread) max_spread = d;
                d = (int)x->offset_x - (int)y->offset_x; if (d < 0) d = -d; if (d > max_spread) max_spread = d;
                d = (int)x->offset_y - (int)y->offset_y; if (d < 0) d = -d; if (d > max_spread) max_spread = d;
            }

            if (f > 0) {
                const pet_xform_t *p = pet_frame_xform((pet_anim_id_t)a, (uint8_t)(f - 1));
                if (memcmp(p, x, sizeof(*x)) == 0) { consecutive_differs = 0; }
            }
        }

        printf("         %-8s frames=%d  max spread=%d\n",
               pet_anim_name((pet_anim_id_t)a), def->frames, max_spread);
        CHECK(consecutive_differs, "no two consecutive frames are identical");
        /* 8 in Q8 is about 3% of size, or 8 px of movement - clearly visible. */
        CHECK(max_spread >= 8, "the animation spans a visible change");
    }
}

/* --------------------------------------------------------------------------- */

int main(void)
{
    printf("pet_render host test suite\n");

    test_identity();
    test_bottom_anchor();
    test_upscale_no_holes();
    test_downscale();
    test_offset();
    test_degenerate_cases();
    test_frame_tables();
    test_animations_visibly_change();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
