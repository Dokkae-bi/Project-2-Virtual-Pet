/**
 ******************************************************************************
 * @file    pet_ui.c
 * @brief   Screen and menu state machine. See pet_ui.h.
 ******************************************************************************
 */
#include "pet_ui.h"

#include <stddef.h>   /* NULL */

/* The three required interactions, in menu order. */
static const struct {
    pet_action_t action;
    const char  *label;
} UI_ITEMS[] = {
    { PET_ACTION_FEED,  "Feed"  },
    { PET_ACTION_PLAY,  "Play"  },
    { PET_ACTION_CLEAN, "Clean" },
};

#define UI_ITEM_COUNT ((uint8_t)(sizeof(UI_ITEMS) / sizeof(UI_ITEMS[0])))

/* ---------------------------------------------------------------------------
 * Accessors
 * ------------------------------------------------------------------------- */

uint8_t pet_ui_item_count(void)
{
    return UI_ITEM_COUNT;
}

const char *pet_ui_item_label(uint8_t index)
{
    if (index >= UI_ITEM_COUNT) {
        index = (uint8_t)(UI_ITEM_COUNT - 1u);
    }
    return UI_ITEMS[index].label;
}

pet_action_t pet_ui_item_action(uint8_t index)
{
    if (index >= UI_ITEM_COUNT) {
        return PET_ACTION_NONE;
    }
    return UI_ITEMS[index].action;
}

pet_screen_t pet_ui_screen(const pet_ui_t *ui)
{
    return (ui == NULL) ? PET_SCREEN_PET : ui->screen;
}

uint8_t pet_ui_cursor(const pet_ui_t *ui)
{
    return (ui == NULL) ? 0u : ui->cursor;
}

uint8_t pet_ui_portion(const pet_ui_t *ui)
{
    return (ui == NULL) ? PET_FEED_PORTION_MIN : ui->portion;
}

void pet_ui_set_portion(pet_ui_t *ui, uint8_t portion)
{
    if (ui == NULL) {
        return;
    }
    if (portion < PET_FEED_PORTION_MIN) { portion = PET_FEED_PORTION_MIN; }
    if (portion > PET_FEED_PORTION_MAX) { portion = PET_FEED_PORTION_MAX; }
    ui->portion = portion;
}

const char *pet_ui_input_name(pet_input_t input)
{
    switch (input) {
        case PET_INPUT_UP:     return "up";
        case PET_INPUT_DOWN:   return "down";
        case PET_INPUT_LEFT:   return "left";
        case PET_INPUT_RIGHT:  return "right";
        case PET_INPUT_SELECT: return "select";
        case PET_INPUT_BACK:   return "back";
        case PET_INPUT_NONE:
        default:               return "none";
    }
}

/* ---------------------------------------------------------------------------
 * State machine
 * ------------------------------------------------------------------------- */

void pet_ui_init(pet_ui_t *ui)
{
    if (ui == NULL) {
        return;
    }
    ui->screen  = PET_SCREEN_PET;
    ui->cursor  = 0u;
    ui->portion = PET_FEED_PORTION_MIN;
}

pet_ui_result_t pet_ui_handle(pet_ui_t *ui, pet_input_t input, const pet_t *pet)
{
    pet_ui_result_t result = { PET_ACTION_NONE, false, false };

    if (ui == NULL) {
        return result;
    }

    /* A NULL model is treated as interactive: the state machine stays usable in
     * isolation, and only an explicit terminal state locks it out. */
    const bool interactive = (pet == NULL) ? true : pet_is_interactive(pet);

    /*
     * If the pet reaches a terminal state while the menu is open, close the menu
     * and consume this event doing it. Leaving the menu up would hide the very
     * screen that announces the ending.
     */
    if (!interactive && (ui->screen != PET_SCREEN_PET)) {
        ui->screen  = PET_SCREEN_PET;
        result.redraw = true;
        return result;
    }

    switch (ui->screen) {

    case PET_SCREEN_PET:
        if (input == PET_INPUT_SELECT) {
            result.redraw = true;
            if (interactive) {
                ui->screen = PET_SCREEN_MENU;
                ui->cursor = 0u;
            } else {
                /* The only meaningful action left. */
                result.restart = true;
            }
        }
        break;

    case PET_SCREEN_MENU:
        switch (input) {
        case PET_INPUT_UP:
            /* Wrap, so the menu cannot get stuck at either end. */
            ui->cursor = (uint8_t)((ui->cursor + UI_ITEM_COUNT - 1u) % UI_ITEM_COUNT);
            result.redraw = true;
            break;

        case PET_INPUT_DOWN:
            ui->cursor = (uint8_t)((ui->cursor + 1u) % UI_ITEM_COUNT);
            result.redraw = true;
            break;

        case PET_INPUT_SELECT:
            result.action = UI_ITEMS[ui->cursor].action;
            result.redraw = true;
            ui->screen = PET_SCREEN_PET;
            break;

        case PET_INPUT_BACK:
            /* Cancel: go back without acting. */
            result.redraw = true;
            ui->screen = PET_SCREEN_PET;
            break;

        default:
            /* LEFT/RIGHT are reserved and deliberately do nothing here. */
            break;
        }
        break;

    default:
        /* Unreachable unless the state was corrupted; recover rather than loop. */
        ui->screen = PET_SCREEN_PET;
        result.redraw = true;
        break;
    }

    return result;
}
