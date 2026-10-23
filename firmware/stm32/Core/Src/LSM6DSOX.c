#include "LSM6DSOX.h"
#include "mlc/lsm6dsox_wrist_mlc.h"

#define TEST 0

static I2C_HandleTypeDef *hi2c = NULL;
static uint8_t sensor_initialized = 0;
static uint8_t mlc_loaded = 0;


/*
// FIFO batch read buffer, 32 samples × 15 bytes (1 byte TAG + 14 bytes valid data)
static uint8_t FifoBuffer[32 * 15] __attribute__((aligned(4)));
*/


#if TEST
// Debug dedicated
static uint8_t DmaSingleBuffer[14] __attribute__((aligned(4)));
static int16_t raw_data[2][7];
static volatile uint8_t data_ready = 0;
static volatile uint8_t active_buf_idx = 0;
#endif

// I2C address: SA0=GND → 7-bit=0x6A, HAL expects left-shifted → 0xD4
// [2026-07-02 FIX]: 原始 0x6A(未左移)→修正为 0xD4(0x6A<<1), HAL 内部不左移
#define LSM6DSOX_I2C_ADDR 0xD4

/* I2C 超时（ms）：有限超时防止 I2C 总线卡死时任务永久挂起 */
#define SENSOR_I2C_TIMEOUT      10
#define SENSOR_I2C_FIFO_TIMEOUT 100

static uint8_t reg_write(uint8_t reg, uint8_t value)
{
    if(hi2c == NULL) return 0;
    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Mem_Write(hi2c, LSM6DSOX_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, SENSOR_I2C_TIMEOUT) == HAL_OK)
            return 1;
        HAL_Delay(1);
    }
    return 0;
}

static uint8_t reg_read(uint8_t reg, uint8_t *value)
{
    if(hi2c == NULL || value == NULL) return 0;
    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Mem_Read(hi2c, LSM6DSOX_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 1, SENSOR_I2C_TIMEOUT) == HAL_OK)
            return 1;
        HAL_Delay(1);
    }
    return 0;
}

static uint8_t reg_read_mul(uint8_t reg_addr, uint8_t *buf, uint8_t len)
{
    if(hi2c == NULL || buf == NULL || len == 0) return 0;
    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Mem_Read(hi2c, LSM6DSOX_I2C_ADDR, reg_addr, I2C_MEMADD_SIZE_8BIT, buf, len, SENSOR_I2C_FIFO_TIMEOUT) == HAL_OK)
            return 1;
        HAL_Delay(1);
    }
    return 0;
}



#if TEST
// Debug dedicated: DMA single-sample read completion callback
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2cPar)
{
    if(hi2cPar == hi2c && hi2cPar->XferSize == 14)
    {
        // order:ax, ay, az, temp, gx, gy, gz
        raw_data[active_buf_idx][3] = ((int16_t)DmaSingleBuffer[1] << 8) | DmaSingleBuffer[0];
        raw_data[active_buf_idx][4] = ((int16_t)DmaSingleBuffer[3] << 8) | DmaSingleBuffer[2];
        raw_data[active_buf_idx][5] = ((int16_t)DmaSingleBuffer[5] << 8) | DmaSingleBuffer[4];
        raw_data[active_buf_idx][6] = ((int16_t)DmaSingleBuffer[7] << 8) | DmaSingleBuffer[6];
        raw_data[active_buf_idx][0] = ((int16_t)DmaSingleBuffer[9] << 8) | DmaSingleBuffer[8];
        raw_data[active_buf_idx][1] = ((int16_t)DmaSingleBuffer[11] << 8) | DmaSingleBuffer[10];
        raw_data[active_buf_idx][2] = ((int16_t)DmaSingleBuffer[13] << 8) | DmaSingleBuffer[12];

        active_buf_idx = !active_buf_idx;
        data_ready = 1;
    }
}
#endif



