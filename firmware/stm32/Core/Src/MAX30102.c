#include "MAX30102.h"


#define TEST2 0


// Static variables, aligned with LSM6DSOX driver style
static I2C_HandleTypeDef *hi2c = NULL;
static uint8_t FifoBuffer[32 * 6] __attribute__((aligned(4)));


#define MAX30102_I2C_ADDR 0xAE  /* 0x57 << 1: HAL 瑕佹眰 7-bit 鍦板潃宸︾Щ鍚庝紶鍏ワ紝鍐呴儴涓嶅啀绉讳綅 */
#define MAX30102_FIFO_DEPTH 32

/* I2C 瓒呮椂锛坢s锛夛細鏈夐檺瓒呮椂闃叉 I2C 鎬荤嚎鍗℃鏃朵换鍔℃案涔呮寕璧?*/
#define SENSOR_I2C_TIMEOUT      10
#define SENSOR_I2C_FIFO_TIMEOUT 100

static uint8_t reg_write(uint8_t reg, uint8_t value)
{
    if(hi2c == NULL) return 0;
    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Mem_Write(hi2c, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, SENSOR_I2C_TIMEOUT) == HAL_OK)
            return 1;
        HAL_Delay(1);
    }
    return 0;
}

static uint8_t reg_read(uint8_t reg, uint8_t *value)
{
    if(hi2c == NULL || value == NULL) return 0;
    for (int retry = 0; retry < 3; retry++) {
        if (HAL_I2C_Mem_Read(hi2c, MAX30102_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 1, SENSOR_I2C_TIMEOUT) == HAL_OK)
            return 1;
        HAL_Delay(1);
    }
    return 0;
}

// Sensor initialization function (100Hz ODR, for dual FIFO synchronization with LSM6DSOX)
// Return value: 0=Success, 1=NULL I2C handle, 2=Chip ID verification failed
// 3=Software reset failed, 4=FIFO configuration failed, 5=Sampling & interrupt configuration failed
uint8_t MAX30102_Init(I2C_HandleTypeDef *handle)
{
    uint8_t chip_id = 0;
    uint8_t reg_val = 0;
    hi2c = handle;

    // 1. Validate I2C handle availability
    if(hi2c == NULL) return 1;

    // 2. Verify chip ID with retry (improve I2C robustness)
    // MAX30102 fixed PART ID register value is 0x15
    uint8_t id_retry = 3;
    while(id_retry--)
    {
        if(reg_read(0xFF, &chip_id) && chip_id == 0x15) break;
        HAL_Delay(1);
    }
    if(chip_id != 0x15) return 2;

    // 3. Software reset (FIFO pointers auto-cleared by hardware after reset)
    // Trigger reset: set RESET bit (bit6) of MODE register
    if(!reg_write(0x09, 0x40)) return 3;
    // Poll reset bit until it is auto-cleared by hardware
    uint32_t timeout = 100;
    while(timeout--)
    {
        if(reg_read(0x09, &reg_val))
        {
            if((reg_val & 0x40) == 0) break;
        }
        HAL_Delay(1);
    }
    if(timeout == 0) return 3;

    // 4. Set MODE=000 (idle, no sampling) before configuration
    // NOTE: This does NOT set SHDN (bit7=0). To truly power down, write 0x80.
    if(!reg_write(0x09, 0x00)) return 5;

    // 5. FIFO core configuration (aligned with LSM6DSOX 26-sample watermark)
    // FIFO_CONFIG register: enable rollover mode
    // bit4=1: enable FIFO rollover, new data overwrites old data when full, prevent lockup
    // bit2-0=000: FIFO almost full threshold = 0 (full 32 samples), reserved for debug
    if(!reg_write(0x08, 0x10)) return 4;

    // 6. SpO2 sampling core configuration (100Hz ODR, matched with LSM6DSOX sync scheme)
    // SpO2_CONFIG register:
    // Uncomment the option you need, and keep only one line
    if(!reg_write(0x0A, 0x27)) return 5; // Match 4096nA annotation

    if(!reg_write(0x0C, 0x1F)) return 5; // LED1_PA: Red LED current
    if(!reg_write(0x0D, 0x1F)) return 5; // LED2_PA: IR LED current

    // Set operating mode: SpO2 mode (Red + IR dual LED synchronous sampling)
    if(!reg_write(0x09, 0x03)) return 5;

    // 7. Soft sync alignment with LSM6DSOX
    // Wait 1 full 100Hz sampling cycle (10ms) to ensure sampling start alignment
    // Note: This is software sync with ms-level error. For us-level sync, use LSM6DSOX INT pin to trigger read
    HAL_Delay(10);

    // 8. [Core Sync Design] Disable all interrupts
    // Only use LSM6DSOX FIFO watermark interrupt as the sole MCU wake-up source
    // 0x00 = disable all interrupts, avoid frequent MCU wake-up, achieve ultra-low power consumption
    if(!reg_write(0x02, 0x00)) return 5;

    return 0;
}

/* [2026-07-04]: 鍔ㄦ€侀噰鏍风巼 鈥?MLC 闈欐鈫?0Hz, 杩愬姩鈫?00Hz.
 *   SpO2_CONFIG(0x0A): SR[4:2]=001鈫?00Hz, 000鈫?0Hz.
 *   LED_PW=11(411渭s), ADC_RGE=01(4096nA) 淇濈暀涓嶅彉. */
