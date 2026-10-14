/**
 * @file    test_ui_text.c
 * @brief   Every string the UI draws must be drawable in the 5x7 font, and fit.
 *
 * The font used to stop at 'Z', so every lowercase letter came out blank and
 * the menu read "F", "P", "C". This checks the real strings against the real
 * font table, so a new label with an unsupported character fails here, not on
 * the board.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc -I Drivers/BSP/GFX01M2 \
 *        tests/test_ui_text.c Drivers/BSP/GFX01M2/gfx_font5x7.c \
 *        Core/Src/pet_ui.c Core/Src/pet_core.c -o /tmp/test_ui_text && /tmp/test_ui_text
 */
#include <stdio.h>
#include <string.h>

#include "gfx_font5x7.h"
#include "pet_ui.h"
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
            printf("  [FAIL] %s   (%s:%d)\n", (msg), __FILE__, __LINE__); \
        }                                                                 \
    } while (0)

static void section(const char *name) { printf("\n%s\n", name); }

/* True if c has a glyph and, unless it is a space, the glyph is not blank. */
static int drawable(char c)
{
    const unsigned char u = (unsigned char)c;
    if ((u < GFX_FONT5X7_FIRST_CHAR) || (u > GFX_FONT5X7_LAST_CHAR)) { return 0; }
    if (c == ' ') { return 1; }
    const uint8_t *g = GFX_Font5x7[u - GFX_FONT5X7_FIRST_CHAR];
    return (g[0] | g[1] | g[2] | g[3] | g[4]) != 0u;
}

static void check_string(const char *s, uint16_t x)
{
    char msg[96];
    int ok = 1;
    for (const char *p = s; *p != '\0'; p++) { if (!drawable(*p)) { ok = 0; } }
    snprintf(msg, sizeof msg, "\"%s\" has a visible glyph for every character", s);
    CHECK(ok, msg);

    const uint32_t width = (uint32_t)strlen(s) * (GFX_FONT5X7_WIDTH + 1u) * PET_HUD_TEXT_SCALE;
    snprintf(msg, sizeof msg, "\"%s\" fits on screen (%u px from x=%u)", s,
             (unsigned)width, (unsigned)x);
    CHECK(x + width <= PET_HUD_SCREEN_W, msg);
}

static void test_menu_labels(void)
{
    section("Menu labels");
    for (uint8_t i = 0u; i < pet_ui_item_count(); i++) {
        check_string(pet_ui_item_label(i), PET_HUD_MENU_X + 12u);
    }
    check_string("x1", PET_HUD_MENU_X + PET_HUD_MENU_W - 36u);
    check_string("x5", PET_HUD_MENU_X + PET_HUD_MENU_W - 36u);
}

static void test_ending_text(void)
{
    section("Ending screen");
    check_string(PET_HUD_TEXT_STARVED,  PET_HUD_MENU_X);
    check_string(PET_HUD_TEXT_RAN_AWAY, PET_HUD_MENU_X);
    check_string(PET_HUD_TEXT_RESTART1, PET_HUD_MENU_X);
    check_string(PET_HUD_TEXT_RESTART2, PET_HUD_MENU_X);
}

static void test_name_and_age(void)
{
    pet_t p;
    char  buf[PET_HUD_AGE_TEXT_MAX];

    section("Name and age line");
    check_string(PET_HUD_NAME, pet_hud_centered_x((uint16_t)strlen(PET_HUD_NAME)));
    pet_init(&p, 0);
    p.age_ms = 0xFFFFFFFFu;                      /* the longest text it can ever show */
    pet_hud_age_text(buf, sizeof buf, &p);
    check_string(buf, pet_hud_centered_x((uint16_t)strlen(buf)));
}

static void test_table_alignment(void)
{
    section("The table is not shifted by a missing or extra row");
    const uint8_t *a = GFX_Font5x7['a' - GFX_FONT5X7_FIRST_CHAR];
    const uint8_t *z = GFX_Font5x7['z' - GFX_FONT5X7_FIRST_CHAR];
    const uint8_t *b = GFX_Font5x7['[' - GFX_FONT5X7_FIRST_CHAR];
    CHECK(a[0] == 0x20u && a[4] == 0x78u, "'a' is the classic 5x7 'a'");
    CHECK(z[0] == 0x44u && z[2] == 0x54u, "'z' is the classic 5x7 'z'");
    CHECK(b[1] == 0x7Fu, "'[' sits right after 'Z'");
    CHECK(GFX_FONT5X7_LAST_CHAR == 'z', "the font now ends at 'z'");
}

int main(void)
{
    printf("ui text tests\n");
    test_menu_labels();
    test_ending_text();
    test_name_and_age();
    test_table_alignment();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