// Sensor initialization function (for dual FIFO synchronization with MAX30102)
// Return value: 0=Success, 1=NULL I2C handle, 2=Chip ID verification failed
// 3=Software reset failed, 4=Accelerometer configuration failed
// 5=Gyroscope configuration failed
// 6=FIFO configuration failed, 8=Interrupt pin configuration failed
uint8_t LSM6DSOX_Init(I2C_HandleTypeDef *handle)
{
    uint8_t chip_id = 0;
    uint8_t reg_val = 0;
    hi2c = handle;
    sensor_initialized = 0;
    mlc_loaded = 0;  /* Reinitialization requires reloading the decision tree. */

    // 1. Validate I2C handle availability
    if(hi2c == NULL) return 1;

    // Recover the user bank even if a previous MLC transfer lost the bus.
    if (!reg_write(0x01, 0x00)) return 2;

    // 2. Verify chip ID (LSM6DSOX fixed WHO_AM_I register value is 0x6C)
    if(!reg_read(0x0F, &chip_id) || chip_id != 0x6C) return 2;

    // 3. Software reset to restore default register state, wait for hardware reset completion
    if(!reg_write(0x12, 0x01)) return 3;
    uint32_t timeout = 100;
    uint8_t reset_success = 0;
    while(timeout--)
    {
        // Only check reset bit when I2C read is successful
        if(reg_read(0x12, &reg_val))
        {
            if((reg_val & 0x01) == 0)
            {
                reset_success = 1;
                break;
            }
        }
        HAL_Delay(1);
    }
    if(reset_success == 0) return 3;

    // 4. Accelerometer: 104Hz ODR, ±8g, ~52Hz bandwidth (ODR/2, LPF1_BW_SEL=0)
    // ODR[3:0] = 0100 → 104Hz, FS_XL[1:0] = 11 → ±8g
    if(!reg_write(0x10, 0x4C)) return 4;
    // CTRL8_XL: disable LPF2/HPF on accelerometer (no conflict with LPF1)
    if(!reg_write(0x17, 0x00)) return 4;

    // 5. Gyroscope: 104Hz ODR, ±2000dps, ~33Hz LPF1 (FTYPE=001 @ 104Hz)
    // ODR[3:0] = 0100 → 104Hz,  ±2000dps, high-pass filter disabled
    if(!reg_write(0x11, 0x4C)) return 5;
    // Configure LPF1 bandwidth to 33Hz @ 104Hz ODR (FTYPE=001)
    if(!reg_read(0x15, &reg_val)) return 5;
    reg_val &= ~0x07; // Clear FTYPE[2:0] bits
    reg_val |= 0x01;  // FTYPE[2:0] = 001 → bandwidth = 33Hz @ 104Hz ODR
    if(!reg_write(0x15, reg_val)) return 5;
    // Enable LPF1 on gyro signal path (CTRL4_C bit1 LPF1_SEL_G=1)
    // Without this, FTYPE setting has NO effect on gyro output
    if(!reg_read(0x13, &reg_val)) return 5;
    reg_val |= 0x02;  // LPF1_SEL_G = 1
    if(!reg_write(0x13, reg_val)) return 5;

    // 6. FIFO core configuration (fully compliant with ST official datasheet)
    // FIFO watermark configuration: trigger interrupt when 26 samples are ready (matches MAX30102 FIFO depth)
    if(!reg_write(0x07, FIFO_WATERMARK & 0xFF)) return 6; // FIFO_CTRL1: Watermark low 8 bits
    if(!reg_write(0x08, 0x00)) return 6;                   // FIFO_CTRL2: Watermark high 3 bits = 0
    // Accelerometer + Gyroscope FIFO batch rate: 104Hz, matches sensor ODR
    if(!reg_write(0x09, 0x44)) return 6;                   // FIFO_CTRL3: GYRO/XL batch rate = 104Hz
    // FIFO continuous mode: new data overwrites old when full
    // DEC_TS_BATCH=00 (no timestamp in FIFO), ODR_T_BATCH=00 (no temp in FIFO)
    // Only accelerometer + gyroscope data goes to FIFO
    if(!reg_write(0x0A, 0x06)) return 6;                   // FIFO_CTRL4: FIFO_MODE=110 → Continuous

    // 7. Interrupt pin full configuration
    // CTRL3_C register: BDU=1 (block data update, bit6), IF_INC=1 (auto-increment, bit2),
    //   PP_OD=0 (push-pull, bit4), H_LACTIVE=0 (active high, bit5)
    if(!reg_read(0x12, &reg_val)) return 7;
    reg_val &= ~0x30;
    reg_val |= 0x44;
    if(!reg_write(0x12, reg_val)) return 7;
    // INT1_CTRL register: enable FIFO watermark interrupt
    // Per DS13305 Table 37/38: bit5=INT1_FIFO_FULL, bit4=INT1_FIFO_OVR, bit3=INT1_FIFO_TH
    // [2026-07-02 FIX]: 原始 0x40(bit6)→第一次误改 0x10(bit4=FIFO_OVR)→对照数据手册修正为 0x08(bit3=FIFO_TH)
    if(!reg_read(0x0D, &reg_val)) return 7;
    reg_val &= 0x00; // Clear all interrupt enable bits
    reg_val |= 0x08; // INT1_FIFO_TH (bit3): fire when FIFO >= watermark (26 samples)
    if(!reg_write(0x0D, reg_val)) return 7;

    sensor_initialized = 1;
    return 0;
}

