/**
 * @file    test_pet_core.c
 * @brief   Tests for the pet model, run on a PC instead of the board.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_core.c \
 *        Core/Src/pet_core.c -o /tmp/test_pet_core && /tmp/test_pet_core
 *
 * Exits 0 if every check passes, 1 otherwise.
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

static void test_init(void)
{
    pet_t p;
    pet_init(&p, 0);

    section("Initialisation");
    CHECK(p.food == PET_INIT_FOOD, "food starts at PET_INIT_FOOD");
    CHECK(p.happiness == PET_INIT_HAPPINESS, "happiness starts at PET_INIT_HAPPINESS");
    CHECK(p.state == PET_ALIVE, "starts alive");
    CHECK(pet_is_interactive(&p), "a new pet accepts interaction");
    CHECK(p.age_ms == 0, "age starts at zero");
}

static void test_time_evolution(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, 10 * PET_HAPPINESS_PERIOD_MS);   /* 20 s */

    section("Evolution over 20 s (-1 food / 1.2 s, -1 happiness / 2.0 s)");
    CHECK(p.food == 59, "food 75 -> 59");
    CHECK(p.happiness == 65, "happiness 75 -> 65");
    CHECK(p.age_ms == 10 * PET_HAPPINESS_PERIOD_MS, "age counts the elapsed time");
    CHECK(p.state == PET_ALIVE, "still alive");
}

static void test_call_frequency_independence(void)
{
    pet_t single, stepped;
    uint32_t t;

    pet_init(&single, 0);
    pet_tick(&single, 10 * PET_HAPPINESS_PERIOD_MS);

    pet_init(&stepped, 0);
    for (t = 500; t <= 10 * PET_HAPPINESS_PERIOD_MS; t += 500) {
        pet_tick(&stepped, t);
    }

    section("Ticking often gives the same result as ticking once");
    CHECK(stepped.food == single.food,
          "food same for 1 tick as for 40");
    CHECK(stepped.happiness == single.happiness,
          "happiness same for 1 tick as for 40");
    CHECK(stepped.food_accum_ms == single.food_accum_ms,
          "leftover time is the same in both cases");
}

static void test_clamping(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, 10 * PET_HAPPINESS_PERIOD_MS);        /* food 59, happiness 65 */

    section("Statistics stay inside [0, 100]");
    pet_feed(&p, 5, 10 * PET_HAPPINESS_PERIOD_MS);     /* +60 food, which would go past 100 */
    CHECK(p.food == PET_STAT_MAX, "over-feeding stops at 100");
    CHECK(p.happiness == 69, "feeding adds the happiness bonus");

    pet_play(&p, 21000);
    pet_clean(&p, 22000);
    CHECK(p.happiness <= PET_STAT_MAX, "happiness never goes over 100");

    { int i; for (i = 0; i < 20; i++) { pet_play(&p, 23000); } }
    CHECK(p.happiness == PET_STAT_MAX, "happiness stops at exactly 100");
}

static void test_feed_portion_clamping(void)
{
    pet_t p;

    section("Feed portion is clamped to [MIN, MAX]");
    pet_init(&p, 0);
    pet_tick(&p, 10 * PET_HAPPINESS_PERIOD_MS);            /* food 59 */
    pet_feed(&p, 0, 10 * PET_HAPPINESS_PERIOD_MS);
    CHECK(p.food == 59 + PET_FEED_FOOD_PER_PORTION,
          "portion 0 is treated as the minimum");

    pet_init(&p, 0);
    pet_tick(&p, 10 * PET_HAPPINESS_PERIOD_MS);            /* food 59 */
    pet_feed(&p, 99, 10 * PET_HAPPINESS_PERIOD_MS);
    CHECK(p.food == PET_STAT_MAX,
          "portion 99 is treated as the maximum and clamps at 100");
}

