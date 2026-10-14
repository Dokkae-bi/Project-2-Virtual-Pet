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

/* --- action menu ---------------------------------------------------------- */

#define PET_HUD_MENU_X          20u
#define PET_HUD_MENU_Y          248u
#define PET_HUD_MENU_W          200u
#define PET_HUD_MENU_ROW_H      24u

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

/* Top edge of menu row `index`. Out of range wraps, so it can't go off screen. */
uint16_t pet_hud_menu_row_y(uint8_t index);

#endif /* PET_HUD_H */
