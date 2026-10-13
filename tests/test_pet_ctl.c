/**
 * @file    test_pet_ctl.c
 * @brief   Tests for action handling.
 *
 * This is the first place the model and the animation meet, so the checks here
 * are about them staying in step: an accepted action has to change the pet AND
 * play its animation, and a refused one has to do neither.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_ctl.c \
 *        Core/Src/pet_ctl.c Core/Src/pet_anim.c Core/Src/pet_core.c \
 *        -o /tmp/test_pet_ctl && /tmp/test_pet_ctl
 */
#include <stdio.h>

#include "pet_ctl.h"

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

static void test_init(void)
{
    pet_ctl_t c;
    pet_ctl_init(&c, 0);

    section("Starting up");
    CHECK(c.pet.state == PET_ALIVE, "the pet starts alive");
    CHECK(c.anim.id == PET_ANIM_IDLE, "the animation starts idle");
    CHECK(!pet_anim_finished(&c.anim), "an idle animation never reports finished");
}

static void test_tick_advances_both(void)
{
    pet_ctl_t c;
    pet_ctl_init(&c, 0);

    section("Ticking drives the pet and the animation together");
    pet_ctl_tick(&c, 20000);
    CHECK(c.pet.food == 59, "food evolved over 20 s");
    CHECK(c.pet.happiness == 65, "happiness evolved over 20 s");
    CHECK(c.pet.age_ms == 20000, "the age was recorded");

    /* Out of the 20 s, an idle animation at 250 ms per frame has been round
       several times and must still be idle. */
    CHECK(c.anim.id == PET_ANIM_IDLE, "a looping idle animation stays on idle");
}

static void test_each_action_changes_the_pet_and_the_animation(void)
{
    pet_ctl_t c;

    section("Feed");
    pet_ctl_init(&c, 0);
    CHECK(pet_ctl_action(&c, PET_ACTION_FEED, 2, 1000), "feeding is accepted");
    CHECK(c.pet.food == PET_INIT_FOOD + 2 * PET_FEED_FOOD_PER_PORTION,
          "food rises by the portion amount");
    CHECK(c.pet.happiness == PET_INIT_HAPPINESS + PET_FEED_HAPPINESS_BONUS,
          "feeding is slightly cheering");
    CHECK(c.anim.id == PET_ANIM_FEED, "the feeding animation starts");

    section("Pet");
    pet_ctl_init(&c, 0);
    CHECK(pet_ctl_action(&c, PET_ACTION_PET, 1, 1000), "playing is accepted");
    CHECK(c.pet.happiness == PET_INIT_HAPPINESS + PET_PET_HAPPINESS_GAIN,
          "happiness rises");
    CHECK(c.pet.food == PET_INIT_FOOD - PET_PET_FOOD_COST,
          "playing works up an appetite");
    CHECK(c.anim.id == PET_ANIM_PET, "the playing animation starts");

    section("Clean");
    pet_ctl_init(&c, 0);
    CHECK(pet_ctl_action(&c, PET_ACTION_CLEAN, 1, 1000), "cleaning is accepted");
    CHECK(c.pet.happiness == PET_INIT_HAPPINESS + PET_CLEAN_HAPPINESS_GAIN,
          "happiness rises");
    CHECK(c.anim.id == PET_ANIM_CLEAN, "the cleaning animation starts");
}

static void test_portion_is_passed_through_and_clamped(void)
{
    pet_ctl_t c;

    section("The analog input's portion reaches pet_feed and is clamped");
    pet_ctl_init(&c, 0);
    pet_ctl_action(&c, PET_ACTION_FEED, 0, 1000);
    CHECK(c.pet.food == PET_INIT_FOOD + PET_FEED_FOOD_PER_PORTION,
          "a portion of 0 is treated as the minimum");

    pet_ctl_init(&c, 0);
    pet_ctl_action(&c, PET_ACTION_FEED, 200, 1000);
    CHECK(c.pet.food == PET_STAT_MAX, "a wild portion cannot push food past 100");
}

