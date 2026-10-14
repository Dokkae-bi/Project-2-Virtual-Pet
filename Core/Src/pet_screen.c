/**
 * @file    pet_screen.c
 * @brief   Drawing the pet screen. See pet_screen.h.
 */
#include "pet_screen.h"

#include <stdio.h>
#include <string.h>

#include "ili9341.h"
#include "pet_hud.h"
#include "pet_view.h"

#include "icon_hunger.h"
#include "icon_happy.h"

/* Composed pet frame. 64x64 RGB565 is 8 KB, so it has to be static - the stack
   is only 2 KB. */
static uint16_t g_frame[PET_HUD_SPRITE_W * PET_HUD_SPRITE_H];

/* What the panel is currently showing, so we only push changes. */
/* Where the slime is, and where it was last drawn. */
static int16_t g_pet_x = (int16_t)PET_HUD_SPRITE_X;

static struct {
    int16_t       pet_x;
    uint32_t      day;
    uint8_t       hour;
    int16_t       food;
    int16_t       happiness;
    uint8_t       cursor;
    uint8_t       portion;
    pet_screen_t  screen;
    pet_anim_id_t anim;
    uint8_t       frame;
    pet_mood_t    mood;
    pet_state_t   state;
    bool          valid;
} g_drawn;

static void draw_meter(uint16_t row_y, const uint16_t *icon, int16_t value)
{
    char      text[8];
    const uint16_t bar_y = (uint16_t)(row_y + PET_HUD_TRACK_DY);
    const uint16_t fill  = pet_hud_meter_fill_px(value, PET_HUD_TRACK_W);

    ILI9341_DrawBitmap(PET_HUD_ICON_X, row_y, PET_HUD_ICON_SIZE, PET_HUD_ICON_SIZE,
                       icon);

    /* Empty track first, then the filled part over it. The bar is at most
       PET_HUD_TRACK_W wide, so this touches 160x12 px rather than the screen. */
    ILI9341_FillRect(PET_HUD_TRACK_X, bar_y, PET_HUD_TRACK_W, PET_HUD_TRACK_H,
                     PET_HUD_COL_TRACK);
    if (fill > 0u) {
        ILI9341_FillRect(PET_HUD_TRACK_X, bar_y, fill, PET_HUD_TRACK_H,
                         pet_hud_meter_color(value));
    }

    (void)snprintf(text, sizeof(text), "%d", (int)value);
    /* Clear the old number first: "99" drawn over "100" would leave the last 0
       behind and read as "990". */
    ILI9341_FillRect(PET_HUD_READOUT_X, (uint16_t)(row_y + 4u), PET_HUD_READOUT_W,
                     14u, PET_HUD_COL_PANEL);
    ILI9341_DrawString(PET_HUD_READOUT_X, (uint16_t)(row_y + 4u), text,
                       PET_HUD_COL_TEXT, PET_HUD_COL_PANEL, PET_HUD_TEXT_SCALE);
}

