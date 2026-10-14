/**
 * @file    pet_tilt.c
 * @brief   Tilt-to-position. See pet_tilt.h.
 */
#include "pet_tilt.h"

#include <stddef.h>

void pet_tilt_init(pet_tilt_t *t, uint32_t now_ms)
{
    if (t == NULL) {
        return;
    }
    t->x       = (int16_t)PET_HUD_SPRITE_X;
    t->next_ms = now_ms;
}

int16_t pet_tilt_step(int16_t ax)
{
    const int32_t v = ax;
    const int32_t a = (v < 0) ? -v : v;

    if (a <= PET_TILT_DEAD_COUNTS) {
        return 0;
    }

    int32_t step = PET_TILT_MAX_STEP;
    if (a < PET_TILT_FULL_COUNTS) {
        step = PET_TILT_MIN_STEP +
               ((a - PET_TILT_DEAD_COUNTS) * (PET_TILT_MAX_STEP - PET_TILT_MIN_STEP)) /
               (PET_TILT_FULL_COUNTS - PET_TILT_DEAD_COUNTS);
    }

    return (int16_t)(((v < 0) ? -step : step) * PET_TILT_DIR);
}

bool pet_tilt_update(pet_tilt_t *t, int16_t ax, uint32_t now_ms)
{
    if (t == NULL) {
        return false;
    }
    if ((int32_t)(now_ms - t->next_ms) < 0) {
        return false;
    }
    t->next_ms = now_ms + PET_TILT_PERIOD_MS;

    int32_t x = (int32_t)t->x + pet_tilt_step(ax);
    if (x < PET_TILT_X_MIN) {
        x = PET_TILT_X_MIN;
    } else if (x > PET_TILT_X_MAX) {
        x = PET_TILT_X_MAX;
    }

    const bool moved = (x != t->x);
    t->x = (int16_t)x;
    return moved;
}
