#include "MyIIC_HW.h"

/* 硬件 I2C 事件等待超时（防止总线卡死时死循环）。
   注意：本库会被 10kHz 电流环中断调用，超时值不能大——每次循环约十几周期，
   100000 相当于 18ms，一旦总线异常就会把中断堵死、系统卡顿（实测过）。
   3000 约 0.5ms，远大于 400kHz 下单字节事件的正常耗时(约 20us)。 */
#define I2C_HW_TIMEOUT 3000

/*
 * @brief  初始化硬件 I2C3
 * @param  无
 * @retval 无
 */
void MyIIC_HW_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef I2C_InitStructure;

    /* 使能 GPIO 与 I2C3 时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C3, ENABLE);

    /* SCL = PA8 (I2C3_SCL, AF4) */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;       // I2C 必须开漏
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;         // 上拉
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_I2C3); // AF4

    /* SDA = PB4 (I2C3_SDA, AF9)。PB4 默认是 JTAG NJTRST，配置 AF9 后自动释放 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF9_I2C3); // AF9

    /* I2C3 配置：快速模式 400kHz */
    I2C_InitStructure.I2C_ClockSpeed = 400000;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C3, &I2C_InitStructure);

    I2C_Cmd(I2C3, ENABLE);
}

/*
 * @brief  写单字节：START -> 器件地址(写) -> 寄存器地址 -> 数据 -> STOP
 * @retval 0 成功 / 1 失败（超时）
 */
uint8_t MyIIC_HW_Write_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address, uint8_t REG_data)
{
    uint32_t timeout = 0;

    I2C_GenerateSTART(I2C3, ENABLE);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_MODE_SELECT)) // EV5
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_Send7bitAddress(I2C3, SlaveAddress << 1, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) // EV6
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_SendData(I2C3, REG_Address);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) // EV8
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_SendData(I2C3, REG_data);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) // EV8_2
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_GenerateSTOP(I2C3, ENABLE);
    return 0;
}

/*
 * @brief  读单字节：START -> 器件地址(写) -> 寄存器地址 -> 重复START -> 器件地址(读) -> 数据(NACK) -> STOP
 * @retval 读出的数据
 */
uint8_t MyIIC_HW_Read_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address)
{
    uint32_t timeout = 0;
    uint8_t data;

    I2C_GenerateSTART(I2C3, ENABLE);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_MODE_SELECT)) // EV5
        if (++timeout > I2C_HW_TIMEOUT) return 0;

    I2C_Send7bitAddress(I2C3, SlaveAddress << 1, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) // EV6
        if (++timeout > I2C_HW_TIMEOUT) return 0;

    I2C_SendData(I2C3, REG_Address);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) // EV8
        if (++timeout > I2C_HW_TIMEOUT) return 0;

    /* 重复起始，切换为读方向 */
    I2C_GenerateSTART(I2C3, ENABLE);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_MODE_SELECT)) // EV5
        if (++timeout > I2C_HW_TIMEOUT) return 0;

    I2C_Send7bitAddress(I2C3, SlaveAddress << 1, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) // EV6
        if (++timeout > I2C_HW_TIMEOUT) return 0;

    /* 单字节读取：先发 NACK */
    I2C_AcknowledgeConfig(I2C3, DISABLE);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_RECEIVED)) // EV7
        if (++timeout > I2C_HW_TIMEOUT) return 0;
    data = I2C_ReceiveData(I2C3);
    I2C_AcknowledgeConfig(I2C3, ENABLE);

    I2C_GenerateSTOP(I2C3, ENABLE);
    return data;
}

/*
 * @brief  写多字节
 * @retval 0 成功 / 1 失败（超时）
 */
uint8_t MyIIC_HW_Write_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
    uint32_t timeout = 0;
    uint8_t i;

    I2C_GenerateSTART(I2C3, ENABLE);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_MODE_SELECT))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_Send7bitAddress(I2C3, DeviceAddr << 1, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_SendData(I2C3, REG_Address);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    for (i = 0; i < BytesNum; i++)
    {
        I2C_SendData(I2C3, buf[i]);
        while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
            if (++timeout > I2C_HW_TIMEOUT) return 1;
    }

    I2C_GenerateSTOP(I2C3, ENABLE);
    return 0;
}

/*
 * @brief  读多字节
 * @retval 0 成功 / 1 失败（超时）
 */
uint8_t MyIIC_HW_Read_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
    uint32_t timeout = 0;
    uint8_t i;

    I2C_GenerateSTART(I2C3, ENABLE);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_MODE_SELECT))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_Send7bitAddress(I2C3, DeviceAddr << 1, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_SendData(I2C3, REG_Address);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    /* 重复起始，切换为读方向 */
    I2C_GenerateSTART(I2C3, ENABLE);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_MODE_SELECT))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    I2C_Send7bitAddress(I2C3, DeviceAddr << 1, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
        if (++timeout > I2C_HW_TIMEOUT) return 1;

    for (i = 0; i < BytesNum; i++)
    {
        /* 最后一个字节前发 NACK */
        if (i == BytesNum - 1)
            I2C_AcknowledgeConfig(I2C3, DISABLE);
        while (!I2C_CheckEvent(I2C3, I2C_EVENT_MASTER_BYTE_RECEIVED))
            if (++timeout > I2C_HW_TIMEOUT) return 1;
        buf[i] = I2C_ReceiveData(I2C3);
    }
    I2C_AcknowledgeConfig(I2C3, ENABLE);

    I2C_GenerateSTOP(I2C3, ENABLE);
    return 0;
}
