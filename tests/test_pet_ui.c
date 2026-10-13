/**
 * @file    test_pet_ui.c
 * @brief   Tests for the screen and menu state machine.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_ui.c \
 *        Core/Src/pet_ui.c Core/Src/pet_core.c -o /tmp/test_pet_ui \
 *        && /tmp/test_pet_ui
 */
#include <stdio.h>
#include <string.h>

#include "pet_ui.h"

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

/* Starve the pet without waiting around in real time. */
static void kill_pet(pet_t *p) { pet_init(p, 0); pet_tick(p, 200000); }

static void test_initial_state(void)
{
    pet_ui_t ui;

    section("Starting state");
    pet_ui_init(&ui);
    CHECK(pet_ui_screen(&ui) == PET_SCREEN_PET, "starts on the pet screen");
    CHECK(pet_ui_cursor(&ui) == 0, "cursor starts on the first row");
    CHECK(pet_ui_portion(&ui) == PET_FEED_PORTION_MIN, "portion starts at the minimum");
}

static void test_opening_the_menu(void)
{
    pet_ui_t ui;
    pet_t p;

    section("Opening the menu");
    pet_ui_init(&ui);
    pet_init(&p, 0);

    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_UP, &p);
      CHECK(r.action == PET_ACTION_NONE && !r.restart && !r.redraw,
            "an input that isn't used here does nothing"); }

    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_NONE, &p);
      CHECK(!r.redraw && r.action == PET_ACTION_NONE,
            "a pass with no input gives no action and no redraw"); }

    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
      CHECK(pet_ui_screen(&ui) == PET_SCREEN_MENU, "select opens the menu");
      CHECK(r.redraw, "opening the menu wants a redraw");
      CHECK(r.action == PET_ACTION_NONE, "opening the menu doesn't do an action"); }
}

static void test_navigation_wraps(void)
{
    pet_ui_t ui;
    pet_t p;
    uint8_t n;

    section("Moving the cursor wraps round at both ends");
    pet_ui_init(&ui);
    pet_init(&p, 0);
    n = pet_ui_item_count();
    (void)pet_ui_handle(&ui, PET_INPUT_SELECT, &p);

    {
        int i;
        for (i = 0; i < n; i++) {
            pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
            CHECK(r.redraw, "moving the cursor wants a redraw");
            CHECK(pet_ui_cursor(&ui) == (uint8_t)((i + 1) % n),
                  "down moves on and wraps back to the top");
        }
    }
    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_UP, &p);
      CHECK(pet_ui_cursor(&ui) == (uint8_t)(n - 1u),
            "up from the first row wraps to the last");
      CHECK(r.redraw, "wrapping up wants a redraw"); }
}

static void test_every_row_commits_its_action(void)
{
    pet_ui_t ui;
    pet_t p;
    uint8_t i;

    section("Every row does its own action and goes back to the pet");
    for (i = 0; i < pet_ui_item_count(); i++) {
        pet_ui_result_t r;
        pet_init(&p, 0);
        pet_ui_init(&ui);
        (void)pet_ui_handle(&ui, PET_INPUT_SELECT, &p);   /* open */
        {
            uint8_t k;
            for (k = 0; k < i; k++) { (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p); }
        }
        r = pet_ui_handle(&ui, PET_INPUT_SELECT, &p);

        printf("         row %u: %-6s -> %s\n", i,
               pet_ui_item_label(i, &p), pet_action_name(r.action));
        CHECK(r.action == pet_ui_item_action(i, &p), "the action matches the row");
        CHECK(r.action != PET_ACTION_NONE, "the row does a real action");
        CHECK(!r.wake, "awake rows are not wake-only");
        CHECK(pet_ui_screen(&ui) == PET_SCREEN_PET, "it goes back to the pet screen");
        CHECK(!r.restart, "confirming isn't a restart");
    }

    section("The menu has the three required interactions plus Sleep");
    {
        pet_t awake;
        pet_init(&awake, 0);
        CHECK(pet_ui_item_count() == 4, "four actions on the menu");
        CHECK(pet_ui_item_action(0, &awake) == PET_ACTION_FEED,  "Feed is on it");
        CHECK(pet_ui_item_action(1, &awake) == PET_ACTION_PET,  "Pet is on the menu");
        CHECK(pet_ui_item_action(2, &awake) == PET_ACTION_CLEAN, "Clean is on it");
        CHECK(pet_ui_item_action(3, &awake) == PET_ACTION_SLEEP, "Sleep is on it");
    }
}

static void test_cancel(void)
{
    pet_ui_t ui;
    pet_t p;

    section("Back cancels without doing anything");
    pet_ui_init(&ui);
    pet_init(&p, 0);
    (void)pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
    (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p);

    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_BACK, &p);
      CHECK(pet_ui_screen(&ui) == PET_SCREEN_PET, "back goes to the pet screen");
      CHECK(r.action == PET_ACTION_NONE, "back does no action");
      CHECK(!r.restart, "back isn't a restart");
      CHECK(r.redraw, "back wants a redraw"); }
}