static void test_action_effects(void)
{
    pet_t p;

    section("Action effects");
    pet_init(&p, 0);
    pet_play(&p, 1000);
    CHECK(p.happiness == PET_INIT_HAPPINESS + PET_PLAY_HAPPINESS_GAIN, "play raises happiness");
    CHECK(p.food == PET_INIT_FOOD - PET_PLAY_FOOD_COST, "play costs food");
    CHECK(p.last_action == PET_ACTION_PLAY, "play is recorded as the last action");
    CHECK(p.action_count[PET_ACTION_PLAY] == 1, "play is counted");

    pet_init(&p, 0);
    pet_clean(&p, 1000);
    CHECK(p.happiness == PET_INIT_HAPPINESS + PET_CLEAN_HAPPINESS_GAIN, "clean raises happiness");
    CHECK(p.action_count[PET_ACTION_CLEAN] == 1, "clean is counted");
}

static void test_age_clock(void)
{
    pet_t p;
    pet_init(&p, 0);
    section("Age clock: 1 real minute is 1 game hour, 24 hours a day");
    CHECK(pet_age_day(&p) == 1 && pet_age_hour(&p) == 0, "a new pet is Day 1, 00h");

    pet_tick(&p, 59999);
    CHECK(pet_age_day(&p) == 1 && pet_age_hour(&p) == 0, "59.999 s is still hour 0");
    pet_tick(&p, 60000);
    CHECK(pet_age_day(&p) == 1 && pet_age_hour(&p) == 1, "60 s is hour 1");

    pet_init(&p, 0);
    p.age_ms = 23u * PET_MS_PER_GAME_HOUR + 59999u;
    CHECK(pet_age_day(&p) == 1 && pet_age_hour(&p) == 23, "23h 59.999 s is Day 1, 23h");
    p.age_ms = 24u * PET_MS_PER_GAME_HOUR;
    CHECK(pet_age_day(&p) == 2 && pet_age_hour(&p) == 0, "24 min rolls over to Day 2, 00h");
    p.age_ms = 3u * 24u * PET_MS_PER_GAME_HOUR + 7u * PET_MS_PER_GAME_HOUR;
    CHECK(pet_age_day(&p) == 4 && pet_age_hour(&p) == 7, "3 days 7 h is Day 4, 07h");

    pet_init(&p, 0);
    pet_tick(&p, PET_STARVE_MS);          /* starves */
    CHECK(p.state == PET_DEAD, "the pet starved");
    const uint32_t d = pet_age_day(&p);
    const uint8_t  h = pet_age_hour(&p);
    pet_tick(&p, (3u * PET_RUNAWAY_MS));
    CHECK(pet_age_day(&p) == d && pet_age_hour(&p) == h, "the clock freezes at death");
    pet_restart(&p, (3u * PET_RUNAWAY_MS));
    CHECK(pet_age_day(&p) == 1 && pet_age_hour(&p) == 0, "a restart goes back to Day 1");
    CHECK(pet_age_day(NULL) == 1 && pet_age_hour(NULL) == 0, "NULL is safe");
}

static void test_play_on_empty_stomach(void)
{
    pet_t p;
    int16_t happiness_before_play;
    pet_init(&p, 0);
    section("A hungry pet refuses play until fed");
    pet_tick(&p, 65 * PET_FOOD_PERIOD_MS);          /* 65 units of food gone: 75 -> 10 */
    happiness_before_play = p.happiness;
    CHECK(p.food == PET_PLAY_HUNGER_LIMIT, "food reaches the play refusal threshold");
    CHECK(!pet_play(&p, 65 * PET_FOOD_PERIOD_MS), "play is refused at food 10");
    CHECK(p.last_action == PET_ACTION_NONE, "a refused play is not recorded");
    CHECK(p.action_count[PET_ACTION_PLAY] == 0, "a refused play is not counted");
    CHECK(p.happiness == happiness_before_play, "a refused play has no effect");

    pet_tick(&p, 66 * PET_FOOD_PERIOD_MS);
    CHECK(p.food == PET_PLAY_HUNGER_LIMIT - 1, "food can fall below the refusal threshold");
    CHECK(!pet_play(&p, 66 * PET_FOOD_PERIOD_MS), "play remains refused while hungrier");

    CHECK(pet_feed(&p, 1, 66 * PET_FOOD_PERIOD_MS), "feeding is still accepted during the tantrum");
    CHECK(p.food > PET_PLAY_HUNGER_LIMIT, "feeding raises food above the threshold");
    CHECK(pet_play(&p, 66 * PET_FOOD_PERIOD_MS), "play is accepted again after feeding");
}

