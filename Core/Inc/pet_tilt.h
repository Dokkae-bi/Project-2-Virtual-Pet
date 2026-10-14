/**
 * @file    pet_tilt.h
 * @brief   Turns the tilt of the accelerometer into the slime's position.
 *
 * Holding the board still, gravity shows up on whichever axis points down, so
 * the X reading says how far it is tipped left or right. Inside a dead zone the
 * slime stays put; beyond it the slime slides, faster the further the tilt, and
 * stops at the screen edges. Levelling the board stops it where it is.
 *
 * No hardware dependency: feed it the raw X reading and a millisecond timestamp.
 * Samples are raw MPU-6050 counts at the +/-2 g range (16384 per g).
 */
#ifndef PET_TILT_H
#define PET_TILT_H

#include <stdint.h>
#include <stdbool.h>

#include "pet_hud.h"

/* Slime's left edge may be anywhere from the panel's left edge to flush right. */
#define PET_TILT_X_MIN        0
#define PET_TILT_X_MAX        ((int16_t)(PET_HUD_SCREEN_W - PET_HUD_SPRITE_W))

#define PET_TILT_DEAD_COUNTS  4200    /* ~0.26 g, about 15 degrees: no movement */
#define PET_TILT_FULL_COUNTS  12000   /* ~0.73 g, about 47 degrees: top speed */
#define PET_TILT_MIN_STEP     1       /* pixels per move just past the dead zone */
#define PET_TILT_MAX_STEP     8       /* pixels per move at full tilt */
#define PET_TILT_PERIOD_MS    40u     /* time between moves */

/* +1 if a positive X reading should slide the slime right, -1 if left. Depends on
 * which way the board is mounted; flip it if the slime goes the wrong way. */
#define PET_TILT_DIR          1

typedef struct {
    int16_t  x;         /* the slime's left edge, in pixels */
    uint32_t next_ms;   /* earliest time of the next move */
} pet_tilt_t;

/* Put the slime back in the centre and allow a move at now_ms. */
void pet_tilt_init(pet_tilt_t *t, uint32_t now_ms);

/*
 * Signed pixels to move for one X reading: 0 inside the dead zone, otherwise
 * PET_TILT_MIN_STEP..PET_TILT_MAX_STEP in the direction of the tilt.
 */
int16_t pet_tilt_step(int16_t ax);

/*
 * Feed one X reading. Moves at most once per PET_TILT_PERIOD_MS. Returns true
 * when the slime's position changed. Safe across HAL_GetTick() wraparound. A
 * NULL tracker returns false.
 */
bool pet_tilt_update(pet_tilt_t *t, int16_t ax, uint32_t now_ms);

#endif /* PET_TILT_H */
