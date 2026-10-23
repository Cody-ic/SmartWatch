/*
 * MPU6050 旧版驱动已弃用，保留供参考与教学使用。
 * 当前手表已改用 LSM6DSOX；本文件继续通过 #if 0 排除编译。
 */
#if 0
#include "MPU6050.h"

/* STM32 HAL expects the 7-bit device address shifted left once: 0x68 << 1. */
#define MPU6050_Addr 0xD0
#define SENSOR_I2C_TIMEOUT 10  /* Timeout for each blocking I2C call, in ms. */

/* Bound by MPU6050_Init; a non-NULL pointer alone does not prove init success. */
static I2C_HandleTypeDef *hi2c = NULL;

/* DMA receives seven high-byte/low-byte pairs, starting at register 0x3B. */
static uint8_t DmaBuffer[14] __attribute__((aligned(4)));

/* Each row stores AX, AY, AZ, temperature, GX, GY, GZ as raw signed values. */
static int16_t raw_data[2][7];
static volatile uint8_t isready = 0;
static volatile uint8_t active_buf_idx = 0;  /* Row to fill on the next completion. */

/* Write one register, with up to three attempts.
 * This legacy interface does not report a write failure to its caller.
 */
static void reg_write(uint8_t reg, uint8_t value)
{
    if (hi2c == NULL) {
        return;
    }

    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Mem_Write(hi2c, MPU6050_Addr, reg,
                              I2C_MEMADD_SIZE_8BIT, &value, 1,
                              SENSOR_I2C_TIMEOUT) == HAL_OK) {
            return;
        }
        HAL_Delay(1);
    }
}

/* Read one register after the bus handle has been bound.
 * Read failures are not reported separately from the returned byte.
 */
static uint8_t reg_read(uint8_t reg)
{
    uint8_t result = 0;

    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Mem_Read(hi2c, MPU6050_Addr, reg,
                             I2C_MEMADD_SIZE_8BIT, &result, 1,
                             SENSOR_I2C_TIMEOUT) == HAL_OK) {
            return result;
        }
        HAL_Delay(1);
    }
    return result;
}

/* Bind the bus, check identity, reset, wake, and select measurement ranges.
 * Call from normal code with an initialized, valid I2C handle.
 * Returning 1 means the identity check passed and the writes were attempted;
 * reg_write does not let this function confirm that all writes succeeded.
 */
uint8_t MPU6050_Init(I2C_HandleTypeDef *handle)
{
    uint8_t chip_dev = 0;

    hi2c = handle;
    chip_dev = reg_read(0x75);      /* WHO_AM_I: expected identity is 0x68. */
    if (chip_dev != 0x68) {
        return 0;
    }

    reg_write(0x6B, 0x80);         /* PWR_MGMT_1: request device reset. */
    HAL_Delay(100);                /* Allow time for reset before configuring. */
    reg_write(0x6B, 0x00);         /* Clear SLEEP; CLKSEL = 0, internal clock. */
    reg_write(0x1B, 0x18);         /* GYRO_CONFIG: FS_SEL = 3, +/-2000 deg/s. */
    reg_write(0x1C, 0x10);         /* ACCEL_CONFIG: AFS_SEL = 2, +/-8 g. */
    return 1;
}

/* Start one DMA burst; returning does not mean the sample is available yet.
 * The original guard and ignored start status are retained for reference.
 * A working DMA/IRQ/completion path is required before using this operation.
 */
void MPU6050_readbegin(void)
{
    if (hi2c == NULL || hi2c->State == HAL_I2C_STATE_BUSY) {
        return;
    }

    HAL_I2C_Mem_Read_DMA(hi2c, MPU6050_Addr, 0x3B,
                         I2C_MEMADD_SIZE_8BIT, DmaBuffer, 14);
}

/* Completion callback shown explicitly for reading the legacy DMA flow.
 * HAL_I2C_Mem_Read_DMA uses HAL_I2C_MemRxCpltCallback, not MasterRxCpltCallback.
 * Merge the device-specific branch into the application's shared callback
 * when porting this driver; do not add a second definition of that callback.
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2cPar)
{
    if (hi2cPar == hi2c) {
        // Decode high-byte/low-byte pairs in the order defined by the device.
        raw_data[active_buf_idx][0] = ((int16_t)DmaBuffer[0] << 8) | DmaBuffer[1];   // AX
        raw_data[active_buf_idx][1] = ((int16_t)DmaBuffer[2] << 8) | DmaBuffer[3];   // AY
        raw_data[active_buf_idx][2] = ((int16_t)DmaBuffer[4] << 8) | DmaBuffer[5];   // AZ
        raw_data[active_buf_idx][3] = ((int16_t)DmaBuffer[6] << 8) | DmaBuffer[7];   // Temperature
        raw_data[active_buf_idx][4] = ((int16_t)DmaBuffer[8] << 8) | DmaBuffer[9];   // GX
        raw_data[active_buf_idx][5] = ((int16_t)DmaBuffer[10] << 8) | DmaBuffer[11]; // GY
        raw_data[active_buf_idx][6] = ((int16_t)DmaBuffer[12] << 8) | DmaBuffer[13]; // GZ

        // Publish the completed row, then use the other row on the next completion.
        active_buf_idx = !active_buf_idx;
        isready = 1;
    }
}

/* Copy the most recently completed row into seven valid output pointers.
 * Values remain raw: at the configured ranges, acceleration uses 4096 LSB/g
 * and angular velocity uses 16.4 LSB/(deg/s). Temperature is also a raw value.
 * Call after isReady reports a sample, before later completions overwrite it.
 * Two rows and volatile flags do not by themselves guarantee a concurrent snapshot.
 */
void MPU6050_getData(int16_t *ax, int16_t *ay, int16_t *az, int16_t *temp,
                     int16_t *gx, int16_t *gy, int16_t *gz)
{
    if (hi2c == NULL) {
        return;
    }

    uint8_t read_idx = 0;
    if (active_buf_idx == 0) {
        read_idx = 1;              /* The callback has switched away from this row. */
    }

    *ax = raw_data[read_idx][0];
    *ay = raw_data[read_idx][1];
    *az = raw_data[read_idx][2];
    *temp = raw_data[read_idx][3];
    *gx = raw_data[read_idx][4];
    *gy = raw_data[read_idx][5];
    *gz = raw_data[read_idx][6];
    isready = 0;                   /* Mark this sample as consumed. */
}

/* Return the sample-ready flag; this is not the sensor's hardware status register. */
uint8_t MPU6050_isReady(void)
{
    return isready;
}
#endif /* 已弃用：不参与当前固件编译。 */