static void test_death_by_neglect(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, PET_STARVE_MS);         /* 75 units at the configured rate */

    section("Death from neglect");
    CHECK(p.food == PET_STAT_MIN, "food reaches 0 after 90 s");
    CHECK(p.state == PET_DEAD, "pet is dead");
    CHECK(!pet_is_interactive(&p), "a dead pet refuses interaction");
    CHECK(pet_mood(&p) == PET_MOOD_MISERABLE, "dead pet reports its worst mood");
}

static void test_runaway_by_feeding(void)
{
    pet_t p;
    uint32_t t;
    bool ran_away = false;

    pet_init(&p, 0);

    /* Feed it enough to keep food up, so happiness is what runs out first.
       That's how the runaway ending is reached on purpose. */
    for (t = 1000; t <= (3u * PET_RUNAWAY_MS); t += 1000) {
        pet_tick(&p, t);
        if (p.food <= 50) { pet_feed(&p, 5, t); }
        if (p.state == PET_RUNAWAY) { ran_away = true; break; }
        if (p.state == PET_DEAD)    { break; }
    }

    section("Runaway when happiness is neglected");
    printf("         reached at t = %u ms, food = %d, happiness = %d\n",
           t, p.food, p.happiness);
    CHECK(ran_away, "pet runs away instead of starving when it's kept fed");
    CHECK(p.state == PET_RUNAWAY, "pet is in the runaway state");
    CHECK(p.happiness == PET_STAT_MIN, "happiness reached 0");
    CHECK(p.food > PET_STAT_MIN, "pet did not starve");

    section("Frozen after the end state");
    {
        pet_t frozen = p;
        pet_tick(&p, t + 300000);
        CHECK(p.food == frozen.food && p.happiness == frozen.happiness,
              "statistics stop changing once the pet has run away");
        CHECK(pet_feed(&p, 5, t + 1000) == false, "feeding is refused");
        CHECK(pet_play(&p, t + 1000) == false, "playing is refused");
    }
}

static void test_restart(void)
{
    pet_t p;
    pet_init(&p, 0);
    pet_tick(&p, PET_STARVE_MS);
    CHECK(p.state == PET_DEAD, "pet is dead first");

    section("Restart");
    pet_restart(&p, 95000);
    CHECK(p.food == PET_INIT_FOOD, "food goes back to the start");
    CHECK(p.happiness == PET_INIT_HAPPINESS, "happiness goes back to the start");
    CHECK(p.state == PET_ALIVE, "pet is alive again");
    CHECK(pet_is_interactive(&p), "it accepts interaction again");
    CHECK(p.age_ms == 0, "lifetime resets");
    CHECK(pet_feed(&p, 3, 96000), "feeding works after the restart");
}

static void test_mood(void)
{
    pet_t p;

    section("Mood selection, which picks the idle sprite");
    pet_init(&p, 0);
    CHECK(pet_mood(&p) == PET_MOOD_DELIGHTED, "delighted at happiness 75");

    pet_tick(&p, 20 * PET_HAPPINESS_PERIOD_MS);   /* happiness 55, food 42 */
    CHECK(pet_mood(&p) == PET_MOOD_CONTENT, "content in the middle band");

    pet_tick(&p, 35 * PET_HAPPINESS_PERIOD_MS);   /* happiness 40, food 17 */
    CHECK(pet_mood(&p) == PET_MOOD_HUNGRY, "hungry once food drops to the threshold");

    { pet_t p2; pet_init(&p2, 0); pet_tick(&p2, 55 * PET_HAPPINESS_PERIOD_MS);
      CHECK(pet_mood(&p2) == PET_MOOD_MISERABLE, "miserable once happiness drops low"); }
}

int main(void)
{
    printf("pet_core tests\n");
    printf("rates: food -1/%u ms, happiness -1/%u ms, limits [%d, %d]\n",
           PET_FOOD_MS_PER_UNIT, PET_HAPPINESS_MS_PER_UNIT,
           PET_STAT_MIN, PET_STAT_MAX);

    test_init();
    test_time_evolution();
    test_call_frequency_independence();
    test_clamping();
    test_feed_portion_clamping();
    test_action_effects();
    test_play_on_empty_stomach();
    test_age_clock();
    test_death_by_neglect();
    test_runaway_by_feeding();
    test_restart();
    test_mood();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
