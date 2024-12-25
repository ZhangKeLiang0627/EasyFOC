#include "AS5047P.h"
#include "spi.h"

/**
 * @brief  使用SPI总线往AS5047P的寄存器中写入并读取两个字节数据（16bits）
 * @param  byte: 写入的数据
 * @retval 读出的数据
 */
uint16_t AS5047P_ReadWriteByte(uint16_t byte)
{
    return SPI1_ReadWriteByte(byte);
}

/**
 * @brief  从AS5047P读出原始数据
 * @param  none
 * @retval 读出的原始数据
 */
uint16_t AS5047P_GetRawAngle(void)
{
    uint16_t data = 0;

    SPI_CS_Clr();

    data = AS5047P_ReadWriteByte(0Xffff);

    SPI_CS_Set();

    return data & 0x3fff;
}

/**
 * @brief AS5047P_Init
 * @brief 初始化芯片AS5047P
 * @param  无
 * @retval 0（成功），1（失败）
 */
uint8_t AS5047P_Init(void)
{
    /* init SPI interface */
    SPI1_Init();

    /* 片选CS拉高 */
    SPI_CS_Set();

    /* init param */
    AS5047P_GetRawAngle();

    return 0;
}
