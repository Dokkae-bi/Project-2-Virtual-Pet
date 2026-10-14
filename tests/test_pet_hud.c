/**
 * @file    test_pet_hud.c
 * @brief   Tests for the HUD layout and meter maths.
 *
 * The geometry checks matter more than they look. A row that overlaps another,
 * or a menu that runs off the bottom of the panel, is invisible until the board
 * is in your hands - and then it's expensive to find.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_pet_hud.c \
 *        Core/Src/pet_hud.c Core/Src/pet_ui.c Core/Src/pet_core.c \
 *        -o /tmp/test_pet_hud && /tmp/test_pet_hud
 */
#include <stdio.h>

#include "pet_hud.h"

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

static void test_fill_maths(void)
{
    section("Meter fill");
    CHECK(pet_hud_meter_fill_px(0, PET_HUD_TRACK_W) == 0, "0 is an empty bar");
    CHECK(pet_hud_meter_fill_px(100, PET_HUD_TRACK_W) == PET_HUD_TRACK_W,
          "100 is a completely full bar");
    CHECK(pet_hud_meter_fill_px(50, PET_HUD_TRACK_W) == PET_HUD_TRACK_W / 2,
          "50 is exactly half");
    CHECK(pet_hud_meter_fill_px(25, PET_HUD_TRACK_W) == PET_HUD_TRACK_W / 4,
          "25 is a quarter");
    CHECK(pet_hud_meter_fill_px(1, PET_HUD_TRACK_W) >= 1,
          "a value of 1 still draws at least one pixel");

    section("Meter fill clamps instead of overflowing the bar");
    CHECK(pet_hud_meter_fill_px(-50, PET_HUD_TRACK_W) == 0, "a negative value gives an empty bar");
    CHECK(pet_hud_meter_fill_px(999, PET_HUD_TRACK_W) == PET_HUD_TRACK_W,
          "a value over 100 cannot draw past the end");
    CHECK(pet_hud_meter_fill_px(50, 0) == 0, "a zero-width track draws nothing");
}

static void test_colour_bands(void)
{
    section("Food runs the same way as happiness: low is bad");
    CHECK(pet_hud_meter_color(100) == PET_HUD_COL_GOOD, "food 100 is healthy");
    CHECK(pet_hud_meter_color(70)  == PET_HUD_COL_GOOD, "food 70 is still healthy");
    CHECK(pet_hud_meter_color(69)  == PET_HUD_COL_WARN, "food 69 turns marginal");
    CHECK(pet_hud_meter_color(30)  == PET_HUD_COL_WARN, "food 30 is still marginal");
    CHECK(pet_hud_meter_color(29)  == PET_HUD_COL_BAD,  "food 29 is critical");
    CHECK(pet_hud_meter_color(0)   == PET_HUD_COL_BAD,  "food 0 is critical");
    CHECK(pet_hud_meter_fill_px(0, PET_HUD_TRACK_W) == 0u, "an empty stomach is an empty bar");

    section("Happiness is bad when low");
    CHECK(pet_hud_meter_color(100) == PET_HUD_COL_GOOD, "happiness 100 is healthy");
    CHECK(pet_hud_meter_color(70) == PET_HUD_COL_GOOD,  "happiness 70 is healthy");
    CHECK(pet_hud_meter_color(69) == PET_HUD_COL_WARN,  "happiness 69 turns marginal");
    CHECK(pet_hud_meter_color(30) == PET_HUD_COL_WARN,  "happiness 30 is still marginal");
    CHECK(pet_hud_meter_color(29) == PET_HUD_COL_BAD,   "happiness 29 is critical");
    CHECK(pet_hud_meter_color(0) == PET_HUD_COL_BAD,    "happiness 0 is critical");

    section("The three bands are actually different colours");
    CHECK(PET_HUD_COL_GOOD != PET_HUD_COL_WARN, "good and marginal differ");
    CHECK(PET_HUD_COL_WARN != PET_HUD_COL_BAD,  "marginal and critical differ");
    CHECK(PET_HUD_COL_GOOD != PET_HUD_COL_BAD,  "good and critical differ");
    CHECK(PET_HUD_COL_TRACK != PET_HUD_COL_PANEL, "the track is visible against the panel");
}

static void test_menu_rows(void)
{
    uint8_t i;

    section("Menu rows are evenly spaced");
    for (i = 0; i < pet_ui_item_count(); i++) {
        CHECK(pet_hud_menu_row_y(i) == (uint16_t)(PET_HUD_MENU_Y + i * PET_HUD_MENU_ROW_H),
              "row top is a multiple of the row height below the menu origin");
    }

    section("An out of range menu row wraps rather than going off screen");
    CHECK(pet_hud_menu_row_y(pet_ui_item_count()) == PET_HUD_MENU_Y,
          "one past the last row wraps to the first");
    CHECK(pet_hud_menu_row_y(200) < PET_HUD_SCREEN_H,
          "a wild row index still lands on the screen");
}

static void test_layout_fits(void)
{
    const uint16_t food_bot = (uint16_t)(PET_HUD_FOOD_ROW_Y + PET_HUD_ICON_SIZE);
    const uint16_t happy_top  = PET_HUD_HAPPINESS_ROW_Y;
    const uint16_t happy_bot  = (uint16_t)(PET_HUD_HAPPINESS_ROW_Y + PET_HUD_ICON_SIZE);
    const uint16_t menu_bot   = (uint16_t)(PET_HUD_MENU_Y +
                                           pet_ui_item_count() * PET_HUD_MENU_ROW_H);

    section("Nothing overlaps and nothing runs off the panel");
    CHECK(happy_top >= food_bot, "the happiness row starts below the food row");
    CHECK(PET_HUD_MENU_Y >= happy_bot, "the menu starts below the meters");
    CHECK(menu_bot <= PET_HUD_SCREEN_H, "the last menu row fits on the panel");
    CHECK(PET_HUD_TRACK_X + PET_HUD_TRACK_W <= PET_HUD_SCREEN_W,
          "the bar track fits across the width");
    CHECK(PET_HUD_TRACK_DY + PET_HUD_TRACK_H <= PET_HUD_ICON_SIZE,
          "the bar track fits within its row height");
    CHECK(PET_HUD_ICON_X + PET_HUD_ICON_SIZE <= PET_HUD_TRACK_X,
          "the icon does not overlap the bar track");
    CHECK(PET_HUD_MENU_X + PET_HUD_MENU_W <= PET_HUD_SCREEN_W,
          "the menu panel fits across the width");

    section("The slime sits centred in the stage, clear of the meters");
    CHECK(PET_HUD_SPRITE_X == (PET_HUD_SCREEN_W - PET_HUD_SPRITE_W) / 2,
          "the sprite is horizontally centred");
    CHECK(PET_HUD_SPRITE_Y + PET_HUD_SPRITE_H <= PET_HUD_FOOD_ROW_Y,
          "the sprite does not reach down into the food row");
}

static void test_flash_cost(void)
{
    const uint32_t icon_bytes = PET_HUD_ICON_SIZE * PET_HUD_ICON_SIZE * 2u;

    section("Icon cost");
    printf("         %u bytes per icon, %u bytes for the pair\n",
           (unsigned)icon_bytes, (unsigned)(icon_bytes * 2u));
    CHECK(icon_bytes * 2u < 4096u, "both HUD icons cost under 4 KB of flash");
}

int main(void)
{
    printf("pet_hud tests\n");

    test_fill_maths();
    test_colour_bands();
    test_menu_rows();
    test_layout_fits();
    test_flash_cost();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
