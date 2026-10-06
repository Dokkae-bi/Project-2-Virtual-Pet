/**
 ******************************************************************************
 * @file    test_pet_core.c
 * @brief   Host test suite for the pet model (pet_core.c).
 *
 * The pet model has no hardware dependency, so its behaviour can be verified on
 * a development PC without the board, a display or a debug probe. Run with:
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_core.c Core/Src/pet_core.c -o /tmp/test_pet_core && /tmp/test_pet_core
 *
 * Exit status is 0 when every check passes, 1 otherwise, so this can be wired
 * into CI later.
 ******************************************************************************
 */
#include <stdio.h>
#include <stdbool.h>

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

static void section(const char *name)
{
    printf("\n%s\n", name);
}

/* --------------------------------------------------------------------------- */

static void test_init(void)
{
    pet_t p;
    pet_init(&p, 0);

    section("Initialisation");
    CHECK(p.hunger == PET_INIT_HUNGER, "hunger starts at the documented initial value");
    CHECK(p.happiness == PET_INIT_HAPPINESS, "happiness starts at the documented initial value");
    CHECK(p.state == PET_ALIVE, "pet starts alive");
    CHECK(pet_is_interactive(&p), "a fresh pet accepts interaction");
    CHECK(p.age_ms == 0, "age starts at zero");
}

static void test_time_evolution(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, 20000);   /* 20 s */

    section("Evolution over 20 s (documented rates: +1 hunger / 1.2 s, -1 happiness / 2.0 s)");
    CHECK(p.hunger == 41, "hunger 25 -> 41 after 20 s");
    CHECK(p.happiness == 65, "happiness 75 -> 65 after 20 s");
    CHECK(p.age_ms == 20000, "age accumulates elapsed time");
    CHECK(p.state == PET_ALIVE, "still alive after 20 s");
}

static void test_call_frequency_independence(void)
{
    pet_t single, stepped;
    uint32_t t;

    pet_init(&single, 0);
    pet_tick(&single, 20000);

    pet_init(&stepped, 0);
    for (t = 500; t <= 20000; t += 500) {
        pet_tick(&stepped, t);
    }

    section("Elapsed-time scheduling (result must not depend on tick frequency)");
    CHECK(stepped.hunger == single.hunger,
          "hunger identical for 1 tick vs 40 ticks over the same interval");
    CHECK(stepped.happiness == single.happiness,
          "happiness identical for 1 tick vs 40 ticks over the same interval");
    CHECK(stepped.hunger_accum_ms == single.hunger_accum_ms,
          "unconsumed time remainder is preserved exactly");
}

static void test_clamping(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, 20000);        /* hunger 41, happiness 65 */

    section("Documented limits: statistics are clamped to [0, 100]");
    pet_feed(&p, 5, 20000);     /* -60 hunger -> would be -19 */
    CHECK(p.hunger == PET_STAT_MIN, "over-feeding clamps hunger to the floor, not negative");
    CHECK(p.happiness == 69, "feeding adds the documented happiness bonus");

    pet_play(&p, 21000);
    pet_clean(&p, 22000);
    CHECK(p.happiness <= PET_STAT_MAX, "happiness never exceeds the ceiling");

    /* Push happiness hard against the ceiling. */
    { int i; for (i = 0; i < 20; i++) { pet_play(&p, 23000); } }
    CHECK(p.happiness == PET_STAT_MAX, "happiness clamps to the ceiling exactly");
}

static void test_feed_portion_clamping(void)
{
    pet_t p;

    section("Feed portion is clamped into [PET_FEED_PORTION_MIN, MAX]");
    pet_init(&p, 0);
    pet_tick(&p, 20000);            /* hunger 41 */
    pet_feed(&p, 0, 20000);
    CHECK(p.hunger == 41 - PET_FEED_HUNGER_PER_PORTION,
          "portion 0 is treated as the minimum portion");

    pet_init(&p, 0);
    pet_tick(&p, 20000);            /* hunger 41 */
    pet_feed(&p, 99, 20000);
    CHECK(p.hunger == PET_STAT_MIN,
          "portion 99 is treated as the maximum portion and clamps at the floor");
}

static void test_action_effects(void)
{
    pet_t p;

    section("Action effects");
    pet_init(&p, 0);
    pet_play(&p, 1000);
    CHECK(p.happiness == PET_INIT_HAPPINESS + PET_PLAY_HAPPINESS_GAIN, "play raises happiness");
    CHECK(p.hunger == PET_INIT_HUNGER + PET_PLAY_HUNGER_COST, "play costs hunger");
    CHECK(p.last_action == PET_ACTION_PLAY, "last action recorded");
    CHECK(p.action_count[PET_ACTION_PLAY] == 1, "play counted");

    pet_init(&p, 0);
    pet_clean(&p, 1000);
    CHECK(p.happiness == PET_INIT_HAPPINESS + PET_CLEAN_HAPPINESS_GAIN, "clean raises happiness");
    CHECK(p.action_count[PET_ACTION_CLEAN] == 1, "clean counted");
}

