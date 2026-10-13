/**
 * @file    mpu6050.c
 * @brief   MPU-6050 I2C driver. See mpu6050.h.
 */
#include "mpu6050.h"

/* HAL wants the 7-bit address shifted into the top bits. */
#define DEV_ADDR    ((uint16_t)(MPU6050_ADDR_7BIT << 1))

static HAL_StatusTypeDef write_reg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(hi2c, DEV_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1u, MPU6050_TIMEOUT_MS);
}

HAL_StatusTypeDef mpu6050_init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id = 0u;
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(hi2c, DEV_ADDR, MPU6050_REG_WHO_AM_I,
                                            I2C_MEMADD_SIZE_8BIT, &id, 1u,
                                            MPU6050_TIMEOUT_MS);
    if (st != HAL_OK)             { return st; }
    if (!mpu6050_who_am_i_ok(id)) { return HAL_ERROR; }

    /* The chip powers up asleep; everything else is ignored until it wakes. */
    if ((st = write_reg(hi2c, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR_CLK_PLL_X)) != HAL_OK) { return st; }
    if ((st = write_reg(hi2c, MPU6050_REG_CONFIG,     MPU6050_DLPF_44HZ))     != HAL_OK) { return st; }
    if ((st = write_reg(hi2c, MPU6050_REG_SMPLRT_DIV, MPU6050_SMPLRT_100HZ))  != HAL_OK) { return st; }
    return write_reg(hi2c, MPU6050_REG_ACCEL_CFG, MPU6050_ACCEL_FS_2G);
}

HAL_StatusTypeDef mpu6050_read_accel(I2C_HandleTypeDef *hi2c, int16_t out[3])
{
    uint8_t raw[6];
    const HAL_StatusTypeDef st = HAL_I2C_Mem_Read(hi2c, DEV_ADDR, MPU6050_REG_ACCEL_XOUT,
                                                  I2C_MEMADD_SIZE_8BIT, raw, sizeof raw,
                                                  MPU6050_TIMEOUT_MS);
    if (st == HAL_OK) {
        mpu6050_accel_from_bytes(raw, out);
    }
    return st;
}
