#include "AS5600.h"

/* 软/硬 I2C 二选一：通过 AS5600_USE_HW_I2C 宏切换，接口函数名统一映射为 IIC_* */
#if AS5600_USE_HW_I2C
#include "MyIIC_HW.h"
#define IIC_Init             MyIIC_HW_Init
#define IIC_Write_SingleByte MyIIC_HW_Write_SingleByte
#define IIC_Read_SingleByte  MyIIC_HW_Read_SingleByte
#define IIC_Write_MultiBytes MyIIC_HW_Write_MultiBytes
#define IIC_Read_MultiBytes  MyIIC_HW_Read_MultiBytes
#else
#include "MyIIC_SW.h"
#define IIC_Init             MyIIC_SW_Init
#define IIC_Write_SingleByte MyIIC_SW_Write_SingleByte
#define IIC_Read_SingleByte  MyIIC_SW_Read_SingleByte
#define IIC_Write_MultiBytes MyIIC_SW_Write_MultiBytes
#define IIC_Read_MultiBytes  MyIIC_SW_Read_MultiBytes
#endif

/**
 * @brief  使用IIC总线往AS5600的寄存器中写一字节数据
 * @param  addr: 寄存器的地址
 * @param  dat: 	待写入的数据
 * @retval None
 */
void AS5600_Write_Byte(uint8_t addr, uint8_t dat)
{
    IIC_Write_SingleByte(AS5600_IIC_ADDR, addr, dat);
}

/**
 * @brief  使用IIC总线往AS5600的寄存器中写多组数据
 * @param  REG_Address: 寄存器的地址
 * @param  BytesNum: 写入数据的字节数
 * @param  buf: 待写入的数据指针
 * @retval None
 */
void AS5600_Write_MultiBytes(uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
    IIC_Write_MultiBytes(AS5600_IIC_ADDR, REG_Address, BytesNum, buf);
}

/**
 * @brief  使用IIC总线从AS5600的寄存器中读一字节数据
 * @param  addr: 寄存器的地址
 * @retval 读出的一字节数据
 */
uint8_t AS5600_Read_Byte(uint8_t addr)
{
    return IIC_Read_SingleByte(AS5600_IIC_ADDR, addr);
}

/**
 * @brief  使用IIC总线从AS5600的寄存器中读一字节数据
 * @param  REG_Address: 寄存器的地址
 * @param  BytesNum: 写入数据的字节数
 * @param  buf: 待写入的数据指针
 * @retval 返回读出是否成功 0成功/1失败
 */
uint8_t AS5600_Read_MultiBytes(uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
    return IIC_Read_MultiBytes(AS5600_IIC_ADDR, REG_Address, BytesNum, buf);
}

/**
 * @brief AS5600_Init
 * @brief 初始化芯片AS5600
 * @param  无
 * @retval 0（成功），1（失败）
 */
uint8_t AS5600_Init(void)
{
    /* init i2c interface */
    IIC_Init();

    AS5600_GetRawAngle();

    return 0;
}

/* AS5600 read raw_angle */
uint16_t AS5600_GetRawAngle(void)
{
    uint16_t raw_angle;
    uint8_t buf[2] = {0};

    AS5600_Read_MultiBytes(AS5600_RAW_ANGLE_REGISTER1, 2, buf);

    raw_angle = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    return raw_angle;
}