static void test_death_by_neglect(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, 90000);         /* 75 units x 1200 ms = 90 s to full hunger */

    section("Terminal state: death by neglect");
    CHECK(p.hunger == PET_STAT_MAX, "hunger reaches the documented ceiling after 90 s");
    CHECK(p.state == PET_DEAD, "pet is dead");
    CHECK(!pet_is_interactive(&p), "a dead pet refuses interaction");
    CHECK(pet_mood(&p) == PET_MOOD_MISERABLE, "dead pet displays its worst mood");
}

static void test_runaway_by_feeding(void)
{
    pet_t p;
    uint32_t t;
    bool ran_away = false;

    pet_init(&p, 0);

    /* Keep hunger satisfied by feeding, which lets happiness be the statistic
     * that reaches its limit first. This is how the runaway ending is reached
     * deliberately rather than by pure neglect. */
    for (t = 1000; t <= 600000; t += 1000) {
        pet_tick(&p, t);
        if (p.hunger >= 50) { pet_feed(&p, 5, t); }
        if (p.state == PET_RUNAWAY) { ran_away = true; break; }
        if (p.state == PET_DEAD)    { break; }
    }

    section("Terminal state: runaway by sustained neglect of happiness");
    printf("         reached at t = %u ms, hunger = %d, happiness = %d\n",
           t, p.hunger, p.happiness);
    CHECK(ran_away, "pet runs away rather than dying when hunger is kept satisfied");
    CHECK(p.state == PET_RUNAWAY, "pet is in the runaway state");
    CHECK(p.happiness == PET_STAT_MIN, "happiness reached its documented floor");
    CHECK(p.hunger < PET_STAT_MAX, "pet did not starve");

    section("Frozen after the end state");
    {
        pet_t frozen = p;
        pet_tick(&p, t + 300000);
        CHECK(p.hunger == frozen.hunger && p.happiness == frozen.happiness,
              "statistics stop changing after the pet has run away");
        CHECK(pet_feed(&p, 5, t + 1000) == false, "feeding is refused after the pet has run away");
        CHECK(pet_play(&p, t + 1000) == false, "playing is refused after the pet has run away");
    }
}

static void test_restart(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, 90000);
    CHECK(p.state == PET_DEAD, "pet is dead before the restart");

    section("Restart procedure");
    pet_restart(&p, 95000);
    CHECK(p.hunger == PET_INIT_HUNGER, "restart restores the initial hunger");
    CHECK(p.happiness == PET_INIT_HAPPINESS, "restart restores the initial happiness");
    CHECK(p.state == PET_ALIVE, "restart revives the pet");
    CHECK(pet_is_interactive(&p), "pet accepts interaction again after the restart");
    CHECK(p.age_ms == 0, "restart resets the lifetime");
    CHECK(pet_feed(&p, 3, 96000), "feeding works again after the restart");
}

static void test_mood(void)
{
    pet_t p;

    section("Mood selection (drives the idle animation)");
    pet_init(&p, 0);
    CHECK(pet_mood(&p) == PET_MOOD_DELIGHTED, "starts delighted at happiness 75");

    pet_tick(&p, 40000);   /* happiness 55, hunger 58 */
    CHECK(pet_mood(&p) == PET_MOOD_CONTENT, "content in the middle band");

    pet_tick(&p, 70000);   /* happiness 40, hunger 83 */
    CHECK(pet_mood(&p) == PET_MOOD_HUNGRY, "hungry once hunger passes the threshold");

    { pet_t p2; pet_init(&p2, 0); pet_tick(&p2, 110000);
      CHECK(pet_mood(&p2) == PET_MOOD_MISERABLE, "miserable once happiness drops low"); }
}

/* --------------------------------------------------------------------------- */

int main(void)
{
    printf("pet_core host test suite\n");
    printf("rates: hunger +1/%u ms, happiness -1/%u ms, limits [%d, %d]\n",
           PET_HUNGER_MS_PER_UNIT, PET_HAPPINESS_MS_PER_UNIT,
           PET_STAT_MIN, PET_STAT_MAX);

    test_init();
    test_time_evolution();
    test_call_frequency_independence();
    test_clamping();
    test_feed_portion_clamping();
    test_action_effects();
    test_death_by_neglect();
    test_runaway_by_feeding();
    test_restart();
    test_mood();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