static void test_terminal_pet_locks_the_menu(void)
{
    pet_ui_t ui;
    pet_t p;

    section("A finished pet can't open the menu and offers a restart");
    pet_ui_init(&ui);
    kill_pet(&p);
    CHECK(!pet_is_interactive(&p), "the pet is really finished");

    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
      CHECK(pet_ui_screen(&ui) == PET_SCREEN_PET, "select doesn't open the menu");
      CHECK(r.restart, "select on a finished pet asks for a restart");
      CHECK(r.action == PET_ACTION_NONE, "a restart isn't a menu action"); }

    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
      CHECK(!r.restart && r.action == PET_ACTION_NONE,
            "moving the cursor can't restart the pet"); }

    /* Same again for a pet that ran away instead of starving. */
    { pet_t rp; uint32_t t; bool ran = false;
      pet_init(&rp, 0);
      for (t = 1000; t <= 900000; t += 1000) {
          pet_tick(&rp, t);
          if (rp.food <= 50) { pet_feed(&rp, 5, t); }
          if (rp.state == PET_RUNAWAY) { ran = true; break; }
          if (rp.state == PET_DEAD) { break; }
      }
      CHECK(ran, "pet reached the runaway ending");
      pet_ui_init(&ui);
      { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_SELECT, &rp);
        CHECK(r.restart, "a runaway pet offers a restart too");
        CHECK(pet_ui_screen(&ui) == PET_SCREEN_PET, "and still doesn't open the menu"); } }
}

static void test_death_closes_an_open_menu(void)
{
    pet_ui_t ui;
    pet_t p;

    section("If the pet dies with the menu open, the menu closes");
    pet_ui_init(&ui);
    pet_init(&p, 0);
    (void)pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
    CHECK(pet_ui_screen(&ui) == PET_SCREEN_MENU, "the menu is open");

    pet_tick(&p, 200000);                 /* starves while the menu is up */
    CHECK(!pet_is_interactive(&p), "the pet died in the meantime");

    { pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
      CHECK(pet_ui_screen(&ui) == PET_SCREEN_PET,
            "the menu closes so the ending isn't hidden behind it");
      CHECK(r.action == PET_ACTION_NONE, "closing didn't run the highlighted row");
      CHECK(r.redraw, "closing wants a redraw"); }
}

static void test_portion(void)
{
    pet_ui_t ui;

    section("The analog input's portion is stored and clamped");
    pet_ui_init(&ui);
    pet_ui_set_portion(&ui, 3);
    CHECK(pet_ui_portion(&ui) == 3, "a normal portion is stored");

    pet_ui_set_portion(&ui, 0);
    CHECK(pet_ui_portion(&ui) == PET_FEED_PORTION_MIN, "too small clamps up to the minimum");
    pet_ui_set_portion(&ui, 200);
    CHECK(pet_ui_portion(&ui) == PET_FEED_PORTION_MAX, "too big clamps down to the maximum");
}

static void test_labels_are_usable(void)
{
    uint8_t i;
    pet_t p;

    section("Menu labels are there and all different");
    pet_init(&p, 0);
    for (i = 0; i < pet_ui_item_count(); i++) {
        const char *a = pet_ui_item_label(i, &p);
        uint8_t k;
        CHECK(a != NULL && a[0] != '\0', "the row has a label");
        for (k = 0; k < i; k++) {
            const char *b = pet_ui_item_label(k, &p);
            CHECK(strcmp(a, b) != 0, "the labels aren't the same");
        }
    }
    CHECK(pet_ui_item_label(200, &p) != NULL, "a row number out of range clamps, doesn't return NULL");
    CHECK(pet_ui_item_action(200, &p) == PET_ACTION_NONE, "a row number out of range has no action");
}

static void test_sleep_menu(void)
{
    pet_ui_t ui;
    pet_t p;

    section("Sleep from the menu");
    pet_ui_init(&ui);
    pet_init(&p, 0);
    (void)pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
    (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
    (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
    (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
    {
        pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
        CHECK(r.action == PET_ACTION_SLEEP, "the Sleep row commits SLEEP");
        CHECK(!r.wake, "Sleep is not wake-only while awake");
    }

    section("While asleep SELECT still opens the menu");
    CHECK(pet_sleep(&p, 1000), "pet is sleeping");
    pet_ui_init(&ui);
    {
        pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
        CHECK(pet_ui_screen(&ui) == PET_SCREEN_MENU, "select opens the menu while asleep");
        CHECK(r.action == PET_ACTION_NONE && !r.wake && !r.restart,
              "opening the menu does not wake or act");
        CHECK(strcmp(pet_ui_item_label(3, &p), "Wake") == 0, "the Sleep row reads Wake");
    }

    section("Wake row wakes without a care action");
    (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
    (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
    (void)pet_ui_handle(&ui, PET_INPUT_DOWN, &p);
    {
        pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
        CHECK(r.wake, "Wake sets the wake flag");
        CHECK(r.action == PET_ACTION_NONE, "Wake is not a care action");
        CHECK(pet_ui_screen(&ui) == PET_SCREEN_PET, "back on the pet screen");
    }

    section("Feed from the asleep menu commits FEED");
    /* Still sleeping: the Wake result was not applied to the model here. */
    CHECK(pet_is_sleeping(&p), "still asleep for the Feed check");
    pet_ui_init(&ui);
    (void)pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
    {
        pet_ui_result_t r = pet_ui_handle(&ui, PET_INPUT_SELECT, &p);
        CHECK(r.action == PET_ACTION_FEED, "Feed is still committed while asleep");
        CHECK(!r.wake, "Feed is not the wake-only path");
    }
}

int main(void)
{
    printf("pet_ui tests\n");

    test_initial_state();
    test_opening_the_menu();
    test_navigation_wraps();
    test_every_row_commits_its_action();
    test_cancel();
    test_terminal_pet_locks_the_menu();
    test_death_closes_an_open_menu();
    test_portion();
    test_labels_are_usable();
    test_sleep_menu();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