uint8_t LSM6DSOX_MLC_Init(void)
{
    uint8_t bank = 0;
    mlc_loaded = 0;
    if (!sensor_initialized) return 1;

    /* Load only at startup. Bypass clears samples collected before upload. */
    if (!reg_write(0x01, 0x00) || !reg_write(0x0A, 0x00)) goto load_failed;

    for (uint32_t i = 0; i < sizeof(lsm6dsox_wrist_mlc) / sizeof(lsm6dsox_wrist_mlc[0]); i++) {
        uint8_t reg = lsm6dsox_wrist_mlc[i][0];
        uint8_t value = lsm6dsox_wrist_mlc[i][1];

        /* Same addresses mean different registers in each bank. Keep every
         * PAGE_VALUE write in order: they contain the features and tree nodes.
         * Suppress the reference model's MLC interrupt routing before writing
         * it, so INT1 remains the dedicated FIFO time base for IMU and PPG. */
        if ((bank == 0x00 && reg == 0x5E) || (bank == 0x80 && reg == 0x0D))
            value = 0x00;
        /* PAGE_VALUE auto-increments its destination. On a bus error the chip
         * may already have accepted the byte: retrying could shift the tree.
         * Abort that upload instead of retrying an ambiguous page write. */
        if (bank == 0x80 && reg == 0x09) {
            if (HAL_I2C_Mem_Write(hi2c, LSM6DSOX_I2C_ADDR, reg,
                    I2C_MEMADD_SIZE_8BIT, &value, 1, SENSOR_I2C_TIMEOUT) != HAL_OK)
                goto load_failed;
        } else if (!reg_write(reg, value)) goto load_failed;
        if (reg == 0x01) bank = value;
    }

    /* Reference model: accel 26 Hz / +/-8 g, gyro off, MLC 26 Hz.
     * AN5259 allows sensor ODR >= MLC ODR. Keep the project's fixed 104 Hz
     * accel/gyro and FIFO pairing; classification accuracy needs device tests. */
    if (!reg_write(0x10, 0x4C) || !reg_write(0x11, 0x4C) ||
        !reg_write(0x0A, 0x06)) goto load_failed;

    mlc_loaded = 1;
    return 0;

load_failed:
    /* Best effort after an I2C failure: close page writes, disable the partial
     * tree, restore user bank. The caller must not start acquisition on error. */
    (void)reg_write(0x01, 0x80);
    (void)reg_write(0x17, 0x00);
    (void)reg_write(0x05, 0x00);
    (void)reg_write(0x01, 0x00);
    sensor_initialized = 0;
    return 1;
}

uint8_t LSM6DSOX_MLC_GetResult(void)
{
    uint8_t result = MLC_UNKNOWN;
    if (!mlc_loaded) return MLC_UNKNOWN;

    if (!reg_write(0x01, 0x80)) {
        (void)reg_write(0x01, 0x00);
        return MLC_UNKNOWN;
    }
    uint8_t read_ok = reg_read(0x70, &result);  /* Embedded bank: MLC0_SRC. */
    if (!reg_write(0x01, 0x00)) {
        (void)reg_write(0x01, 0x00);
        return MLC_UNKNOWN;
    }
    if (!read_ok) return MLC_UNKNOWN;

    switch (result) {
    case MLC_OTHER:
    case MLC_WALKING:
    case MLC_RUNNING:
        return result;
    default:
        return MLC_UNKNOWN;  /* Includes power-on output 0 before classification. */
    }
}

uint8_t LSM6DSOX_WakeUp_Init(void)
{
    uint8_t value;
    if (!sensor_initialized) return 1;

    /* Separate threshold detector, not a decision tree. WK_THS=2 with
     * FS/64 at +/-8 g gives 250 mg; duration is one accelerometer period. */
    if (!reg_read(0x5B, &value) || !reg_write(0x5B, (value & 0x80) | 0x02)) return 1;
    if (!reg_read(0x5C, &value) || !reg_write(0x5C, value & 0x8F)) return 1;
    if (!reg_read(0x56, &value)) return 1;
    value = (value & ~0x10) | 0x41;  /* Slope path, LIR, INT_CLR_ON_READ. */
    if (!reg_write(0x56, value)) return 1;
    if (!reg_read(0x58, &value) || !reg_write(0x58, (value & ~0x60) | 0x80)) return 1;

    /* Do not route wake-up onto either interrupt pin. Poll the latched source
     * without changing CTRL8_XL, the raw acceleration or the MLC input path. */
    if (!reg_read(0x5E, &value) || !reg_write(0x5E, value & ~0x20)) return 1;
    if (!reg_read(0x5F, &value) || !reg_write(0x5F, value & ~0x20)) return 1;
    if (!reg_read(0x1B, &value)) return 1;  /* Clear events from initialization. */
    return 0;
}

