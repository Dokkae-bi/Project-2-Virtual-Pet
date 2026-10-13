/**
 * @file    mpu6050.h
 * @brief   Minimal MPU-6050 driver over HAL I2C: wake it, read the accelerometer.
 *
 * Every call is blocking but bounded by MPU6050_TIMEOUT_MS, so a missing or
 * unplugged sensor costs a few milliseconds and an error code, never a hang.
 */
#ifndef MPU6050_H
#define MPU6050_H

#include "stm32l4xx_hal.h"
#include "mpu6050_regs.h"

#define MPU6050_TIMEOUT_MS  5u

/* Check WHO_AM_I and configure: awake, +/-2 g, 44 Hz filter, 100 Hz rate. */
HAL_StatusTypeDef mpu6050_init(I2C_HandleTypeDef *hi2c);

/* HAL_OK if the sensor is still awake; HAL_ERROR if it has reset (gone back to
 * sleep) and needs mpu6050_init() again, or the HAL error if the read failed. */
HAL_StatusTypeDef mpu6050_check_awake(I2C_HandleTypeDef *hi2c);

/* Read one x, y, z sample in raw counts (16384 per g). */
HAL_StatusTypeDef mpu6050_read_accel(I2C_HandleTypeDef *hi2c, int16_t out[3]);

#endif /* MPU6050_H */
