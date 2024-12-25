#include "MyADC.h"
#include "delay.h"

/**
 * @brief MyADC_Init
 * @brief 初始化ADC
 * @paramm none
 * @retval none
 */
void MyADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_CommonInitTypeDef ADC_CommonInitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE); // 使能GPIOA时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE); // 使能GPIOC时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);  // 使能ADC1时钟

    // 初始化ADC1通道2 ---> PA2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;        // PA2 ---> ADC1_IN2(通道2)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;     // 模拟输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 不带上下拉
    GPIO_Init(GPIOA, &GPIO_InitStructure);           // 初始化

    // 初始化ADC1通道14 ---> PC4
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;        // PC4 ---> ADC1_IN14(通道14)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;     // 模拟输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 不带上下拉
    GPIO_Init(GPIOC, &GPIO_InitStructure);           // 初始化

    // 初始化ADC1通道15 ---> PC5
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;        // PC5 ---> ADC1_IN15(通道15)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;     // 模拟输入
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 不带上下拉
    GPIO_Init(GPIOC, &GPIO_InitStructure);           // 初始化

    // 进行复位校准 ResetCalibration
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, ENABLE);  // ADC1复位
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1, DISABLE); // 复位结束

    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;                     // 独立模式
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles; // 两个采样阶段之间的延迟5个时钟
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;      // DMA失能
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;                  // 预分频4频，ADCCLK=PCLK2/4=84/4=21Mhz，ADC时钟最好不要超过36Mhz
    ADC_CommonInit(&ADC_CommonInitStructure);                                    // 初始化

    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;                      // 12位模式
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;                               // 非扫描模式（单通道模式）
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;                         // 关闭连续转换（单次转换模式）
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; // 禁止触发检测，使用软件触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;                      // 数据右对齐
    ADC_InitStructure.ADC_NbrOfConversion = 1;                                  // 1个转换在规则序列中，也就是只转换规则序列1
    ADC_Init(ADC1, &ADC_InitStructure);                                         // ADC初始化

    ADC_Cmd(ADC1, ENABLE); // 开启AD转换器
}

/**
 * @brief MyADC_GetValue
 * @brief 获得ADC值
 * @paramm 0 ~ 16取值范围为：ADC_Channel_0 ~ ADC_Channel_16
 * @retval 转换结果 -> Value
 */
uint16_t MyADC_GetValue(uint8_t channel)
{
    // 设置指定ADC的规则组通道，一个序列，采样时间
    // ADC1, ADC通道, 480个周期, 提高采样时间可以提高精确度
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_480Cycles);

    // 使能指定的ADC1的软件转换启动功能
    ADC_SoftwareStartConv(ADC1);

    // 等待转换结束
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
        ;

    // 返回最近一次ADC1规则组的转换结果
    return ADC_GetConversionValue(ADC1);
}

/**
 * @brief MyADC_GetValue
 * @brief 获取通道ch的转换值，获取times次，然后求平均值
 * @paramm channel:通道编号 & times:获取次数
 * @retval 通道ch的times次转换结果平均值
 */
uint16_t MyADC_GetValue_Average(uint8_t channel, uint8_t times)
{
    uint32_t temp_val = 0;
    uint8_t t = 0;
    for (t = 0; t < times; t++)
    {
        temp_val += MyADC_GetValue(channel);
        delay_ms(5);
    }
    return temp_val / times;
}

/**
 * @brief getBetteryVolt
 * @brief 获取当前电池电压值（总电压12.6V）
 * @paramm none
 * @retval 返回测量所得的电压值
 */
float getBetteryVolt(void)
{
    uint16_t raw = MyADC_GetValue(ADC_BATTERY);

    float val = (3.3f / 4096) * (raw + 100); // 这里 +100 的操作是为了弥补ADC检测与现实计算的误差

    return val;
}

/**
 * @brief analogRead
 * @brief 获得ADC值，在FOC电流采样中使用
 * @paramm 0 ~ 16取值范围为：ADC_Channel_0 ~ ADC_Channel_16
 * @retval 转换结果 -> Value
 */
unsigned short analogRead(unsigned char channel)
{
    // 设置指定ADC的规则组通道，一个序列，采样时间
    // ADC1, ADC通道, 15个周期（提高采样时间可以提高精确度，但采样时间越低采样速率越快
    ADC_RegularChannelConfig(ADC1, channel, 1, ADC_SampleTime_15Cycles);

    // 使能指定的ADC1的软件转换启动功能
    ADC_SoftwareStartConv(ADC1);

    // 等待转换结束
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC))
        ;

    // 返回最近一次ADC1规则组的转换结果
    return ADC_GetConversionValue(ADC1);
}

// function reading an ADC value and returning the read voltage
float _readADCVoltageInline(unsigned char ch)
{
    unsigned short raw_adc = analogRead(ch);
    return (float)raw_adc * 3.3f / 4096;
}