uint8_t LSM6DSOX_WakeUp_GetResult(void)
{
    uint8_t value;
    if (!sensor_initialized || !reg_write(0x01, 0x00) ||
        !reg_read(0x1B, &value)) return 0xFF;
    return (value & 0x08) ? 1 : 0;
}

/* [2026-07-04]: 动态 ODR 切换 — hi_rate=1→104Hz(0100), hi_rate=0→26Hz(0010).
 *   CTRL1_XL(0x10)+CTRL2_G(0x11)+FIFO_CTRL3(0x09) 同步更新。DS13305: FIFO batch 必须与 ODR 一致。
 *   ⚠ [2026-07-11] MLC 静止降频已从 main.c 移除(降 26Hz 破坏 sleep_features/HAR 的固定采样率假设,
 *   见 memory driver-verify-20260711), 此函数现**无调用者、保留为备用 API**。重启用的前提: 必须同步
 *   调 FIFO_WATERMARK(现 52 假设 104Hz→250ms; 26Hz 会 1s INT1、PPG 溢出/不对齐)+ sleep_features
 *   (ACC_FS/PPG_FS)与 HAR 的采样率适配, 否则复活低功耗采样率冲突。 */
uint8_t LSM6DSOX_SetODR(uint8_t hi_rate)
{
    uint8_t odr_nibble = hi_rate ? 0x40 : 0x20;  /* 0100→104Hz, 0010→26Hz */
    uint8_t odr_val    = odr_nibble | 0x0C;        /* FS_XL=11±8g, FS_G=11±2000dps */
    uint8_t fifo_val   = hi_rate ? 0x44 : 0x22;    /* BDR_GY|BDR_XL 与 ODR 一致 */

    if(!reg_write(0x10, odr_val)) return 1;  /* CTRL1_XL */
    if(!reg_write(0x11, odr_val)) return 2;  /* CTRL2_G  */
    if(!reg_write(0x09, fifo_val)) return 3; /* FIFO_CTRL3 */

    return 0;
}

