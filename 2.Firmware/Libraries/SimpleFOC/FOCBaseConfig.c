#include "FOCBaseConfig.h"
#include "FOCuser_Inc.h"
#include "led.h"

/* FOC电机控制PWM初始化 */
void TIM3_PWM_Init(u16 arr)
{
	/*初始化结构体*/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	/*开启rcc时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	/*初始化TIM3*/
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_CenterAligned1; // 中心对称1!
	TIM_TimeBaseInitStructure.TIM_Period = arr - 1;								// ARR = 3360
	TIM_TimeBaseInitStructure.TIM_Prescaler = 1 - 1;							// 使得频率剩下25kHz
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

	TIM_ARRPreloadConfig(TIM3, ENABLE);

	/*TIM3配置PWM模式*/
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0; // 占空比

	TIM_OC2Init(TIM3, &TIM_OCInitStructure);
	TIM_OC3Init(TIM3, &TIM_OCInitStructure);
	TIM_OC4Init(TIM3, &TIM_OCInitStructure);

	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);
	TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
	TIM_OC4PreloadConfig(TIM3, TIM_OCPreload_Enable);

	TIM_CtrlPWMOutputs(TIM3, ENABLE);

	TIM_Cmd(TIM3, ENABLE);
}

/* TIM10定时中断1ms */
void TIM10_Count_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruture;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE);

	TIM_TimeBaseInitStruture.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruture.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStruture.TIM_Period = 8400 - 1;	 // ARR = 8400 - 1	// 1ms
	TIM_TimeBaseInitStruture.TIM_Prescaler = 10 - 1; // RSC = 10 - 1
	TIM_TimeBaseInitStruture.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM10, &TIM_TimeBaseInitStruture);

	TIM_ClearFlag(TIM10, TIM_FLAG_Update);

	TIM_ITConfig(TIM10, TIM_IT_Update, ENABLE);

	NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_TIM10_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 7;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);

	TIM_Cmd(TIM10, ENABLE);
}

void FOC_GPIO_Config(void)
{
	/*初始化结构体*/
	GPIO_InitTypeDef GPIO_InitStructure;

	/*开启rcc时钟*/
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	/*初始化PB5-TIM3_CH2*/
	/*初始化PB0-TIM3_CH3*/
	/*初始化PB1-TIM3_CH4*/
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_0 | GPIO_Pin_1;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_TIM3);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_TIM3);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource1, GPIO_AF_TIM3);

	/*初始化PC14-DRV_EN*/
	/*初始化PC15-DRV_FAULT*/
	GPIO_StructInit(&GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOC, GPIO_Pin_14);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
}

void EasyFOC_Init(void)
{
	FOC_GPIO_Config();		   // FOC相关的引脚初始化
	TIM3_PWM_Init(PWM_Period); // FOC的PWM初始化

	delay_ms(100); // Wait for the system to stabilize

	InlineCurrentSense(0.01f, 50, ADC_Channel_14, ADC_Channel_15, NOT_SET); // SimpleMotor // 采样电阻阻值，运放倍数，A相，B相，C相
	InlineCurrentSense_Init();												// ADC初始化和偏置电压校准

	MagneticSensor_OptionSelect(MAGNETIC_SENSOR_AS5600); // 磁编码器选择AS5047P
	MagneticSensor_Init(); // 磁编码器初始化
	LPF_init();			   // LPF参数初始化
	PID_init();			   // PID参数初始化

	voltage_power_supply = 12.0f;	  // V，电源电压
	voltage_sensor_align = 4.0f;	  // V，航模电机设置的值小一点比如0.5-1，云台电机设置的大一点比如2-3
	voltage_limit = 6.0f;			  // V，主要为限制电机最大电流，最大值需小于12/1.732=6.9
	velocity_limit = 40;			  // rad/s，角度模式时限制最大转速，力矩模式和速度模式不起作用
	current_limit = 20;				  // 50	// A，foc_current和dc_current模式限制电流，不能为0。速度模式和位置模式起作用
	torque_controller = Type_voltage; // 当前只有电压模式
	controller = Type_velocity;		  // Type_angle; //Type_torque; //Type_velocity
	pole_pairs = 7;					  // 电机极对数，按照实际设置，虽然可以上电检测但有失败的概率

	PID_velocity.P = 0.11f; // 速度环PI参数，只用P参数方便快速调试
	PID_velocity.I = 0.98f;

	P_angle.P = 20.0f; // 位置环参数，只需P参数，一般不需要改动

	PID_current_d.P = 0.2f; // 电流环PI参数，可以进入 PID_init() 函数中修改其它参数
	PID_current_d.I = 0;	// 电流环I参数不太好调试，设置为0只用P参数也可以
	PID_current_q.P = 0.16f;
	PID_current_q.I = 0;

	PID_velocity.output_ramp = 0; // 速度爬升斜率，如果不需要可以设置为0
	LPF_velocity.Tf = 0.01f;

	Motor_init();
	Motor_initFOC(0, UNKNOWN); // (0,UNKNOWN) // 第一次先获得偏移角和方向，填入代码编译后再下载，以后可以跳过零点校准

	TIM10_Count_Init(); // interrupt per 1ms

	printf("EasyFOC Init is OK!\r\nMotor is ready.\r\n");
}

#include "CommonMacro.h"

uint32_t Led_count = 0;

extern float target; // 上电后以 x rad/s的转速转动(x 圈/秒)

void TIM1_UP_TIM10_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM10, TIM_IT_Update) == SET)
	{
		TIM_ClearITPendingBit(TIM10, TIM_IT_Update);

		// Led每500ms反转一次
		if (++Led_count > 500)
		{
			LED0 = !LED0;
			Led_count = 0;
		}
	}
}
