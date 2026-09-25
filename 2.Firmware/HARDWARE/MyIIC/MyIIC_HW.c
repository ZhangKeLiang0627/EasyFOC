#include "MyIIC_HW.h"

/* 单次事件等待的超时次数（每次循环约十几周期，2000 ≈ 0.3ms）。
   400kHz 下单字节事件约 20us，余量足够；主要用于防止总线异常时死等。 */
#define I2C_HW_TIMEOUT 2000

/* 一次性初始化标志：反复重配 PA8/PB4 的 AF 与外设会在总线上引入瞬态，
   使紧随其后的第一笔事务必失败（每次重发 S0 都读不通的原因）。配置不变就只初始化一次。 */
static uint8_t MyIIC_HW_Inited = 0;

/*
 * @brief  出错恢复：整外设复位 + 重新初始化
 *         只用 SWRST/清标志不够：CR1 里挂起的位清不掉，会让后续 START 全被忽略
 * @param  无
 * @retval 无
 */
static void MyIIC_HW_ErrorRecover(void)
{
    MyIIC_HW_Inited = 0;
    I2C_DeInit(I2C3);
    MyIIC_HW_Init();
}

/*
 * @brief  事务开始前检查总线：上次操作若把它留在 BUSY，先恢复
 * @param  无
 * @retval 无
 */
static void MyIIC_HW_CheckIdle(void)
{
    if (I2C_GetFlagStatus(I2C3, I2C_FLAG_BUSY))
        MyIIC_HW_ErrorRecover();
}

/*
 * @brief  等事件：每次等待独立计时；超时先恢复总线再返回失败
 * @param  stage 失败时返回该编号，便于定位卡在第几步
 * @retval 0 成功 / stage 失败
 */
static uint8_t MyIIC_HW_WaitEvent(uint32_t event, uint8_t stage)
{
    uint32_t timeout = 0;

    while (!I2C_CheckEvent(I2C3, event))
    {
        if (++timeout > I2C_HW_TIMEOUT)
        {
            MyIIC_HW_ErrorRecover();
            return stage;
        }
    }

    return 0;
}

/*
 * @brief  初始化硬件 I2C3（首次调用才真正配置，之后为空操作）
 * @param  无
 * @retval 无
 */
void MyIIC_HW_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef I2C_InitStructure;

    if (MyIIC_HW_Inited)
        return;

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

    /* I2C3 配置：快速模式 400kHz。
       必须先关 PE 再调用 I2C_Init：外设在 PE=1 时会忽略 CCR/TRISE 的写入，
       导致重复初始化时时钟配置根本没生效（重发 S0 后一直读不通的原因）。 */
    I2C_Cmd(I2C3, DISABLE);
    I2C_InitStructure.I2C_ClockSpeed = 400000;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C3, &I2C_InitStructure);

    I2C_Cmd(I2C3, ENABLE);
    MyIIC_HW_Inited = 1;
}

/*
 * @brief  写单字节：START -> 器件地址(写) -> 寄存器地址 -> 数据 -> STOP
 * @retval 0 成功 / 非 0 失败（值为卡住的事件编号）
 */
uint8_t MyIIC_HW_Write_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address, uint8_t REG_data)
{
    uint8_t st;

    MyIIC_HW_CheckIdle();

    I2C_GenerateSTART(I2C3, ENABLE);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, 1);
    if (st)
        return st;

    I2C_Send7bitAddress(I2C3, SlaveAddress << 1, I2C_Direction_Transmitter);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 2);
    if (st)
        return st;

    I2C_SendData(I2C3, REG_Address);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED, 3);
    if (st)
        return st;

    I2C_SendData(I2C3, REG_data);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED, 4);
    if (st)
        return st;

    I2C_GenerateSTOP(I2C3, ENABLE);
    return 0;
}

/*
 * @brief  读单字节：START -> 器件地址(写) -> 寄存器地址 -> 重复START -> 器件地址(读) -> 数据(NACK) -> STOP
 * @retval 读出的数据（失败返回 0）
 */
uint8_t MyIIC_HW_Read_SingleByte(uint8_t SlaveAddress, uint8_t REG_Address)
{
    uint8_t st;
    uint8_t data;

    MyIIC_HW_CheckIdle();

    I2C_GenerateSTART(I2C3, ENABLE);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, 1);
    if (st)
        return 0;

    I2C_Send7bitAddress(I2C3, SlaveAddress << 1, I2C_Direction_Transmitter);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 2);
    if (st)
        return 0;

    I2C_SendData(I2C3, REG_Address);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED, 3);
    if (st)
        return 0;

    I2C_GenerateSTART(I2C3, ENABLE);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, 4);
    if (st)
        return 0;

    I2C_Send7bitAddress(I2C3, SlaveAddress << 1, I2C_Direction_Receiver);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 5);
    if (st)
        return 0;

    I2C_AcknowledgeConfig(I2C3, DISABLE);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED, 6);
    if (st)
        return 0;
    data = I2C_ReceiveData(I2C3);
    I2C_AcknowledgeConfig(I2C3, ENABLE);

    I2C_GenerateSTOP(I2C3, ENABLE);
    return data;
}

/*
 * @brief  写多字节
 * @retval 0 成功 / 非 0 失败（值为卡住的事件编号）
 */
uint8_t MyIIC_HW_Write_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
    uint8_t st;
    uint8_t i;

    MyIIC_HW_CheckIdle();

    I2C_GenerateSTART(I2C3, ENABLE);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, 1);
    if (st)
        return st;

    I2C_Send7bitAddress(I2C3, DeviceAddr << 1, I2C_Direction_Transmitter);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 2);
    if (st)
        return st;

    I2C_SendData(I2C3, REG_Address);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED, 3);
    if (st)
        return st;

    for (i = 0; i < BytesNum; i++)
    {
        I2C_SendData(I2C3, buf[i]);
        st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED, 4);
        if (st)
            return st;
    }

    I2C_GenerateSTOP(I2C3, ENABLE);
    return 0;
}

/*
 * @brief  读多字节
 * @retval 0 成功 / 非 0 失败（值为卡住的事件编号）
 */
uint8_t MyIIC_HW_Read_MultiBytes(uint8_t DeviceAddr, uint8_t REG_Address, uint8_t BytesNum, uint8_t *buf)
{
    uint8_t st;
    uint8_t i;

    MyIIC_HW_CheckIdle();

    I2C_GenerateSTART(I2C3, ENABLE);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, 1);
    if (st)
        return st;

    I2C_Send7bitAddress(I2C3, DeviceAddr << 1, I2C_Direction_Transmitter);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, 2);
    if (st)
        return st;

    I2C_SendData(I2C3, REG_Address);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED, 3);
    if (st)
        return st;

    /* 重复起始，切换为读方向 */
    I2C_GenerateSTART(I2C3, ENABLE);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, 4);
    if (st)
        return st;

    I2C_Send7bitAddress(I2C3, DeviceAddr << 1, I2C_Direction_Receiver);
    st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, 5);
    if (st)
        return st;

    for (i = 0; i < BytesNum; i++)
    {
        /* 最后一个字节前发 NACK */
        if (i == BytesNum - 1)
            I2C_AcknowledgeConfig(I2C3, DISABLE);
        st = MyIIC_HW_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED, 6);
        if (st)
            return st;
        buf[i] = I2C_ReceiveData(I2C3);
    }
    I2C_AcknowledgeConfig(I2C3, ENABLE);

    I2C_GenerateSTOP(I2C3, ENABLE);
    return 0;
}
