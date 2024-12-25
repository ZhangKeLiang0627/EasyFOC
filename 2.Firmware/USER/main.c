#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"

#include "led.h"
#include "BEEPER.h"
#include "oled.h"
#include "multi_button_user.h"
#include "multi_button.h"
#include "AS5600.h"
#include "AS5047P.h"
#include "MyADC.h"
#include "Random.h"

#include "w25qxx.h"
#include "malloc.h"
#include "ff.h"
#include "exfuns.h"
#include "fattester.h"

#include "u8g2.h"
#include "u8g2_Init.h"
#include "HugoUI_User.h"
#include "Hugo_UI.h"
#include "FileSystem.h"

#include "FOCuser_Inc.h"

u8g2_t u8g2; // 初始化u8g2结构体

uint8_t KeyNum = 0;
uint8_t EncoderNum = 0;

float target; // 控制电机转速 rad/s(0圈/秒)
float angle;
float BatteryVoltage;
extern uint8_t USART6_Recive_flag;

// 任务句柄
TaskHandle_t LED0Task_Handler;
TaskHandle_t OledRefreshTask_Handler;
TaskHandle_t CommanderProcTask_Handler;
TaskHandle_t FOCLoopTask_Handler;
TaskHandle_t KeyProcTask_Handler;
TaskHandle_t BeepProcTask_Handler;

// 任务函数
void led0_task(void *pvParameters);
void OledRefresh_task(void *pvParameters);
void CommanderProc_task(void *pvParameters);
void KeyProc_task(void *pvParameters);
void BeepProc_task(void *pvParameters);
void FOCLoop_task(void *pvParameters);

// 应用函数
void Commander_Proc(void);
void Oled_Refresh(void);

int main(void)
{
	// 设置系统中断优先级分组4
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	// 初始化延时函数
	delay_init(84);

	// 初始化串口（波特率）
	USART6_Init(115200);
	USART1_Init(115200);

	// ADC外设初始化
	MyADC_Init();

	// 初始化内部内存池
	my_mem_init(SRAMIN);

	// BSP初始化
	OLED_Init();
	LED_Init();
	user_keyBSP_init();

	// u8g2图形库初始化
	// HugoUI_InitLayout();
	u8g2Init(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_wqy13_t_gb2312a); // 选择字库，若内存不够就用u8g2_font_profont15_mr

	// Oled打印：正在初始化
	printf("[System] Motor init...\r\n");
	Oled_u8g2_ClearBuffer();
	Oled_u8g2_ShowUTF8(0, FONT_HEIGHT * 2.5f, "Init...电机准备中...");
	Oled_u8g2_SendBuffer();

	// EasyFOC初始化
	EasyFOC_Init();
	// M1_Disable();
	M1_Enable();
	target = 3.0f;

	// Oled打印：准备完毕
	printf("[System] Motor ready!\r\n");
	Oled_u8g2_ClearBuffer();
	Oled_u8g2_ShowUTF8(0, FONT_HEIGHT * 2.5f, "Ready! 电机准备好啦!");
	Oled_u8g2_SendBuffer();

	// xTaskCreate((TaskFunction_t)led0_task, "led0_task", 512, NULL, 2, &LED0Task_Handler);
	xTaskCreate((TaskFunction_t)OledRefresh_task, "OledRefresh_task", 512, NULL, 6, &OledRefreshTask_Handler);
	xTaskCreate((TaskFunction_t)CommanderProc_task, "CommanderProc_task", 512, NULL, 6, &CommanderProcTask_Handler);
	xTaskCreate((TaskFunction_t)KeyProc_task, "KeyProc_task", 512, NULL, 6, &KeyProcTask_Handler);
	xTaskCreate((TaskFunction_t)BeepProc_task, "BeepProc_task", 512, NULL, 6, &BeepProcTask_Handler);
	xTaskCreate((TaskFunction_t)FOCLoop_task, "FOCLoop_task", 512, NULL, 6, &FOCLoopTask_Handler);

	vTaskStartScheduler(); // 开启任务调度

	while (1)
	{
		// HugoUI_System();

		// __IntervalExecute(Oled_Refresh(), 1000);

		// Commander_Proc();

		// __IntervalExecute(printf("Volt = %.2f\r\n", getBetteryVolt()), 5000);
	}
}

void Oled_Refresh(void)
{
	Oled_u8g2_ClearBuffer();

	Oled_u8g2_ShowStr(0, FONT_HEIGHT, "Angle:");
	Oled_u8g2_ShowFloat(50, FONT_HEIGHT, angle, 3, 2);

	Oled_u8g2_ShowStr(0, FONT_HEIGHT * 2, "Speed:");
	Oled_u8g2_ShowFloat(50, FONT_HEIGHT * 2, target, 3, 2);

	Oled_u8g2_ShowStr(0, FONT_HEIGHT * 3, "Vel:");
	Oled_u8g2_ShowFloat(50, FONT_HEIGHT * 3, shaft_velocity, 2, 2);

	Oled_u8g2_ShowStr(0, FONT_HEIGHT * 4, "Volt:");
	Oled_u8g2_ShowFloat(50, FONT_HEIGHT * 4, BatteryVoltage, 2, 2);

	Oled_u8g2_SendBuffer();
}

