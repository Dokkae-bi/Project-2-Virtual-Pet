/**
 * @file    pet_screen.h
 * @brief   Draws the pet screen on the LCD.
 *
 * The only pet_* module that touches the display. It takes the pure layout from
 * pet_hud.h and the composed frame from pet_view.h, and pushes them to the panel.
 *
 * It redraws only what changed - the sprite region when the animation advances,
 * a meter when a statistic moves, the lower block when the selection or the
 * state changes. A full 240x320 repaint costs 692 ms over SPI, so repainting
 * everything every pass is not an option.
 */
#ifndef PET_SCREEN_H
#define PET_SCREEN_H

#include <stdint.h>

#include "pet_core.h"
#include "pet_anim.h"
#include "pet_ui.h"

/* Call once after ILI9341_Init(). Clears the panel and draws everything. */
void pet_screen_init(const pet_t *pet, const pet_anim_t *anim, const pet_ui_t *ui);

/* Redraw whatever has changed since the last call. Safe to call every pass. */
void pet_screen_update(const pet_t *pet, const pet_anim_t *anim, const pet_ui_t *ui);

#endif /* PET_SCREEN_H */
