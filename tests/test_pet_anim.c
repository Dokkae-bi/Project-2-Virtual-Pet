/**
 ******************************************************************************
 * @file    test_pet_anim.c
 * @brief   Host test suite for the animation sequencer and the state-to-animation
 *          mapping. Like the model, this layer has no hardware dependency, so its
 *          behaviour is verifiable without the board or the display.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_anim.c \
 *        Core/Src/pet_anim.c Core/Src/pet_core.c -o /tmp/test_pet_anim && /tmp/test_pet_anim
 ******************************************************************************
 */
#include <stdio.h>

#include "pet_anim.h"
#include "pet_core.h"

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

/* --------------------------------------------------------------------------- */

static void test_definitions(void)
{
    int i;
    section("Animation definitions");
    for (i = 0; i < PET_ANIM_COUNT; i++) {
        const pet_anim_def_t *d = pet_anim_def((pet_anim_id_t)i);
        CHECK(d != NULL, pet_anim_name((pet_anim_id_t)i));
        CHECK(d->frames > 0 && d->frames <= 16, "  frame count is sane");
        /* A frame must be long enough to push a 64x64 sprite over SPI (~37 ms). */
        CHECK(d->frame_ms >= 40, "  frame period exceeds the sprite transfer time");
    }
    /* Out-of-range ids must fall back rather than read past the table. */
    CHECK(pet_anim_def((pet_anim_id_t)99) == pet_anim_def(PET_ANIM_IDLE),
          "an out-of-range id falls back to IDLE");
}

static void test_start_and_advance(void)
{
    pet_anim_t a;
    section("Start and frame advance (IDLE: 4 frames, 250 ms, loops)");

    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    CHECK(a.frame == 0, "starts on frame 0");
    CHECK(!pet_anim_finished(&a), "a looping animation is never finished");

    pet_anim_update(&a, 250);
    CHECK(pet_anim_frame(&a) == 1, "frame 1 after one period");
    pet_anim_update(&a, 500);
    CHECK(pet_anim_frame(&a) == 2, "frame 2 after two periods");
    pet_anim_update(&a, 750);
    CHECK(pet_anim_frame(&a) == 3, "frame 3 after three periods");
    pet_anim_update(&a, 1000);
    CHECK(pet_anim_frame(&a) == 0, "wraps back to frame 0");
    CHECK(a.play_count == 1, "one full loop recorded");

    pet_anim_update(&a, 1100);
    CHECK(pet_anim_frame(&a) == 0, "partial period does not advance the frame");
}

static void test_one_shot(void)
{
    pet_anim_t a;
    section("One-shot animation (FEED: 5 frames, 120 ms)");

    pet_anim_start(&a, PET_ANIM_FEED, 0);
    pet_anim_update(&a, 600);           /* 600 / 120 = 5 periods */
    CHECK(pet_anim_frame(&a) == 4, "ends on the last frame");
    CHECK(pet_anim_finished(&a), "one-shot reports finished");
    CHECK(a.play_count == 1, "one pass recorded");

    pet_anim_update(&a, 5000);
    CHECK(pet_anim_frame(&a) == 4, "a finished animation holds its last frame");
    CHECK(a.play_count == 1, "a finished animation does not keep counting");
}

static void test_frequency_independence(void)
{
    pet_anim_t once, stepped;
    uint32_t t;

    section("Elapsed-time scheduling (does not depend on call frequency)");
    pet_anim_start(&once, PET_ANIM_FEED, 0);
    pet_anim_update(&once, 600);

    pet_anim_start(&stepped, PET_ANIM_FEED, 0);
    for (t = 100; t <= 600; t += 100) { pet_anim_update(&stepped, t); }

    CHECK(stepped.frame == once.frame, "same frame for 1 call vs 6 calls");
    CHECK(stepped.finished == once.finished, "same finished state");
    CHECK(stepped.elapsed_ms == once.elapsed_ms, "same leftover time");
}

static void test_long_gap(void)
{
    pet_anim_t a;
    section("A long gap consumes the right number of frames");

    /* A paused debugger or a slow full-screen redraw must not lose frames. */
    pet_anim_start(&a, PET_ANIM_FEED, 0);
    pet_anim_update(&a, 10000);
    CHECK(pet_anim_frame(&a) == 4, "lands on the last frame, not part-way");
    CHECK(pet_anim_finished(&a), "and reports finished");

    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    pet_anim_update(&a, 1000);          /* 4 periods of 250 ms = exactly one loop */
    CHECK(pet_anim_frame(&a) == 0, "looping animation consumes all whole loops");
    CHECK(a.play_count == 1, "and counts exactly one loop");
}

static void test_action_mapping(void)
{
    section("Action to animation mapping");
    CHECK(pet_anim_for_action(PET_ACTION_FEED)  == PET_ANIM_FEED,  "FEED maps to the feeding animation");
    CHECK(pet_anim_for_action(PET_ACTION_PLAY)  == PET_ANIM_PLAY,  "PLAY maps to the playing animation");
    CHECK(pet_anim_for_action(PET_ACTION_CLEAN) == PET_ANIM_CLEAN, "CLEAN maps to the cleaning animation");
    CHECK(pet_anim_for_action(PET_ACTION_NONE)  == PET_ANIM_IDLE,  "no action maps to idle");
}

static void test_selection(void)
{
    pet_t p;
    pet_anim_t a;

    section("State-to-animation selection");

    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_IDLE, "a living idle pet shows idle");

    pet_anim_start(&a, PET_ANIM_FEED, 1000);
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_FEED, "a running action animation wins over idle");

    pet_anim_update(&a, 2000);          /* FEED is 5 x 120 ms, so it is done */
    CHECK(pet_anim_finished(&a), "the feeding animation has finished");
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_IDLE, "and control returns to idle");

    section("A finished life overrides any animation in flight");
    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_PLAY, 1000);
    pet_tick(&p, 90000);                /* starved */
    CHECK(p.state == PET_DEAD, "pet has died");
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_DEATH,
          "death wins even though a play animation is still running");

    { pet_t r; uint32_t t; bool ran = false;
      pet_init(&r, 0);
      for (t = 1000; t <= 600000; t += 1000) {
          pet_tick(&r, t);
          if (r.hunger >= 50) { pet_feed(&r, 5, t); }
          if (r.state == PET_RUNAWAY) { ran = true; break; }
          if (r.state == PET_DEAD) { break; }
      }
      pet_anim_start(&a, PET_ANIM_CLEAN, t);
      CHECK(ran, "pet reached the runaway state");
      CHECK(pet_anim_select(&r, &a) == PET_ANIM_RUNAWAY,
            "runaway wins over a cleaning animation"); }
}

/* --------------------------------------------------------------------------- */

int main(void)
{
    printf("pet_anim host test suite\n");

    test_definitions();
    test_start_and_advance();
    test_one_shot();
    test_frequency_independence();
    test_long_gap();
    test_action_mapping();
    test_selection();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
