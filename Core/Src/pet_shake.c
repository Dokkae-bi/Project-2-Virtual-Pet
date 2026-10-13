/**
 * @file    pet_shake.c
 * @brief   Shake detection. See pet_shake.h.
 */
#include <stddef.h>

#include "pet_shake.h"

/* Thresholds are compared squared so there is no sqrt on the hot path. */
#define MG_TO_LSB(mg)   ((uint32_t)(((uint32_t)(mg) * PET_SHAKE_LSB_PER_G) / 1000u))
#define JOLT2           (MG_TO_LSB(PET_SHAKE_JOLT_MG)  * MG_TO_LSB(PET_SHAKE_JOLT_MG))
#define REARM2          (MG_TO_LSB(PET_SHAKE_REARM_MG) * MG_TO_LSB(PET_SHAKE_REARM_MG))

uint32_t pet_shake_mag2(int16_t ax, int16_t ay, int16_t az)
{
    /* Each square is at most 32768^2 = 2^30, so it fits in int32 before the cast. */
    return (uint32_t)((int32_t)ax * ax) +
           (uint32_t)((int32_t)ay * ay) +
           (uint32_t)((int32_t)az * az);
}

void pet_shake_init(pet_shake_t *s)
{
    if (s == NULL) { return; }

    s->jolts         = 0u;
    s->armed         = true;
    s->cooling       = false;
    s->first_jolt_ms = 0u;
    s->fired_ms      = 0u;
}

bool pet_shake_update(pet_shake_t *s, int16_t ax, int16_t ay, int16_t az,
                      uint32_t now_ms)
{
    if (s == NULL) { return false; }

    if (s->cooling) {
        if ((uint32_t)(now_ms - s->fired_ms) < PET_SHAKE_COOLDOWN_MS) {
            return false;
        }
        s->cooling = false;
        s->jolts   = 0u;
        s->armed   = true;
    }

    /* Jolts that are too far apart don't add up. */
    if ((s->jolts > 0u) &&
        ((uint32_t)(now_ms - s->first_jolt_ms) > PET_SHAKE_WINDOW_MS)) {
        s->jolts = 0u;
    }

    const uint32_t m2 = pet_shake_mag2(ax, ay, az);

    if (!s->armed) {
        if (m2 <= REARM2) { s->armed = true; }
        return false;
    }

    if (m2 < JOLT2) {
        return false;
    }

    s->armed = false;
    if (s->jolts == 0u) { s->first_jolt_ms = now_ms; }
    s->jolts++;

    if (s->jolts >= PET_SHAKE_JOLTS) {
        s->jolts    = 0u;
        s->cooling  = true;
        s->fired_ms = now_ms;
        return true;
    }
    return false;
}
