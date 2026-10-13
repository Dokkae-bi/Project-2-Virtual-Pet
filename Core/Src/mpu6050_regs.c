/**
 * @file    mpu6050_regs.c
 * @brief   MPU-6050 byte decoding. See mpu6050_regs.h.
 */
#include "mpu6050_regs.h"

bool mpu6050_who_am_i_ok(uint8_t id)
{
    return (id == 0x68u) || (id == 0x70u) || (id == 0x72u);
}

bool mpu6050_is_asleep(uint8_t pwr_mgmt_1)
{
    return (pwr_mgmt_1 & MPU6050_PWR_SLEEP) != 0u;
}

void mpu6050_accel_from_bytes(const uint8_t raw[6], int16_t out[3])
{
    for (int i = 0; i < 3; i++) {
        out[i] = (int16_t)(((uint16_t)raw[2 * i] << 8) | raw[2 * i + 1]);
    }
}