/* Clears the lower block, then draws the menu or leaves it blank. */
static void draw_lower_block(const pet_ui_t *ui)
{
    const uint16_t height = (uint16_t)(PET_HUD_SCREEN_H - PET_HUD_MENU_Y);

    ILI9341_FillRect(0, PET_HUD_MENU_Y, PET_HUD_SCREEN_W, height, PET_HUD_COL_PANEL);

    if (pet_ui_screen(ui) != PET_SCREEN_MENU) {
        return;
    }

    for (uint8_t i = 0u; i < pet_ui_item_count(); i++) {
        const uint16_t row_y = pet_hud_menu_row_y(i);
        const bool     sel   = (i == pet_ui_cursor(ui));
        const uint16_t bg    = sel ? PET_HUD_COL_SELECTED : PET_HUD_COL_PANEL;

        ILI9341_FillRect(PET_HUD_MENU_X, row_y, PET_HUD_MENU_W, PET_HUD_MENU_ROW_H, bg);
        ILI9341_DrawString((uint16_t)(PET_HUD_MENU_X + 12u), (uint16_t)(row_y + 5u),
                           pet_ui_item_label(i), PET_HUD_COL_TEXT, bg, PET_HUD_TEXT_SCALE);

        /* The Feed row also shows how much the analog input currently selects,
           so what the player reads is what the pet will actually get. */
        if (pet_ui_item_action(i) == PET_ACTION_FEED) {
            /* Sized for any unsigned, so the compiler doesn't warn about a
               possible truncation. The value is 1-5 in practice. */
            char amount[16];
            (void)snprintf(amount, sizeof(amount), "x%u", (unsigned)pet_ui_portion(ui));
            ILI9341_DrawString((uint16_t)(PET_HUD_MENU_X + PET_HUD_MENU_W - 36u),
                               (uint16_t)(row_y + 5u), amount,
                               PET_HUD_COL_TEXT, bg, PET_HUD_TEXT_SCALE);
        }
    }
}

/* Shown in the lower block once the pet has starved or left. */
static void draw_ending(const pet_t *pet)
{
    const char *line;

    if (pet_is_interactive(pet)) {
        return;
    }

    line = (pet->state == PET_DEAD) ? PET_HUD_TEXT_STARVED : PET_HUD_TEXT_RAN_AWAY;

    ILI9341_DrawString(PET_HUD_MENU_X, (uint16_t)(PET_HUD_MENU_Y + 8u), line,
                       PET_HUD_COL_TEXT, PET_HUD_COL_PANEL, PET_HUD_TEXT_SCALE);
    ILI9341_DrawString(PET_HUD_MENU_X, (uint16_t)(PET_HUD_MENU_Y + 34u), PET_HUD_TEXT_RESTART1,
                       PET_HUD_COL_TEXT, PET_HUD_COL_PANEL, PET_HUD_TEXT_SCALE);
    ILI9341_DrawString(PET_HUD_MENU_X, (uint16_t)(PET_HUD_MENU_Y + 54u), PET_HUD_TEXT_RESTART2,
                       PET_HUD_COL_TEXT, PET_HUD_COL_PANEL, PET_HUD_TEXT_SCALE);
}

/* Name once, then the age line whenever the game hour ticks over. The line is
   cleared first because "Day 9  23h" and "Day 10  00h" differ in width. */
static void draw_age(const pet_t *pet)
{
    char text[PET_HUD_AGE_TEXT_MAX];

    pet_hud_age_text(text, sizeof(text), pet);
    ILI9341_FillRect(0, PET_HUD_DAY_Y, PET_HUD_SCREEN_W, 14u, PET_HUD_COL_PANEL);
    ILI9341_DrawString(pet_hud_centered_x((uint16_t)strlen(text)), PET_HUD_DAY_Y, text,
                       PET_HUD_COL_TEXT, PET_HUD_COL_PANEL, PET_HUD_TEXT_SCALE);
}

/* Blank the strip the sprite has just slid off, so it leaves no trail. The new
   blit covers the rest of the old position. */
static void erase_trail(int16_t from_x, int16_t to_x)
{
    if (to_x > from_x) {
        const uint16_t w = (uint16_t)(to_x - from_x);
        ILI9341_FillRect((uint16_t)from_x, PET_HUD_SPRITE_Y, w, PET_HUD_SPRITE_H,
                         PET_HUD_COL_PANEL);
    } else if (to_x < from_x) {
        const uint16_t w = (uint16_t)(from_x - to_x);
        ILI9341_FillRect((uint16_t)(to_x + (int16_t)PET_HUD_SPRITE_W), PET_HUD_SPRITE_Y,
                         w, PET_HUD_SPRITE_H, PET_HUD_COL_PANEL);
    }
}