uint8_t MAX30102_SetSampleRate(uint8_t hi_rate)
{
    /* [2026-07-04 Fix]: 50Hz 鍊间粠 0x07 鏀逛负 0x23銆?     *   0x07 = ADC_RGE=00(2048nA) + SR=001(100Hz) 鈥?涓や釜閿欒:
     *   鈶犻噰鏍风巼瀹為檯浠嶄负 100Hz (SR=001), 鈶DC 閲忕▼鎰忓鍑忓崐涓?2048nA銆?     *   0x23 = ADC_RGE=01(4096nA) + SR=000(50Hz) + PW=11(411渭s) 鈥?姝ｇ‘銆?     *   浜ゅ弶楠岃瘉: MAX30102 DS p18-19 + Arduino RMW + Linux 瀵勫瓨鍣ㄥ竷灞€ */
    uint8_t val = hi_rate ? 0x27 : 0x23;  /* 100Hz / 50Hz */
    if(!reg_write(0x0A, val)) return 1;
    return 0;
}

uint8_t MAX30102_GetFifoBatch(uint32_t *red_buf, uint32_t *ir_buf, uint8_t max_samples)
{
   if(hi2c == NULL || red_buf == NULL || ir_buf == NULL) return 0;
   uint8_t w_ptr = 0, r_ptr = 0;
   if(!reg_read(0x04,&w_ptr)||!reg_read(0x06,&r_ptr)){
	   return 0;
   }
   /* [2026-07-04 Fix]: 绉婚櫎 & 0x1F 鎺╃爜銆?    *   鍘熷叕寮?(w-r+32)&0x1F 鍦?FIFO 婊?(32鏉? WR=RD) 鏃跺皢 32 鍒悕鍖栦负 0,
    *   瀵艰嚧婊?FIFO 琚鍒や负绌? 闈欓粯涓㈠純鍏ㄩ儴 PPG 鏁版嵁銆?    *   浜ゅ弶楠岃瘉: MAX30102 DS p13-16 + Linux max30102.c + Arduino MAX30105.cpp
    *   涓夋簮鍧囩‘璁?5-bit 鎸囬拡鐨勬弧/绌烘涔夋槸纭欢闄愬埗, & 0x1F 浣垮叾鎭跺寲銆?*/
   /* [2026-07-06 Fix]: w-r+32 在 w>r 时多算32 (如5->37 clamps到25但只5可用),
    *   半满时读到垃圾。int8_t 带符号差+OVF(0x05) 区分空(0)还是满(32)。
    *   5场景手算验证: w>r用diff, w<r用diff+32, w==r查OVF。 */
   int8_t diff = (int8_t)(w_ptr - r_ptr);
   uint8_t data_ready_num;
   if (diff > 0) {
       data_ready_num = (uint8_t)diff;
   } else if (diff < 0) {
       data_ready_num = (uint8_t)(diff + 32);
   } else {
       uint8_t ovf = 0;
       reg_read(0x05, &ovf);
       data_ready_num = (ovf > 0) ? 32 : 0;
   }
   if(data_ready_num == 0) return 0;
   uint8_t read_actul = data_ready_num;
   if(read_actul > max_samples) read_actul = max_samples;
   if(read_actul > MAX30102_FIFO_DEPTH) read_actul = MAX30102_FIFO_DEPTH;
   if(HAL_I2C_Mem_Read(hi2c, MAX30102_I2C_ADDR, 0x07, I2C_MEMADD_SIZE_8BIT, FifoBuffer, read_actul * 6, SENSOR_I2C_FIFO_TIMEOUT) != HAL_OK){
	   return 0;
   }
   for(uint8_t i = 0; i < read_actul; i++)
       {
           uint8_t *ptr = &FifoBuffer[i * 6];
           /* MAX30102 FIFO 18-bit left-justified 24-bit format (Maxim official MAXREFDES117):
            *   Each channel = 3 bytes: B0[23:16], B1[15:8], B2[7:0]
            *   ADC data occupies bits [17:0], upper 6 bits [23:18] are zero
            *   Canonical: ((B0<<16) | (B1<<8) | B2) & 0x3FFFF */
           red_buf[i] = (((uint32_t)ptr[0] << 16) | ((uint32_t)ptr[1] << 8) | ptr[2]) & 0x3FFFF;
           ir_buf[i]  = (((uint32_t)ptr[3] << 16) | ((uint32_t)ptr[4] << 8) | ptr[5]) & 0x3FFFF;
       }
       return read_actul;
}


#if TEST2
// Debug function: read single sample, can be removed after main process is stable
void MAX30102_GetSingleSample(uint32_t *red, uint32_t *ir)
{
    uint8_t buf[6] = {0};
    if(hi2c == NULL || red == NULL || ir == NULL) return;

    if(HAL_I2C_Mem_Read(hi2c, MAX30102_I2C_ADDR, 0x07, I2C_MEMADD_SIZE_8BIT, buf, 6, SENSOR_I2C_TIMEOUT) != HAL_OK)
    {
        return;
    }

    /* MAX30102 18-bit left-justified format (Maxim official) */
    *red = (((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2]) & 0x3FFFF;
    *ir  = (((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5]) & 0x3FFFF;
}

#endif