//  FIFO batch read

	uint8_t LSM6DSOX_GetFifoBatch(int16_t *a_buf, int16_t *g_buf, int16_t *t_buf, uint8_t max_samples)
	{
	    if(hi2c == NULL || a_buf == NULL || g_buf == NULL) return 0;

        /* A previous MLC read may have lost I2C while restoring the bank. */
        if (!reg_write(0x01, 0x00)) return 0;

	    uint8_t samples_read = 0;
	    int16_t cur_acc[3] = {0};
	    int16_t cur_gyr[3] = {0};
	    uint8_t has_acc = 0;
	    uint8_t has_gyr = 0;
	

	    uint8_t status[2] = {0};
	    if(!reg_read(0x3A, &status[0]) || !reg_read(0x3B, &status[1])) return 0;
	    uint16_t fifo_words = ((uint16_t)(status[1] & 0x03) << 8) | status[0];
	    if(fifo_words == 0) return 0;


	    while(fifo_words > 0 && samples_read < max_samples)
	    {
	        uint8_t tag;
	        if(!reg_read(0x78, &tag)) break;
	        uint8_t sensor = tag >> 3;  /* [2026-07-10 Fix]: 原 `tag & 0x1F` 错误。FIFO_DATA_OUT_TAG(0x78) 的 TAG_SENSOR[4:0] 在 bit[7:3](低3位=TAG_CNT[1:0]+PARITY),提取须 >>3。原式取低5位→XL(sensor=2)字节=0x10~0x17 永不等 case 0x01/0x02→全落 default→acc/gyr 不配对→samples_read 恒0→HAR 无 IMU 数据。四源: ST lsm6dsox_reg.h(tag_sensor:5 高位)+Linux st_lsm6dsx_buffer.c(switch tag>>3, ACC=0x02/GYRO=0x01)+ST 社区示例+DS12996 TAG 位表。case 值=tag_sensor 编号不变。手改勿重生成 */

	        switch(sensor)
	        {
	            case 0x02: // XL_NC_TAG
	            {
	            	if(fifo_words < 1) goto done;
	                uint8_t raw[6];
	                if(!reg_read_mul(0x79, raw, 6)) goto done;
	                cur_acc[0] = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
	                cur_acc[1] = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
	                cur_acc[2] = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);
	                has_acc = 1;
	                fifo_words -= 1;
	                break;
	            }

	            case 0x01: // GYRO_NC_TAG
	            {
	            	if(fifo_words < 1) goto done;
	                uint8_t raw[6];
	                if(!reg_read_mul(0x79, raw, 6)) goto done;
	                cur_gyr[0] = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
	                cur_gyr[1] = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
	                cur_gyr[2] = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);
	                has_gyr = 1;
	                fifo_words -= 1;
	                break;
	            }

	            case 0x03: // TEMPERATURE_TAG
	            {
	            	if(fifo_words < 1) goto done;
	                uint8_t raw[2];
	                if(!reg_read_mul(0x79, raw, 2)) goto done;
	                if(t_buf != NULL)
	                {
	                    t_buf[0] = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
	                }
	                fifo_words -= 1;
	                break;
	            }

	            case 0x04: // TIMESTAMP_TAG
	            {
	            	if(fifo_words < 1) goto done;
	                uint8_t dummy[3];

	                if(!reg_read_mul(0x79, dummy, 3)) goto done;
	                fifo_words -= 1;
	                break;
	            }

	            default:
	                // 未知标签，简单策略：读完6字节后重新查询FIFO状态，避免错位
	            {
	                uint8_t dummy[6];
	                reg_read_mul(0x79, dummy, 6);
	                // 重新读取 FIFO 状态
	                if(!reg_read(0x3A, &status[0]) || !reg_read(0x3B, &status[1])) goto done;
	                fifo_words = ((uint16_t)(status[1] & 0x03) << 8) | status[0];
	                break;
	            }
	        }
	        if(has_acc && has_gyr)
	        {
	            uint8_t idx = samples_read * 3;
	            a_buf[idx + 0] = cur_acc[0];
	            a_buf[idx + 1] = cur_acc[1];
	            a_buf[idx + 2] = cur_acc[2];
	            g_buf[idx + 0] = cur_gyr[0];
	            g_buf[idx + 1] = cur_gyr[1];
	            g_buf[idx + 2] = cur_gyr[2];
	            samples_read++;

	            has_acc = 0;
	            has_gyr = 0;
	        }
	    }

	done:
	    return samples_read;
	}


#if TEST
// Debug dedicated: Read single sensor data
void LSM6DSOX_GetSingleData(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz, int16_t *temp)
{
    uint8_t buf[14] = {0};
    if(hi2c == NULL) return;

    if(HAL_I2C_Mem_Read(hi2c, LSM6DSOX_I2C_ADDR, 0x20, I2C_MEMADD_SIZE_8BIT, buf, 14, SENSOR_I2C_TIMEOUT) != HAL_OK)
    {
        return;
    }

    *temp = ((int16_t)buf[1] << 8) | buf[0];
    *gx = ((int16_t)buf[3] << 8) | buf[2];
    *gy = ((int16_t)buf[5] << 8) | buf[4];
    *gz = ((int16_t)buf[7] << 8) | buf[6];
    *ax = ((int16_t)buf[9] << 8) | buf[8];
    *ay = ((int16_t)buf[11] << 8) | buf[10];
    *az = ((int16_t)buf[13] << 8) | buf[12];
}

// Debug dedicated: Start DMA single-sample read
void LSM6DSOX_ReadBegin_DMA(void)
{
    if(hi2c == NULL || hi2c->State != HAL_I2C_STATE_READY) return;
    HAL_I2C_Mem_Read_DMA(hi2c, LSM6DSOX_I2C_ADDR, 0x20, I2C_MEMADD_SIZE_8BIT, DmaSingleBuffer, 14);
}

// Debug dedicated: Check if DMA data is ready
uint8_t LSM6DSOX_IsDataReady(void)
{
    return data_ready;
}

// Debug dedicated: Clear data ready flag
void LSM6DSOX_ClearDataReadyFlag(void)
{
    data_ready = 0;
}

#endif