static void draw_pet(const pet_t *pet, const pet_anim_t *anim)
{
    pet_view_compose_stage(g_frame, PET_HUD_SPRITE_W, PET_HUD_SPRITE_H, pet, anim);
    if (g_drawn.valid && (g_pet_x != g_drawn.pet_x)) {
        erase_trail(g_drawn.pet_x, g_pet_x);
    }
    ILI9341_DrawBitmap((uint16_t)g_pet_x, PET_HUD_SPRITE_Y,
                       PET_HUD_SPRITE_W, PET_HUD_SPRITE_H, g_frame);
    g_drawn.pet_x = g_pet_x;
}

void pet_screen_set_pet_x(int16_t x)
{
    const int16_t max = (int16_t)(PET_HUD_SCREEN_W - PET_HUD_SPRITE_W);
    g_pet_x = (x < 0) ? 0 : ((x > max) ? max : x);
}

void pet_screen_init(const pet_t *pet, const pet_anim_t *anim, const pet_ui_t *ui)
{
    if ((pet == NULL) || (ui == NULL)) {
        return;
    }

    ILI9341_FillScreen(PET_HUD_COL_PANEL);
    g_drawn.valid = false;

    pet_screen_update(pet, anim, ui);
}

void pet_screen_update(const pet_t *pet, const pet_anim_t *anim, const pet_ui_t *ui)
{
    if ((pet == NULL) || (ui == NULL)) {
        return;
    }

    const pet_anim_id_t showing = pet_anim_select(pet, anim);
    const uint8_t       frame   = pet_view_frame(anim);
    const pet_mood_t    mood    = pet_mood(pet);

    /* The pet: redraw when the animation, its frame, the mood or its position
       changes. At
       37 ms a blit this is the most expensive thing here, so it is worth not
       doing twice. */
    if (!g_drawn.valid || (showing != g_drawn.anim) ||
        (frame != g_drawn.frame) || (mood != g_drawn.mood) ||
        (g_pet_x != g_drawn.pet_x)) {
        draw_pet(pet, anim);
        g_drawn.anim  = showing;
        g_drawn.frame = frame;
        g_drawn.mood  = mood;
    }

    if (!g_drawn.valid) {
        ILI9341_DrawString(pet_hud_centered_x((uint16_t)strlen(PET_HUD_NAME)),
                           PET_HUD_NAME_Y, PET_HUD_NAME,
                           PET_HUD_COL_TEXT, PET_HUD_COL_PANEL, PET_HUD_TEXT_SCALE);
    }

    if (!g_drawn.valid || (pet_age_day(pet) != g_drawn.day) ||
        (pet_age_hour(pet) != g_drawn.hour)) {
        draw_age(pet);
        g_drawn.day  = pet_age_day(pet);
        g_drawn.hour = pet_age_hour(pet);
    }

    if (!g_drawn.valid || (pet->food != g_drawn.food)) {
        draw_meter(PET_HUD_FOOD_ROW_Y, icon_hunger, pet->food);
        g_drawn.food = pet->food;
    }

    if (!g_drawn.valid || (pet->happiness != g_drawn.happiness)) {
        draw_meter(PET_HUD_HAPPINESS_ROW_Y, icon_happy, pet->happiness);
        g_drawn.happiness = pet->happiness;
    }

    /* The lower block holds either the menu or the ending message, so it is
       redrawn whole when the screen, the selection or the state changes. */
    if (!g_drawn.valid || (pet_ui_screen(ui) != g_drawn.screen) ||
        (pet->state != g_drawn.state) || (pet_ui_portion(ui) != g_drawn.portion) ||
        ((pet_ui_screen(ui) == PET_SCREEN_MENU) &&
         (pet_ui_cursor(ui) != g_drawn.cursor))) {
        draw_lower_block(ui);
        draw_ending(pet);
        g_drawn.screen  = pet_ui_screen(ui);
        g_drawn.cursor  = pet_ui_cursor(ui);
        g_drawn.portion = pet_ui_portion(ui);
        g_drawn.state   = pet->state;
    }

    g_drawn.valid = true;
}
