/**
 * @file    test_pet_shake.c
 * @brief   Tests for shake detection.
 *
 * The things that matter on the board: resting never fires, a real shake fires
 * exactly once, holding it hard doesn't count as a shake, continuous shaking is
 * rate-limited, and a broken sensor can't trigger Pet on its own.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_shake.c \
 *        Core/Src/pet_shake.c -o /tmp/test_pet_shake && /tmp/test_pet_shake
 */
#include <stdio.h>

#include "pet_shake.h"

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

#define ONE_G   ((int16_t)PET_SHAKE_LSB_PER_G)
#define HARD    ((int16_t)32000)          /* ~1.95 g on one axis: a jolt */

/*
 * Feed samples every 10 ms from start to start+duration. In each period the
 * first high_ms are a hard jolt on z and the rest is resting (1 g on z).
 * Returns how many times a shake fired.
 */
static int run(pet_shake_t *s, uint32_t start, uint32_t duration,
               uint32_t period, uint32_t high_ms)
{
    int fired = 0;
    for (uint32_t t = 0; t < duration; t += 10u) {
        const int16_t z = ((t % period) < high_ms) ? HARD : ONE_G;
        if (pet_shake_update(s, 0, 0, z, start + t)) { fired++; }
    }
    return fired;
}

static void test_mag2(void)
{
    section("Magnitude");
    CHECK(pet_shake_mag2(0, 0, ONE_G) == (uint32_t)ONE_G * (uint32_t)ONE_G, "1 g on z");
    CHECK(pet_shake_mag2(-32768, -32768, -32768) == 3221225472u,
          "the most negative reading on all axes does not overflow");
}

static void test_rest_never_fires(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("Resting");
    CHECK(run(&s, 0, 5000, 100, 0) == 0, "5 s sitting still never fires");
}

static void test_real_shake_fires_once(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("A real shake");
    /* jolts at 0, 100, 200 ms, then stop */
    CHECK(run(&s, 0, 300, 100, 50) == 1, "three quick jolts fire once");
    CHECK(run(&s, 300, 3000, 100, 0) == 0, "and nothing more while it rests");
}

static void test_two_jolts_not_enough(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("Not quite a shake");
    CHECK(run(&s, 0, 200, 100, 50) == 0, "two jolts do not fire");
    CHECK(run(&s, 200, 2000, 100, 0) == 0, "and they expire instead of carrying over");
    CHECK(run(&s, 2200, 100, 100, 50) == 0, "a later single jolt still doesn't fire");
}

static void test_slow_jolts_do_not_fire(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("Bumps spread out in time");
    /* one jolt every 600 ms: never 3 inside a 1000 ms window */
    CHECK(run(&s, 0, 6000, 600, 50) == 0, "slow bumps never add up to a shake");
}

static void test_holding_hard_counts_once(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("Holding it hard");
    int fired = 0;
    for (uint32_t t = 0; t < 2000u; t += 10u) {
        if (pet_shake_update(&s, 0, 0, HARD, t)) { fired++; }
    }
    CHECK(fired == 0, "a sustained push is one jolt, not a shake");
}

static void test_cooldown_limits_rate(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("Shaking without stopping");
    /* fires at 200 ms; cooldown to 1700; fires again at 1900; cooldown to 3400 */
    CHECK(run(&s, 0, 3000, 100, 50) == 2,
          "3 s of non-stop shaking fires twice, not once per jolt");
}

static void test_stuck_high_never_fires(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("Sensor stuck at full scale");
    int fired = 0;
    for (uint32_t t = 0; t < 5000u; t += 10u) {
        if (pet_shake_update(&s, 32767, 32767, 32767, t)) { fired++; }
    }
    CHECK(fired == 0, "a stuck-high reading never fires");
}

static void test_all_zero_never_fires(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("Sensor reading all zeros (asleep or disconnected)");
    int fired = 0;
    for (uint32_t t = 0; t < 5000u; t += 10u) {
        if (pet_shake_update(&s, 0, 0, 0, t)) { fired++; }
    }
    CHECK(fired == 0, "all-zero readings never fire");
}

static void test_tick_wraparound(void)
{
    pet_shake_t s;
    pet_shake_init(&s);
    section("HAL_GetTick wrapping round");
    const uint32_t start = 0xFFFFFFFFu - 150u;   /* the 3 jolts straddle the wrap */
    CHECK(run(&s, start, 300, 100, 50) == 1, "a shake across the wrap fires once");
    CHECK(run(&s, start + 300u, 1000, 100, 0) == 0, "and the cooldown still holds");
}

static void test_null_is_safe(void)
{
    section("A NULL detector is refused rather than dereferenced");
    pet_shake_init(NULL);
    CHECK(!pet_shake_update(NULL, 0, 0, HARD, 0), "an update on NULL returns false");
}

int main(void)
{
    printf("pet_shake tests\n");

    test_mag2();
    test_rest_never_fires();
    test_real_shake_fires_once();
    test_two_jolts_not_enough();
    test_slow_jolts_do_not_fire();
    test_holding_hard_counts_once();
    test_cooldown_limits_rate();
    test_stuck_high_never_fires();
    test_all_zero_never_fires();
    test_tick_wraparound();
    test_null_is_safe();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
