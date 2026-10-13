/**
 * @file    pet_shake.h
 * @brief   Turns a stream of accelerometer samples into "the player shook it".
 *
 * A shake is several hard jolts close together. One jolt is the acceleration
 * magnitude going above PET_SHAKE_JOLT_MG; the detector then waits for it to
 * drop back below PET_SHAKE_REARM_MG before it will count another, so one long
 * push can't count as several jolts.
 *
 * No hardware dependency: feed it raw samples and a millisecond timestamp.
 * Samples are raw MPU-6050 counts at the +/-2 g range.
 */
#ifndef PET_SHAKE_H
#define PET_SHAKE_H

#include <stdint.h>
#include <stdbool.h>

#define PET_SHAKE_LSB_PER_G     16384   /* MPU-6050 ACCEL_FS_SEL = 0 (+/-2 g) */

#define PET_SHAKE_JOLT_MG       1800    /* |a| at or above this is a jolt */
#define PET_SHAKE_REARM_MG      1300    /* |a| must fall to this before the next */
#define PET_SHAKE_JOLTS         3       /* jolts needed for one shake */
#define PET_SHAKE_WINDOW_MS     1000u   /* ...all within this long of the first */
#define PET_SHAKE_COOLDOWN_MS   1500u   /* ignore everything this long after a shake */

typedef struct {
    uint8_t  jolts;          /* jolts counted in the current window */
    bool     armed;          /* true when the next jolt may be counted */
    bool     cooling;        /* true during the cooldown after a shake */
    uint32_t first_jolt_ms;  /* when the current window started */
    uint32_t fired_ms;       /* when the last shake fired */
} pet_shake_t;

/* Reset to "nothing happening". */
void pet_shake_init(pet_shake_t *s);

/*
 * Feed one sample. Returns true exactly once per detected shake, on the sample
 * that completes it. Safe across HAL_GetTick() wraparound. A NULL detector
 * returns false.
 */
bool pet_shake_update(pet_shake_t *s, int16_t ax, int16_t ay, int16_t az,
                      uint32_t now_ms);

/* Squared magnitude of a sample. Cannot overflow: the worst case is
 * 3 * 32768^2 = 3,221,225,472, which fits in 32 bits unsigned. */
uint32_t pet_shake_mag2(int16_t ax, int16_t ay, int16_t az);

#endif /* PET_SHAKE_H */
