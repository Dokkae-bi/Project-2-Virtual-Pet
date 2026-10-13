/**
 * @file    test_pet_anim.c
 * @brief   Tests for the animation sequencer and the state-to-animation mapping.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_anim.c \
 *        Core/Src/pet_anim.c Core/Src/pet_core.c -o /tmp/test_pet_anim \
 *        && /tmp/test_pet_anim
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

static void test_definitions(void)
{
    int i;
    section("Animation definitions");
    for (i = 0; i < PET_ANIM_COUNT; i++) {
        const pet_anim_def_t *d = pet_anim_def((pet_anim_id_t)i);
        CHECK(d != NULL, pet_anim_name((pet_anim_id_t)i));
        CHECK(d->frames > 0 && d->frames <= 16, "  frame count is sensible");
        /* A frame has to be long enough to push a 64x64 sprite over SPI (~37 ms). */
        CHECK(d->frame_ms >= 40, "  frame period is longer than the SPI transfer");
    }
    /* Out of range ids should fall back instead of reading past the table. */
    CHECK(pet_anim_def((pet_anim_id_t)99) == pet_anim_def(PET_ANIM_IDLE),
          "an out of range id falls back to IDLE");
}

static void test_start_and_advance(void)
{
    pet_anim_t a;
    section("Start and advance (IDLE: 4 frames, 250 ms, loops)");

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
    CHECK(a.pet_count == 1, "one loop counted");

    pet_anim_update(&a, 1100);
    CHECK(pet_anim_frame(&a) == 0, "a partial period doesn't advance the frame");
}

static void test_one_shot(void)
{
    pet_anim_t a;
    section("One-shot animation (FEED: 5 frames, 120 ms)");

    pet_anim_start(&a, PET_ANIM_FEED, 0);
    pet_anim_update(&a, 600);           /* 600 / 120 = 5 periods */
    CHECK(pet_anim_frame(&a) == 4, "ends on the last frame");
    CHECK(pet_anim_finished(&a), "reports finished");
    CHECK(a.pet_count == 1, "one pass counted");

    pet_anim_update(&a, 5000);
    CHECK(pet_anim_frame(&a) == 4, "holds the last frame once finished");
    CHECK(a.pet_count == 1, "doesn't keep counting after finishing");
}

static void test_frequency_independence(void)
{
    pet_anim_t once, stepped;
    uint32_t t;

    section("Updating often gives the same result as updating once");
    pet_anim_start(&once, PET_ANIM_FEED, 0);
    pet_anim_update(&once, 600);

    pet_anim_start(&stepped, PET_ANIM_FEED, 0);
    for (t = 100; t <= 600; t += 100) { pet_anim_update(&stepped, t); }

    CHECK(stepped.frame == once.frame, "same frame for 1 call as for 6");
    CHECK(stepped.finished == once.finished, "same finished state");
    CHECK(stepped.elapsed_ms == once.elapsed_ms, "same leftover time");
}

static void test_long_gap(void)
{
    pet_anim_t a;
    section("A long gap catches up the right number of frames");

    /* A paused debugger or a slow redraw shouldn't lose frames. */
    pet_anim_start(&a, PET_ANIM_FEED, 0);
    pet_anim_update(&a, 10000);
    CHECK(pet_anim_frame(&a) == 4, "lands on the last frame, not part way");
    CHECK(pet_anim_finished(&a), "and reports finished");

    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    pet_anim_update(&a, 1000);          /* 4 periods of 250 ms = exactly one loop */
    CHECK(pet_anim_frame(&a) == 0, "looping animation consumes whole loops");
    CHECK(a.pet_count == 1, "and counts exactly one");
}

static void test_action_mapping(void)
{
    section("Action to animation mapping");
    CHECK(pet_anim_for_action(PET_ACTION_FEED)  == PET_ANIM_FEED,  "FEED -> FEED");
    CHECK(pet_anim_for_action(PET_ACTION_PET)  == PET_ANIM_PET,  "PET -> PET");
    CHECK(pet_anim_for_action(PET_ACTION_CLEAN) == PET_ANIM_CLEAN, "CLEAN -> CLEAN");
    CHECK(pet_anim_for_action(PET_ACTION_SLEEP) == PET_ANIM_IDLE,  "SLEEP -> IDLE");
    CHECK(pet_anim_for_action(PET_ACTION_NONE)  == PET_ANIM_IDLE,  "no action -> IDLE");
}

static void test_selection(void)
{
    pet_t p;
    pet_anim_t a;

    section("Which animation should be on screen");
    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_IDLE, 0);
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_IDLE, "a living idle pet shows IDLE");

    pet_anim_start(&a, PET_ANIM_FEED, 1000);
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_FEED, "a running action wins over IDLE");

    pet_anim_update(&a, 2000);          /* FEED is 5 x 120 ms, so it's done */
    CHECK(pet_anim_finished(&a), "the feeding animation has finished");
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_IDLE, "so it goes back to IDLE");

    section("A finished life beats any animation still running");
    pet_init(&p, 0);
    pet_anim_start(&a, PET_ANIM_PET, 1000);
    pet_tick(&p, 90000);                /* starved */
    CHECK(p.state == PET_DEAD, "pet has died");
    CHECK(pet_anim_select(&p, &a) == PET_ANIM_DEATH,
          "DEATH wins even though PET is still running");

    { pet_t r; uint32_t t; bool ran = false;
      pet_init(&r, 0);
      for (t = 1000; t <= 600000; t += 1000) {
          pet_tick(&r, t);
          if (r.food <= 50) { pet_feed(&r, 5, t); }
          if (r.state == PET_RUNAWAY) { ran = true; break; }
          if (r.state == PET_DEAD) { break; }
      }
      pet_anim_start(&a, PET_ANIM_CLEAN, t);
      CHECK(ran, "pet reached the runaway state");
      CHECK(pet_anim_select(&r, &a) == PET_ANIM_RUNAWAY,
            "RUNAWAY wins over a cleaning animation"); }
}

int main(void)
{
    printf("pet_anim tests\n");

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
