/**
 ******************************************************************************
 * @file    pet_ui.h
 * @brief   Screen and menu state machine.
 *
 * Decides what the user is looking at and what an input event means there. Two
 * screens exist so far: the pet itself, and the action menu.
 *
 * Like the rest of the logic layers this module is hardware-free. Inputs arrive
 * as logical events and the pet model is passed in, so every transition is
 * exercisable on a host PC with no board attached - see tests/test_pet_ui.c.
 *
 * The module is strictly event-driven: it never waits, never polls and never
 * calls a delay. That is what keeps the interface responsive while an animation
 * is playing, which the project brief calls out explicitly.
 ******************************************************************************
 */
#ifndef PET_UI_H
#define PET_UI_H

#include <stdint.h>
#include <stdbool.h>

#include "pet_core.h"

/** One logical input event, decoupled from the physical source. */
typedef enum {
    PET_INPUT_NONE = 0,     /* no event this pass                            */
    PET_INPUT_UP,           /* joystick up                                   */
    PET_INPUT_DOWN,         /* joystick down                                 */
    PET_INPUT_LEFT,         /* joystick left   (reserved)                    */
    PET_INPUT_RIGHT,        /* joystick right  (reserved)                    */
    PET_INPUT_SELECT,       /* joystick centre: open menu / confirm / restart */
    PET_INPUT_BACK,         /* user button: close the menu without acting    */
    PET_INPUT_COUNT         /* sentinel - keep last                          */
} pet_input_t;

/** Which screen is on the display. */
typedef enum {
    PET_SCREEN_PET = 0,     /* the pet, plus its status                        */
    PET_SCREEN_MENU,        /* the action menu                                 */
    PET_SCREEN_COUNT        /* sentinel - keep last                            */
} pet_screen_t;

/** Live state of the interface. */
typedef struct {
    pet_screen_t screen;
    uint8_t      cursor;    /* highlighted menu row                            */
    uint8_t      portion;   /* display only: what the analog input selects     */
} pet_ui_t;

/** What the caller must do as a result of one event. */
typedef struct {
    pet_action_t action;    /* committed action, or PET_ACTION_NONE            */
    bool         restart;   /* the user asked to restart a finished pet        */
    bool         redraw;    /* the visible screen changed since the last call  */
} pet_ui_result_t;

/* ---------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

/** Initialise the interface: pet screen, cursor at the top, minimum portion. */
void pet_ui_init(pet_ui_t *ui);

/**
 * @brief Feed one input event in and get back what it meant.
 *
 * @param pet the live pet model. A dead or runaway pet locks the menu out and
 *            turns SELECT into a restart request, which is what makes the
 *            restart procedure reachable from the user interface.
 */
pet_ui_result_t pet_ui_handle(pet_ui_t *ui, pet_input_t input, const pet_t *pet);

/** Number of rows in the action menu. */
uint8_t pet_ui_item_count(void);

/** Label for a menu row, for rendering. Never NULL; out-of-range clamps. */
const char *pet_ui_item_label(uint8_t index);

/** The action a menu row commits. Out-of-range returns PET_ACTION_NONE. */
pet_action_t pet_ui_item_action(uint8_t index);

/** Current screen, cursor and portion - for the renderer. */
pet_screen_t pet_ui_screen(const pet_ui_t *ui);
uint8_t      pet_ui_cursor(const pet_ui_t *ui);
uint8_t      pet_ui_portion(const pet_ui_t *ui);

/**
 * @brief Record the portion the analog input currently selects.
 *
 * Display only: the menu shows it beside the Feed row so the potentiometer
 * visibly participates in the interface instead of being decorative.
 */
void pet_ui_set_portion(pet_ui_t *ui, uint8_t portion);

/** Human-readable input name, for host tests and debug output. */
const char *pet_ui_input_name(pet_input_t input);

#endif /* PET_UI_H */