static void test_refused_actions_do_nothing(void)
{
    pet_ctl_t c;

    section("A finished pet refuses everything, and nothing animates");
    pet_ctl_init(&c, 0);
    pet_ctl_tick(&c, 90000);                 /* starves */
    CHECK(c.pet.state == PET_DEAD, "the pet has died");
    CHECK(!pet_is_interactive(&c.pet), "and refuses interaction");

    CHECK(!pet_ctl_action(&c, PET_ACTION_FEED, 3, 90000), "feeding is refused");
    CHECK(!pet_ctl_action(&c, PET_ACTION_PET, 3, 90000), "playing is refused");
    CHECK(!pet_ctl_action(&c, PET_ACTION_CLEAN, 3, 90000), "cleaning is refused");
    CHECK(c.pet.food == PET_STAT_MIN, "the refused actions left the pet unchanged");
    CHECK(c.anim.id == PET_ANIM_IDLE,
          "no action animation was started for the refused actions");

    section("An unknown action is refused too");
    CHECK(!pet_ctl_action(&c, PET_ACTION_NONE, 1, 90000), "PET_ACTION_NONE is not an action");
}

static void test_animation_returns_to_idle(void)
{
    pet_ctl_t c;

    section("An action animation plays out and hands back to idle");
    pet_ctl_init(&c, 0);
    pet_ctl_action(&c, PET_ACTION_PET, 1, 0);
    CHECK(c.anim.id == PET_ANIM_PET, "the playing animation is running");

    pet_ctl_tick(&c, 600);                   /* PET is 5 x 100 ms */
    CHECK(c.anim.id == PET_ANIM_IDLE,
          "once it finishes, the controller puts the animation back to idle");

    section("And the pet is still fine after the handback");
    CHECK(pet_anim_select(&c.pet, &c.anim) == PET_ANIM_IDLE, "the view shows idle");
    CHECK(c.pet.state == PET_ALIVE, "the pet is still alive");
}

static void test_restart(void)
{
    pet_ctl_t c;

    section("Restart");
    pet_ctl_init(&c, 0);
    pet_ctl_tick(&c, 90000);
    CHECK(c.pet.state == PET_DEAD, "dead first");

    pet_ctl_restart(&c, 95000);
    CHECK(c.pet.state == PET_ALIVE, "the pet is alive again");
    CHECK(c.pet.food == PET_INIT_FOOD, "food is back to the start");
    CHECK(c.anim.id == PET_ANIM_RESTART, "the re-forming animation plays");
    CHECK(pet_ctl_action(&c, PET_ACTION_PET, 1, 96000), "actions work again");
}

static void test_null_is_safe(void)
{
    section("A NULL controller is refused rather than dereferenced");
    pet_ctl_init(NULL, 0);
    pet_ctl_tick(NULL, 0);
    CHECK(!pet_ctl_action(NULL, PET_ACTION_FEED, 1, 0), "an action on NULL returns false");
    pet_ctl_restart(NULL, 0);
    pet_ctl_wake(NULL, 0);
    CHECK(1, "none of the NULL calls crashed");
}

static void test_sleep(void)
{
    pet_ctl_t c;

    section("Sleep action");
    pet_ctl_init(&c, 0);
    CHECK(pet_ctl_action(&c, PET_ACTION_SLEEP, 1, 1000), "sleep is accepted");
    CHECK(pet_is_sleeping(&c.pet), "the pet is sleeping");
    CHECK(c.anim.id == PET_ANIM_IDLE, "sleep stays on idle");

    section("Petting while asleep wakes and applies");
    CHECK(pet_ctl_action(&c, PET_ACTION_PET, 1, 2000), "play is accepted while asleep");
    CHECK(!pet_is_sleeping(&c.pet), "play woke the pet");
    CHECK(c.anim.id == PET_ANIM_PET, "the play animation started");

    section("Wake without a care action");
    pet_ctl_init(&c, 0);
    CHECK(pet_ctl_action(&c, PET_ACTION_SLEEP, 1, 0), "asleep again");
    pet_ctl_wake(&c, 1000);
    CHECK(!pet_is_sleeping(&c.pet), "wake cleared sleeping");
    CHECK(c.anim.id == PET_ANIM_IDLE, "wake returns to idle");
}

int main(void)
{
    printf("pet_ctl tests\n");

    test_init();
    test_tick_advances_both();
    test_each_action_changes_the_pet_and_the_animation();
    test_portion_is_passed_through_and_clamped();
    test_refused_actions_do_nothing();
    test_animation_returns_to_idle();
    test_restart();
    test_null_is_safe();
    test_sleep();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
