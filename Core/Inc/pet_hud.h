/**
 * @file    pet_hud.h
 * @brief   Screen layout and meter maths for the pet display.
 *
 * Holds the geometry of the 240x320 HUD (where the sprite, meters and menu go)
 * and the arithmetic behind the meters - how full a bar is and what colour it
 * should be.
 *
 * No display calls in here at all, so the layout can be checked on a PC. The
 * module that actually draws it is pet_screen.c.
 */
#ifndef PET_HUD_H
#define PET_HUD_H

#include <stddef.h>
#include <stdint.h>

#include "pet_core.h"
#include "pet_ui.h"

/* Panel is 240x320 portrait. */
#define PET_HUD_SCREEN_W        240u
#define PET_HUD_SCREEN_H        320u

/* --- stage: where the slime sits ----------------------------------------- */

#define PET_HUD_SPRITE_W        64u
#define PET_HUD_SPRITE_H        64u
#define PET_HUD_SPRITE_X        88u     /* (240 - 64) / 2, centred */

/* The sprite's feet end up here once the frame is drawn. */
#define PET_HUD_SPRITE_Y        100u

/* --- meter rows ----------------------------------------------------------- */

#define PET_HUD_ICON_SIZE       24u
#define PET_HUD_ICON_X          8u
#define PET_HUD_FOOD_ROW_Y      184u
#define PET_HUD_HAPPINESS_ROW_Y 214u

/* The bar track sits slightly lower than the icon so the two line up visually. */
#define PET_HUD_TRACK_X         40u
#define PET_HUD_TRACK_DY        6u
#define PET_HUD_TRACK_W         160u
#define PET_HUD_TRACK_H         12u

/* The number beside each bar. It is cleared before each redraw, so it must be
 * wide enough for the longest value, "100": 3 characters, 12 px each. */
#define PET_HUD_READOUT_X       (PET_HUD_TRACK_X + PET_HUD_TRACK_W + 4u)
#define PET_HUD_READOUT_W       36u

/* --- action menu ---------------------------------------------------------- */

#define PET_HUD_MENU_X          20u
#define PET_HUD_MENU_Y          248u
#define PET_HUD_MENU_W          200u
#define PET_HUD_MENU_ROW_H      24u

/* --- name and age, centred above the sprite ------------------------------- */

#define PET_HUD_NAME            "Slimey"
#define PET_HUD_NAME_Y          12u
#define PET_HUD_DAY_Y           34u

/* --- text -------------------------------------------------------------- */

/* All UI text is drawn at this size: 5x7 glyphs become 10x14, 12 px apart. */
#define PET_HUD_TEXT_SCALE      2u

/* Ending screen, drawn in the lower block. Kept here (not in pet_screen.c)
 * so tests/test_ui_text.c can check they are drawable and fit. */
#define PET_HUD_TEXT_STARVED    "Your pet starved"
#define PET_HUD_TEXT_RAN_AWAY   "Your pet ran away"
#define PET_HUD_TEXT_RESTART1   "Press select"
#define PET_HUD_TEXT_RESTART2   "to restart"

/*
 * Meter colours as RGB565. The first two are already in the project palette in
 * docs/pet-design.md; the red is new and only used for the critical band.
 */
#define PET_HUD_COL_GOOD        0x4656u   /* #46C8B4  healthy   */
#define PET_HUD_COL_WARN        0xFD84u   /* #FFB020  marginal  */
#define PET_HUD_COL_BAD         0xE1E7u   /* #E03C3C  critical  */

#define PET_HUD_COL_TRACK       0x2966u   /* #2A2F36  empty part of the bar */
#define PET_HUD_COL_PANEL       0x1946u   /* #1E2A32  same as the stage */
#define PET_HUD_COL_TEXT        0xD7FEu   /* #D6FFF6  near-white */
#define PET_HUD_COL_SELECTED    0x2C70u   /* #2A8C80  highlighted menu row */

/* Band edges. Both meters count up towards good. */
#define PET_HUD_GOOD_AT         70
#define PET_HUD_BAD_AT          30

/*
 * How many pixels of a track_w-wide bar are filled for a statistic of 0-100.
 * Out of range values are clamped, so a bad reading can't draw outside the bar.
 */
uint16_t pet_hud_meter_fill_px(int16_t value, uint16_t track_w);

/*
 * Colour for a meter: teal at PET_HUD_GOOD_AT and above, red below
 * PET_HUD_BAD_AT, amber in between. Out of range values are clamped.
 */
uint16_t pet_hud_meter_color(int16_t value);

/* Size of the buffer pet_hud_age_text() needs: "Day 1431655766  23h" is the
 * longest text a 32-bit age can give. */
#define PET_HUD_AGE_TEXT_MAX    24u

/* Writes "Day N  HHh" for the pet's age. Always terminated; a zero-size
 * buffer is left alone. */
void pet_hud_age_text(char *buf, size_t size, const pet_t *pet);

/* Left edge that centres `chars` characters of text on the screen at
 * PET_HUD_TEXT_SCALE. Text wider than the screen starts at 0. */
uint16_t pet_hud_centered_x(uint16_t chars);

/* Top edge of menu row `index`. Out of range wraps, so it can't go off screen. */
uint16_t pet_hud_menu_row_y(uint8_t index);

#endif /* PET_HUD_H */
