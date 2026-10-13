/**
 * @file    pet_pot.h
 * @brief   Turns raw potentiometer readings into a feeding portion, 1 to 5.
 *
 * The 12-bit ADC range is split into PET_FEED_PORTION_MAX equal bands. Two
 * things keep the result steady enough to show on screen:
 *
 *   - a running average (1/4 weight per sample) takes out ADC noise, and
 *   - hysteresis: the reading has to go PET_POT_HYST counts past a band edge
 *     before the portion changes, so a knob resting on an edge can't make the
 *     menu flicker between two values (and redraw over SPI each time).
 *
 * No hardware dependency: feed it raw readings, get a portion back.
 */
#ifndef PET_POT_H
#define PET_POT_H

#include <stdint.h>
#include <stdbool.h>

#include "pet_core.h"

#define PET_POT_ADC_MAX         4095u   /* 12-bit ADC full scale */
#define PET_POT_HYST            60u     /* ~1.5% of the knob's travel */
#define PET_POT_SMOOTH_SHIFT    2       /* running average weight 1/(2^2) */

typedef struct {
    uint16_t filtered;   /* smoothed reading, 0..PET_POT_ADC_MAX */
    uint8_t  portion;    /* portion currently selected */
    bool     primed;     /* false until the first reading */
} pet_pot_t;

/* Start at the smallest portion, waiting for a first reading. */
void pet_pot_init(pet_pot_t *p);

/*
 * Feed one raw reading and return the selected portion. The first reading is
 * taken as-is, so the menu is right from power-up rather than ramping up. A
 * reading above PET_POT_ADC_MAX is clamped. A NULL pot returns the smallest
 * portion.
 */
uint8_t pet_pot_update(pet_pot_t *p, uint16_t raw);

/* The band a reading falls in, with no smoothing or hysteresis. */
uint8_t pet_pot_portion_of(uint16_t raw);

#endif /* PET_POT_H */
