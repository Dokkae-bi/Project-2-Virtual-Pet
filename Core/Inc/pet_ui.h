/**
 * @file    pet_ui.h
 * @brief   Screen and menu state machine.
 *
 * Decides what the user is looking at and what an input event means there. Two
 * screens so far: the pet itself, and the action menu.
 *
 * Inputs come in as logical events and the pet model is passed in, so every
 * transition can be checked on a PC with no board attached. Nothing here waits,
 * polls or delays, which is what keeps the interface responsive while an
 * animation is playing.
 */
#ifndef PET_UI_H
#define PET_UI_H

#include <stdint.h>
#include <stdbool.h>

#include "pet_core.h"

/* One logical input event, kept separate from the physical source. */
typedef enum {
    PET_INPUT_NONE = 0,     /* no event this pass */
    PET_INPUT_UP,           /* joystick up */
    PET_INPUT_DOWN,         /* joystick down */
    PET_INPUT_LEFT,         /* reserved */
    PET_INPUT_RIGHT,        /* reserved */
    PET_INPUT_SELECT,       /* joystick centre: open menu / confirm / restart */
    PET_INPUT_BACK,         /* user button: leave the menu without acting */
    PET_INPUT_COUNT
} pet_input_t;

/* Which screen is on the display. */
typedef enum {
    PET_SCREEN_PET = 0,     /* the pet and its status */
    PET_SCREEN_MENU,        /* the action menu */
    PET_SCREEN_COUNT
} pet_screen_t;

/* Live state of the interface. */
typedef struct {
    pet_screen_t screen;
    uint8_t      cursor;    /* highlighted menu row */
    uint8_t      portion;   /* display only, from the analog input */
} pet_ui_t;

/* What the caller needs to do after one event. */
typedef struct {
    pet_action_t action;    /* committed action, or PET_ACTION_NONE */
    bool         restart;   /* the user asked to restart a finished pet */
    bool         redraw;    /* the screen changed since the last call */
} pet_ui_result_t;

/* Start on the pet screen with the cursor at the top. */
void pet_ui_init(pet_ui_t *ui);

/*
 * Feed one input event in and get back what it meant.
 *
 * pet is the live model. A dead or runaway pet locks the menu out and turns
 * SELECT into a restart request, which is how the restart procedure is reached
 * from the UI.
 */
pet_ui_result_t pet_ui_handle(pet_ui_t *ui, pet_input_t input, const pet_t *pet);

/* Number of rows in the action menu. */
uint8_t pet_ui_item_count(void);

/* Label for a menu row, for drawing. Never NULL; out of range clamps. */
const char *pet_ui_item_label(uint8_t index);

/* Action a menu row commits. Out of range returns PET_ACTION_NONE. */
pet_action_t pet_ui_item_action(uint8_t index);

/* Current screen, cursor and portion, for the renderer. */
pet_screen_t pet_ui_screen(const pet_ui_t *ui);
uint8_t      pet_ui_cursor(const pet_ui_t *ui);
uint8_t      pet_ui_portion(const pet_ui_t *ui);

/*
 * Record the portion the analog input currently selects. Display only - the menu
 * shows it next to the Feed row so the potentiometer actually does something.
 */
void pet_ui_set_portion(pet_ui_t *ui, uint8_t portion);

/* Input name, for tests and debug output. */
const char *pet_ui_input_name(pet_input_t input);

#endif /* PET_UI_H */