/* ============================================================
 * [2026-07-05]: 硬件 Free Fall 检测 — LSM6DSOX 片内 FREE_FALL
 *
 * 原理: 合加速度低于阈值且持续一定时间 → 触发 INT1 中断
 *   FREE_FALL (0x5D): FF_THS[2:0] = 阈值, FF_DUR[4:0] = 持续时间
 *   阈值: 312mg (FF_THS=3) — 自由落体检测
 *   窗口: 6 个 ODR 周期 @ 104Hz ≈ 57ms
 *
 * 参考: ST LSM6DSOX datasheet DS13305 — FREE_FALL @ 0x5D, MD1_CFG @ 0x5E
 *
 * 使用: init 后 INT1 在 free fall 时触发 → Sensor_Task 信号量
 *       主循环中 LSM6DSOX_IsFreeFall() 查询 FREE_FALL_EVENT 锁存
 * ============================================================ */
uint8_t LSM6DSOX_Fall_Init(void)
{
    uint8_t reg_val;

    /* 1. FREE_FALL (0x5D): FF_THS=3 (312mg), FF_DUR=6 (6 ODR ≈ 57ms)
     *    [2026-07-06 Fix]: 原地址 0x5E 错误 (MD1_CFG), 修正为 0x5D。
     *    [2026-07-11 Fix]: 0x66→0x33。位布局 FF_DUR[4:0]@bit[7:3]、FF_THS[2:0]@bit[2:0]:
     *      0x66 = 0b01100_110 → FF_DUR=12 / FF_THS=6(469mg), 与上行意图(312mg/6ODR)不符
     *      —— 2026-07-06 那次修复把 FF_THS/FF_DUR 两字段的位位置弄反了。
     *      正确: FF_DUR=6(00110) + FF_THS=3(011) = 0b00110_011 = 0x33。
     *    四源: ST 官方 lsm6dsox_reg.h bitfield(LE: ff_ths:3 @bit[2:0], ff_dur:5 @bit[7:3]) +
     *      DS13305 位表 + ST lsm6dsox_free_fall.c 示例(FF_DUR=6/FF_THS=3→0x33) + 手算。
     *    注: LSM6DSOX_Fall_Init 当前全工程无调用者(死代码), 此修为消除隐患。手改勿重生成。 */
    if (!reg_write(0x5D, 0x33)) return 1;

    /* 2. MD1_CFG (0x5E): INT1_FF=1, RMW 保留已有 INT1_WU 等位
     *    [2026-07-06 Fix]: 原地址 0x5B 错误 (WAKE_UP_THS), 修正为 0x5E。
     *    原代码写入 0x5B 将 WK_THS 从 0x02 污染为 0x12 (2.25g), wake-up 阈值被改动。 */
    if (!reg_read(0x5E, &reg_val)) return 2;
    reg_val |= 0x10;  /* bit4 = INT1_FF */
    if (!reg_write(0x5E, reg_val)) return 2;

    /* 3. TAP_CFG0 (0x56): LIR=1 → latch FREE_FALL event until read
     *    [2026-07-06 Fix]: 原硬写 0x01 覆盖 SLOPE_FDS，改为 RMW 保留已有设置。 */
    if (!reg_read(0x56, &reg_val)) return 3;
    reg_val |= 0x01;  /* bit0 = LIR */
    if (!reg_write(0x56, reg_val)) return 3;

    /* 4. FUNCTIONS_ENABLE (0x58): INTERRUPTS_ENABLE=1
     *    显式使能基础事件，不依赖 wake-up 初始化。 */
    if (!reg_read(0x58, &reg_val)) return 4;
    reg_val |= 0x80;  /* bit7 = INTERRUPTS_ENABLE */
    if (!reg_write(0x58, reg_val)) return 4;

    return 0;
}

/**
 * @brief 检查 FREE_FALL 事件 (锁存, 读后自动清除)
 * @return 1=free fall detected, 0=normal
 */
uint8_t LSM6DSOX_IsFreeFall(void)
{
    uint8_t reg_val;
    /* [2026-07-06 Fix]: 原地址 0x5E 错误 (MD1_CFG 配置寄存器), 修正为 0x1B (WAKE_UP_SRC)。
     *   原 bit6=0x40 错误 (INT1_SINGLE_TAP 路由位), 修正为 bit5=0x20 (FF_IA 事件标志)。
     *   交叉验证: DS13305 WAKE_UP_SRC @ 0x1B, bit5=FF_IA。 */
    if (!reg_read(0x1B, &reg_val)) return 0;
    return (reg_val & 0x20) ? 1 : 0;  /* bit5 = FF_IA */
}