void Commander_Proc(void)
{
	if (USART6_Recive_flag == 1)
	{
		USART6_Recive_flag = 0;
		switch (USART6_RX_BUF[0])
		{
		case 'H':
			printf("Hello World!\r\n");
			break;

		case 'S':
			switch (USART6_RX_BUF[1])
			{
			case '0':
				target = 0;
				M1_Disable();

				taskENTER_CRITICAL(); // 进入临界区

				MagneticSensor_OptionSelect(MAGNETIC_SENSOR_AS5600); // 磁编码器选择AS5600
				MagneticSensor_Init();

				taskEXIT_CRITICAL(); // 退出临界区

				vTaskDelay(200);

				taskENTER_CRITICAL(); // 进入临界区

				pole_pairs = 7;
				Motor_init();
				Motor_initFOC(5.1895f, CW);

				taskEXIT_CRITICAL(); // 退出临界区

				printf("SensorChance, AS5600, Motor restart!\r\n");
				break;

			case '1':
				target = 0;
				M1_Disable();

				taskENTER_CRITICAL(); // 进入临界区

				MagneticSensor_OptionSelect(MAGNETIC_SENSOR_AS5047P); // 磁编码器选择AS5047P
				MagneticSensor_Init();

				taskEXIT_CRITICAL(); // 退出临界区

				vTaskDelay(200);

				taskENTER_CRITICAL(); // 进入临界区

				pole_pairs = 11;
				Motor_init();
				Motor_initFOC(1.3760f, CW);

				taskEXIT_CRITICAL(); // 退出临界区

				printf("SensorChance, AS5047P, Motor restart!\r\n");
				break;

			default:
				printf("ErrInput!\r\n");
				break;
			}
			break;

		case 'T': // T6.28
			target = atof((const char *)(USART6_RX_BUF + 1));
			printf("RX=%.2f\r\n", target);
			break;

		case 'P': // P0.5  设置速度环的P参数
			PID_velocity.P = atof((const char *)(USART6_RX_BUF + 1));
			printf("P=%.2f\r\n", PID_velocity.P);
			break;

		case 'I': // I0.2  设置速度环的I参数
			PID_velocity.I = atof((const char *)(USART6_RX_BUF + 1));
			printf("I=%.2f\r\n", PID_velocity.I);
			break;

		case 'V': // V  读实时速度
			printf("Vel=%.2f\r\n", shaft_velocity);
			break;

		case 'A': // A  读绝对角度
			printf("Ang=%.2f\r\n", shaft_angle);
			break;

		case 'E': // E 电机使能，U -> PowerUP使能 / D -> PowerDown失能
			switch (USART6_RX_BUF[1])
			{
			case 'U':
				target = 0;
				controller = Type_velocity;
				M1_Enable();
				printf("PowerUP, VelocityMODE!\r\n");
				break;

			case 'D':
				target = 0;
				controller = Type_velocity;
				M1_Disable();
				printf("PowerDOWN!\r\n");
				break;

			default:
				printf("ErrInput!\r\n");
				break;
			}
			break;

		case 'M': // M 电机运行模式，A -> 角度闭环 / V -> 速度闭环 / T -> 力矩闭环
			switch (USART6_RX_BUF[1])
			{
			case 'A':
				target = angle;
				controller = Type_angle;
				printf("Mode = Angle!\r\n");

				break;

			case 'V':
				target = 0;
				controller = Type_velocity;
				printf("Mode = Velocity!\r\n");
				break;

			case 'T':
				target = 0;
				controller = Type_torque;
				printf("Mode = Torque!\r\n");
				break;

			default:
				printf("Mode = ErrInput!\r\n");
				break;
			}
			break;
		}
		// memset(USART6_RX_BUF, 0, 32); // USART2_BUFFER_SIZE //清空接收数组,长度覆盖接收的字节数即可

		for (int i = 0; i < 32; i++)
		{
			USART6_RX_BUF[i] = '\0';
		}
		USART6_RX_STA = 0;
	}
}

// LED0任务函数
void led0_task(void *pvParameters)
{
	while (1)
	{
		LED0 = ~LED0;

		printf("FreeRTOS is working!\r\n");

		vTaskDelay(500);
	}
}

// Oled刷新任务函数
void OledRefresh_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();

	while (1)
	{
		Oled_Refresh();

		// every Oled refersh task need at least 200ms delay otherwise cannot control motor normally
		vTaskDelayUntil(&xLastWakeTime, 1000);
	}
}

// 命令发布任务函数
void CommanderProc_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();

	while (1)
	{
		Commander_Proc();

		__IntervalExecute(BatteryVoltage = getBetteryVolt() * 6.0f, 5000);

		__IntervalExecute(printf("[System] Voltage = %.2f\r\n", BatteryVoltage), 5000);

		vTaskDelayUntil(&xLastWakeTime, 10);
	}
}

// 按键处理任务函数
void KeyProc_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();

	while (1)
	{
		button_ticks();

		vTaskDelayUntil(&xLastWakeTime, 5);
	}
}

// 蜂鸣器处理任务函数
void BeepProc_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();

	Beeper_Init();
	Beeper_Perform(BEEPER_TRITONE);

	while (1)
	{
		Beeper_Proc();

		vTaskDelayUntil(&xLastWakeTime, 10);
	}
}

void FOCLoop_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();

	while (1)
	{
		// 获取实时角度
		__IntervalExecute(angle = getAngle(), 100);

		// 循环执行FOC控制算法
		move(target);
		loopFOC();

		// every FOC control task need at least 1ms delay otherwise cannot run normally
		vTaskDelayUntil(&xLastWakeTime, 1);
	}
}
