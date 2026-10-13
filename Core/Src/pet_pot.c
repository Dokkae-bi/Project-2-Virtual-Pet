/**
 * @file    pet_pot.c
 * @brief   Potentiometer -> portion mapping. See pet_pot.h.
 */
#include <stddef.h>

#include "pet_pot.h"

#define BANDS   (PET_FEED_PORTION_MAX - PET_FEED_PORTION_MIN + 1u)

uint8_t pet_pot_portion_of(uint16_t raw)
{
    const uint32_t r = (raw > PET_POT_ADC_MAX) ? PET_POT_ADC_MAX : raw;
    return (uint8_t)(PET_FEED_PORTION_MIN + (r * BANDS) / (PET_POT_ADC_MAX + 1u));
}

void pet_pot_init(pet_pot_t *p)
{
    if (p == NULL) { return; }

    p->filtered = 0u;
    p->portion  = PET_FEED_PORTION_MIN;
    p->primed   = false;
}

uint8_t pet_pot_update(pet_pot_t *p, uint16_t raw)
{
    if (p == NULL) { return PET_FEED_PORTION_MIN; }

    if (raw > PET_POT_ADC_MAX) { raw = PET_POT_ADC_MAX; }

    if (!p->primed) {
        p->primed   = true;
        p->filtered = raw;
        p->portion  = pet_pot_portion_of(raw);
        return p->portion;
    }

    /* Running average. The shift of a negative difference must not rely on
       implementation-defined behaviour, so do it on the magnitude. */
    if (raw >= p->filtered) {
        p->filtered = (uint16_t)(p->filtered + ((raw - p->filtered) >> PET_POT_SMOOTH_SHIFT));
    } else {
        p->filtered = (uint16_t)(p->filtered - ((p->filtered - raw) >> PET_POT_SMOOTH_SHIFT));
    }

    /* Only move once the reading is clearly past the edge, in either direction. */
    const uint16_t f    = p->filtered;
    const uint16_t low  = (f > PET_POT_HYST) ? (uint16_t)(f - PET_POT_HYST) : 0u;
    const uint16_t high = (uint16_t)(f + PET_POT_HYST);   /* clamped by portion_of */
    const uint8_t  up   = pet_pot_portion_of(low);
    const uint8_t  down = pet_pot_portion_of(high);

    if (up > p->portion) {
        p->portion = up;
    } else if (down < p->portion) {
        p->portion = down;
    }
    return p->portion;
}
