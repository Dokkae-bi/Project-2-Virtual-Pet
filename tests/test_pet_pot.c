/**
 * @file    test_pet_pot.c
 * @brief   Tests for the potentiometer -> portion mapping.
 *
 * What matters on the board: both ends of the knob reach 1 and 5, every
 * portion in between is reachable, a knob resting on a band edge doesn't make
 * the menu flicker, and turning it still responds quickly.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_pot.c \
 *        Core/Src/pet_pot.c -o /tmp/test_pet_pot && /tmp/test_pet_pot
 */
#include <stdio.h>

#include "pet_pot.h"

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

/* Feed the same reading n times; return how often the portion changed. */
static int hold(pet_pot_t *p, uint16_t raw, int n)
{
    int changes = 0;
    for (int i = 0; i < n; i++) {
        const uint8_t before = p->portion;
        if (pet_pot_update(p, raw) != before) { changes++; }
    }
    return changes;
}

static void test_bands(void)
{
    section("Bands");
    CHECK(pet_pot_portion_of(0) == 1, "fully anticlockwise is portion 1");
    /* 4096 counts / 5 bands = 819.2 counts each, so band 2 starts at 820. */
    CHECK(pet_pot_portion_of(819) == 1, "the top of the first band is still 1");
    CHECK(pet_pot_portion_of(820) == 2, "the next count is 2");
    CHECK(pet_pot_portion_of(2048) == 3, "the middle is 3");
    CHECK(pet_pot_portion_of(PET_POT_ADC_MAX) == 5, "fully clockwise is 5");
    CHECK(pet_pot_portion_of(0xFFFFu) == 5, "an impossible reading is clamped, not out of range");
}

static void test_first_reading(void)
{
    pet_pot_t p;
    pet_pot_init(&p);
    section("Power-up");
    CHECK(p.portion == PET_FEED_PORTION_MIN, "before any reading it is the smallest portion");
    CHECK(pet_pot_update(&p, PET_POT_ADC_MAX) == 5,
          "the first reading is used straight away, not ramped up to");
}

static void test_edge_does_not_flicker(void)
{
    pet_pot_t p;
    pet_pot_init(&p);
    section("Knob resting on a band edge");
    (void)pet_pot_update(&p, 810);
    int changes = 0;
    for (int i = 0; i < 200; i++) {
        const uint8_t before = p.portion;
        if (pet_pot_update(&p, (i & 1) ? 800 : 840) != before) { changes++; }
    }
    CHECK(changes == 0, "noise either side of the 1/2 edge never changes the portion");
}

static void test_turning_responds(void)
{
    pet_pot_t p;
    pet_pot_init(&p);
    section("Turning the knob");
    (void)pet_pot_update(&p, 400);
    (void)hold(&p, 1200, 10);
    CHECK(p.portion == 2, "turning into band 2 shows 2 within 10 readings (200 ms)");
    (void)hold(&p, 100, 20);
    CHECK(p.portion == 1, "turning back shows 1 again");
    (void)hold(&p, PET_POT_ADC_MAX, 20);
    CHECK(p.portion == 5, "a fast turn to the end reaches 5");
    (void)hold(&p, 0, 30);
    CHECK(p.portion == 1, "and a fast turn back reaches 1");
}

static void test_sweep_visits_every_portion(void)
{
    pet_pot_t p;
    pet_pot_init(&p);
    section("A slow sweep end to end");
    (void)pet_pot_update(&p, 0);
    int changes = 0;
    uint8_t last = p.portion;
    bool in_order = true;
    for (uint32_t raw = 0; raw <= PET_POT_ADC_MAX; raw += 8u) {
        const uint8_t now = pet_pot_update(&p, (uint16_t)raw);
        if (now != last) {
            changes++;
            if (now != (uint8_t)(last + 1u)) { in_order = false; }
            last = now;
        }
    }
    (void)hold(&p, PET_POT_ADC_MAX, 20);
    CHECK(p.portion == 5, "the sweep ends on 5");
    CHECK(changes == 4, "it steps through 1..5 exactly once each");
    CHECK(in_order, "with no skipped or repeated portions");
}

static void test_null_is_safe(void)
{
    section("A NULL pot is refused rather than dereferenced");
    pet_pot_init(NULL);
    CHECK(pet_pot_update(NULL, 4000) == PET_FEED_PORTION_MIN, "an update on NULL gives the smallest portion");
}

int main(void)
{
    printf("pet_pot tests\n");

    test_bands();
    test_first_reading();
    test_edge_does_not_flicker();
    test_turning_responds();
    test_sweep_visits_every_portion();
    test_null_is_safe();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
