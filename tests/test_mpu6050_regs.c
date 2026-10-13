/**
 * @file    test_mpu6050_regs.c
 * @brief   Tests for the MPU-6050 byte decoding.
 *
 * Byte order and sign are the easy things to get wrong here, and on the board
 * they would only show up as a pet that jumps when nobody touches it.
 *
 *     cc -std=c11 -Wall -Wextra -I Core/Inc tests/test_mpu6050_regs.c \
 *        Core/Src/mpu6050_regs.c -o /tmp/test_mpu6050_regs && /tmp/test_mpu6050_regs
 */
#include <stdio.h>

#include "mpu6050_regs.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) {                                                       \
            g_pass++;                                                     \
            printf("  [PASS] %s\n", (msg));                               \
        } else {                                                          \
            g_fail++;                                                     \
            printf("  [FAIL] %s   (%s:%d)\n", (msg), __FILE__, __LINE__); \
        }                                                                 \
    } while (0)

static void section(const char *name) { printf("\n%s\n", name); }

static void test_who_am_i(void)
{
    section("WHO_AM_I");
    CHECK(mpu6050_who_am_i_ok(0x68u), "a genuine MPU-6050 is accepted");
    CHECK(mpu6050_who_am_i_ok(0x70u), "the 0x70 clone is accepted");
    CHECK(mpu6050_who_am_i_ok(0x72u), "the 0x72 clone is accepted");
    CHECK(!mpu6050_who_am_i_ok(0x00u), "0x00 (bus held low) is rejected");
    CHECK(!mpu6050_who_am_i_ok(0xFFu), "0xFF (nothing on the bus) is rejected");
}

static void test_decode(void)
{
    section("Decoding a sample");
    const uint8_t raw[6] = { 0x40u, 0x00u, 0xFFu, 0xFFu, 0x80u, 0x00u };
    int16_t a[3] = { 0, 0, 0 };
    mpu6050_accel_from_bytes(raw, a);
    CHECK(a[0] == 16384, "high byte first: 0x4000 is +1 g");
    CHECK(a[1] == -1, "0xFFFF is -1, not 65535");
    CHECK(a[2] == -32768, "0x8000 is the most negative reading");
}

static void test_asleep(void)
{
    section("Spotting a sensor that reset behind our back");
    CHECK(mpu6050_is_asleep(0x40u), "the power-on value of PWR_MGMT_1 is asleep");
    CHECK(!mpu6050_is_asleep(MPU6050_PWR_CLK_PLL_X), "the value init writes is awake");
    CHECK(mpu6050_is_asleep(0x41u), "SLEEP set with another clock is still asleep");
}

int main(void)
{
    printf("mpu6050_regs tests\n");

    test_who_am_i();
    test_decode();
    test_asleep();

    printf("\n----------------------------------------\n");
    printf("%d passed, %d failed\n", g_pass, g_fail);
    printf("----------------------------------------\n");
    return (g_fail == 0) ? 0 : 1;
}
