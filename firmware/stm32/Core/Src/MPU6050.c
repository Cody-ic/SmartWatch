/* MPU6050 旧版驱动已弃用，当前项目使用 LSM6DSOX。
 * 保留供参考与教学使用，通过 #if 0 排除编译。 */
#if 0
#include"MPU6050.h"

/* 教学补加：0xd0 是七位设备地址 0x68 左移一位后的 HAL 传参值。 */
#define MPU6050_Addr 0xd0

/* I2C 超时（ms） */
#define SENSOR_I2C_TIMEOUT 10

//using English annotations is because Chinese will become meaningless characters

static I2C_HandleTypeDef* hi2c = NULL;//it is used to check if it has been initialized
/* 教学补加：从 0x3b 连读十四字节，依次为加速度 XYZ、温度、角速度 XYZ，各占两字节。 */
static uint8_t DmaBuffer[14]__attribute__((aligned(4)));
static int16_t raw_data[2][7];
static volatile uint8_t isready = 0;
static volatile uint8_t active_buf_idx = 0;


/* 教学补加：此写入函数不返回通信状态；初始化调用它后，无法据此确认每次写入是否成功。 */
static void reg_write(uint8_t reg, uint8_t value)
{
	if(hi2c == NULL) return;
	for (int retry = 0; retry < 3; retry++) {
		if (HAL_I2C_Mem_Write(hi2c, MPU6050_Addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, SENSOR_I2C_TIMEOUT) == HAL_OK)
			return;
		HAL_Delay(1);
	}
}

/* 教学补加：本接口只返回数据字节，不能单凭返回值区分有效数据与通信失败。 */
static uint8_t reg_read(uint8_t reg)
{
	uint8_t result = 0;
	for (int retry = 0; retry < 3; retry++) {
		if (HAL_I2C_Mem_Read(hi2c, MPU6050_Addr, reg, I2C_MEMADD_SIZE_8BIT, &result, 1, SENSOR_I2C_TIMEOUT) == HAL_OK)
			return result;
		HAL_Delay(1);
	}
	return result;
}

/* 教学补加：0x75 为身份寄存器；0x6b 控制复位和睡眠；0x1b、0x1c 设置量程。 */
uint8_t MPU6050_Init(I2C_HandleTypeDef* handle)
{
	uint8_t chip_dev = 0;
	hi2c = handle; //indicates that the initialization function has been called
	chip_dev = reg_read(0x75);
	if(chip_dev != 0x68){
		return 0;
	}
	reg_write(0x6b,0x80);
	HAL_Delay(100);
	reg_write(0x6b,0x00);
	/* 教学补加：原注释的角速度单位应读作 degrees/s，不是直接输出的角度。 */
	reg_write(0x1b,0x18);//range:-2000 degrees ~ 2000 degrees
	reg_write(0x1c,0x10);//range:-8g ~ 8g
	return 1;
}

void MPU6050_readbegin(void){
	if(hi2c == NULL || hi2c->State == HAL_I2C_STATE_BUSY) return;//if not initialized or it is busy, do not execute
	HAL_I2C_Mem_Read_DMA(hi2c, MPU6050_Addr, 0x3b, I2C_MEMADD_SIZE_8BIT, DmaBuffer, 14);
}

/* 教学补加：Mem_Read_DMA 对应 MemRxCpltCallback，故仅修正原回调名；
 * 展开此段便于阅读 DMA 完成流程，整个驱动仍由外层 #if 0 排除编译。
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef* hi2cPar){
	if(hi2cPar == hi2c) {
		raw_data[active_buf_idx][0] = ((int16_t)DmaBuffer[0] << 8) | DmaBuffer[1];
		raw_data[active_buf_idx][1] = ((int16_t)DmaBuffer[2] << 8) | DmaBuffer[3];
		raw_data[active_buf_idx][2] = ((int16_t)DmaBuffer[4] << 8) | DmaBuffer[5];
		raw_data[active_buf_idx][3] = ((int16_t)DmaBuffer[6] << 8) | DmaBuffer[7];
		raw_data[active_buf_idx][4] = ((int16_t)DmaBuffer[8] << 8) | DmaBuffer[9];
		raw_data[active_buf_idx][5] = ((int16_t)DmaBuffer[10] << 8) | DmaBuffer[11];
		raw_data[active_buf_idx][6] = ((int16_t)DmaBuffer[12] << 8) | DmaBuffer[13];
		active_buf_idx = !active_buf_idx;
		isready = 1;
	}
}

//Because AI only requires 16-bit of raw data, it obtains unprocessed 16-bit data
/* 教学补加：这里保留原始计数值；按当前量程，加速度为 4096 LSB/g，角速度为 16.4 LSB/(degrees/s)。
 * 两组数组不等于无限缓冲，读取应在后续采集覆盖该组数据之前完成。
 */
void MPU6050_getData(int16_t* ax, int16_t* ay, int16_t* az, int16_t* temp, int16_t* gx, int16_t* gy, int16_t* gz)
{
	if(hi2c == NULL) return;
	uint8_t read_idx = 0;
	if(active_buf_idx == 0) read_idx =1;//read inactive cache area only
	*ax = raw_data[read_idx][0];
	*ay = raw_data[read_idx][1];
	*az = raw_data[read_idx][2];
	*temp = raw_data[read_idx][3];
	*gx = raw_data[read_idx][4];
	*gy = raw_data[read_idx][5];
	*gz = raw_data[read_idx][6];
	isready = 0;
}

uint8_t MPU6050_isReady(void){
	return isready;
}
#endif /* 已弃用：不参与当前固件编译。 */
