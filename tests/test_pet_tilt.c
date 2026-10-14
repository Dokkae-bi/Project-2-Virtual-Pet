/**
 * @file    test_pet_tilt.c
 * @brief   Tests for tilt-to-position.
 *
 * What matters on the board: lying flat never moves the slime, tipping either way
 * moves it that way and faster the further it tips, it stops at the screen edges,
 * and it can't move more often than the move period.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_tilt.c \
 *        Core/Src/pet_tilt.c -o /tmp/test_pet_tilt && /tmp/test_pet_tilt
 */
#include <stdio.h>

#include "pet_tilt.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) {                                                       \
            g_pass++;                                                     \
            printf("  [PASS] %s\n", (msg));                               \
        } else {                                                          \
            g_fail++;                                                     \
            printf("  [FAIL] %s   (%s:%d)\n", (msg), __FILE__, __LINE__); \
        }                                                                 \
    } while (0)

static void section(const char *name) { printf("\n%s\n", name); }

#define RIGHT(x)  ((int16_t)((x) * PET_TILT_DIR))
#define LEFT(x)   ((int16_t)(-(x) * PET_TILT_DIR))

static void test_init_centres(void)
{
    section("init");
    pet_tilt_t t;
    pet_tilt_init(&t, 0u);
    CHECK(t.x == PET_HUD_SPRITE_X, "starts in the centre of the stage");
    CHECK(PET_TILT_X_MAX == 176, "right limit leaves the sprite fully on screen");
}

static void test_dead_zone(void)
{
    section("dead zone");
    CHECK(pet_tilt_step(0) == 0, "flat: no movement");
    CHECK(pet_tilt_step(PET_TILT_DEAD_COUNTS) == 0, "exactly at the edge of the dead zone: none");
    CHECK(pet_tilt_step(-PET_TILT_DEAD_COUNTS) == 0, "same on the other side");
    CHECK(pet_tilt_step(PET_TILT_DEAD_COUNTS + 1) != 0, "just past it: moves");
}

static void test_direction_and_speed(void)
{
    section("direction and speed");
    const int16_t slow = pet_tilt_step(RIGHT(PET_TILT_DEAD_COUNTS + 1));
    const int16_t mid  = pet_tilt_step(RIGHT((PET_TILT_DEAD_COUNTS + PET_TILT_FULL_COUNTS) / 2));
    const int16_t fast = pet_tilt_step(RIGHT(PET_TILT_FULL_COUNTS));
    CHECK(slow == PET_TILT_MIN_STEP, "barely tipped: the minimum step, to the right");
    CHECK(fast == PET_TILT_MAX_STEP, "fully tipped: the maximum step");
    CHECK(slow < mid && mid < fast, "speed rises with the tilt");
    CHECK(pet_tilt_step(RIGHT(PET_TILT_FULL_COUNTS * 2)) == PET_TILT_MAX_STEP,
          "tipped further than full: capped");
    CHECK(pet_tilt_step(LEFT(PET_TILT_FULL_COUNTS)) == -PET_TILT_MAX_STEP, "tipped the other way: negative");
    CHECK(pet_tilt_step(-32768) == LEFT(PET_TILT_MAX_STEP), "most negative reading is capped, no overflow");
}

static void test_moves_and_clamps(void)
{
    section("moving and the screen edges");
    pet_tilt_t t;
    pet_tilt_init(&t, 0u);
    const int16_t start = t.x;

    CHECK(pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 0u), "tipped: reports a move");
    CHECK(t.x == start + PET_TILT_MAX_STEP, "moved right by one full step");

    uint32_t now = 0u;
    for (int i = 0; i < 100; i++) {
        now += PET_TILT_PERIOD_MS;
        (void)pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), now);
    }
    CHECK(t.x == PET_TILT_X_MAX, "held right: stops at the right edge");
    now += PET_TILT_PERIOD_MS;
    CHECK(!pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), now), "at the edge: no change reported");

    for (int i = 0; i < 100; i++) {
        now += PET_TILT_PERIOD_MS;
        (void)pet_tilt_update(&t, LEFT(PET_TILT_FULL_COUNTS), now);
    }
    CHECK(t.x == PET_TILT_X_MIN, "held left: stops at the left edge");
}

static void test_level_stops(void)
{
    section("levelling");
    pet_tilt_t t;
    pet_tilt_init(&t, 0u);
    (void)pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 0u);
    const int16_t where = t.x;
    CHECK(!pet_tilt_update(&t, 0, PET_TILT_PERIOD_MS), "level: no move reported");
    CHECK(t.x == where, "level: the slime stays where it is");
}

static void test_rate_limit(void)
{
    section("rate limit");
    pet_tilt_t t;
    pet_tilt_init(&t, 1000u);
    CHECK(pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 1000u), "first move allowed");
    const int16_t after = t.x;
    CHECK(!pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 1000u + PET_TILT_PERIOD_MS - 1u),
          "a sample inside the period doesn't move it");
    CHECK(t.x == after, "position unchanged");
    CHECK(pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 1000u + PET_TILT_PERIOD_MS),
          "once the period has passed it moves again");
}

static void test_wraparound(void)
{
    section("tick wraparound");
    pet_tilt_t t;
    pet_tilt_init(&t, 0xFFFFFFF0u);
    CHECK(pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 0xFFFFFFF0u), "moves just before the wrap");
    CHECK(!pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 0x00000005u),
          "a sample just after the wrap, inside the period, is still limited");
    CHECK(pet_tilt_update(&t, RIGHT(PET_TILT_FULL_COUNTS), 0x00000030u),
          "and one a period later moves");
}

static void test_null_is_safe(void)
{
    section("NULL");
    pet_tilt_init(NULL, 0u);
    CHECK(!pet_tilt_update(NULL, 20000, 0u), "NULL tracker: returns false, no crash");
}

int main(void)
{
    printf("pet_tilt tests\n");
    test_init_centres();
    test_dead_zone();
    test_direction_and_speed();
    test_moves_and_clamps();
    test_level_stops();
    test_rate_limit();
    test_wraparound();
    test_null_is_safe();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
