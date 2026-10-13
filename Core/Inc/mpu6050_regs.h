/**
 * @file    mpu6050_regs.h
 * @brief   MPU-6050 register map and the byte-level decoding.
 *
 * Kept apart from the I2C driver so it can be tested on a PC.
 * Register addresses are from the MPU-6000/6050 Register Map, rev 4.2.
 */
#ifndef MPU6050_REGS_H
#define MPU6050_REGS_H

#include <stdint.h>
#include <stdbool.h>

#define MPU6050_ADDR_7BIT       0x68u   /* AD0 low (GY-521 default) */

#define MPU6050_REG_SMPLRT_DIV  0x19u
#define MPU6050_REG_CONFIG      0x1Au
#define MPU6050_REG_ACCEL_CFG   0x1Cu
#define MPU6050_REG_ACCEL_XOUT  0x3Bu   /* 6 bytes: X, Y, Z, each high byte first */
#define MPU6050_REG_PWR_MGMT_1  0x6Bu
#define MPU6050_REG_WHO_AM_I    0x75u

#define MPU6050_PWR_CLK_PLL_X   0x01u   /* awake, clock from the X gyro PLL */
#define MPU6050_DLPF_44HZ       0x03u
#define MPU6050_SMPLRT_100HZ    9u      /* 1 kHz / (1 + 9) with the DLPF on */
#define MPU6050_ACCEL_FS_2G     0x00u

/*
 * True if WHO_AM_I looks like a part this driver can talk to. A genuine MPU-6050
 * answers 0x68. Cheap GY-521 boards are often fitted with a compatible clone
 * that answers 0x70 or 0x72 but has the same accelerometer registers.
 * 0x00 and 0xFF are what a missing or shorted bus reads back, so they fail.
 */
bool mpu6050_who_am_i_ok(uint8_t id);

/* Decode the 6 bytes read from ACCEL_XOUT into signed x, y, z counts. */
void mpu6050_accel_from_bytes(const uint8_t raw[6], int16_t out[3]);

#endif /* MPU6050_REGS_H */
