#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"

#include "led.h"
#include "BEEPER.h"
#include "oled.h"
#include "MyADC.h"
#include "multi_button.h"
#include "multi_button_user.h"

#include "u8g2.h"
#include "u8g2_Init.h"
#include "OLED_User.h"

#include "FOCuser_Inc.h"

u8g2_t u8g2; 

uint8_t KeyNum = 0;
uint8_t EncoderNum = 0;

// 控制电机转速 rad/s (圈/秒)
float target; 
float angle;
float BatteryVoltage;
extern uint8_t USART6_Recive_flag;

// DWT 计时统计（loopFOCISR 单次执行耗时，定义在 BLDCMotor.c，单位 CPU 周期 @84MHz）
extern uint32_t foc_cycles_min;
extern uint32_t foc_cycles_max;
extern uint32_t foc_cycles_sum;
extern uint32_t foc_cycles_cnt;

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
	DWT_Init(); // 使能 DWT 周期计数器，用于测量 loopFOC 执行时间

	// 初始化串口（波特率）
	USART6_Init(115200);
	USART1_Init(115200);

	// ADC外设初始化
	MyADC_Init();

	// BSP初始化
	OLED_Init();
	LED_Init();
	user_keyBSP_init();

	// u8g2图形库初始化
	u8g2Init(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_wqy13_t_gb2312a); // 选择字库，若内存不够就用u8g2_font_profont15_mr

	// Oled打印：正在初始化
	printf("[System] Motor init...\r\n");
	Oled_u8g2_ClearBuffer();
	Oled_u8g2_ShowUTF8(0, FONT_HEIGHT * 2.5f, "MOTOR Init...");
	Oled_u8g2_SendBuffer();

	// EasyFOC初始化
	EasyFOC_Init();
	M1_Disable();
	// M1_Enable();
	target = 0.0f; // 开机不转，等待串口命令（S0/S1选编码器、E U使能、T设转速）

	// Oled打印：准备完毕
	printf("[System] Motor ready!\r\n");
	Oled_u8g2_ClearBuffer();
	Oled_u8g2_ShowUTF8(0, FONT_HEIGHT * 2.5f, "MOTOR Ready!!!");
	Oled_u8g2_SendBuffer();

	xTaskCreate((TaskFunction_t)led0_task, "led0_task", 64, NULL, 2, &LED0Task_Handler);
	xTaskCreate((TaskFunction_t)OledRefresh_task, "OledRefresh_task", 512, NULL, 6, &OledRefreshTask_Handler);
	xTaskCreate((TaskFunction_t)CommanderProc_task, "CommanderProc_task", 512, NULL, 6, &CommanderProcTask_Handler);
	xTaskCreate((TaskFunction_t)KeyProc_task, "KeyProc_task", 512, NULL, 6, &KeyProcTask_Handler);
	xTaskCreate((TaskFunction_t)BeepProc_task, "BeepProc_task", 512, NULL, 6, &BeepProcTask_Handler);
	xTaskCreate((TaskFunction_t)FOCLoop_task, "FOCLoop_task", 512, NULL, 8, &FOCLoopTask_Handler);

	vTaskStartScheduler(); // 开启任务调度

	while (1)
	{
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

		case 'Q': // Q3.0  设置电流环P参数（DC current / foc current 模式用，单位欧姆）
			PID_current_q.P = atof((const char *)(USART6_RX_BUF + 1));
			printf("CurrentP=%.2f\r\n", PID_current_q.P);
			break;

		case 'W': // W100  设置电流环I参数（DC current / foc current 模式用，单位欧姆/秒）
			PID_current_q.I = atof((const char *)(USART6_RX_BUF + 1));
			printf("CurrentI=%.2f\r\n", PID_current_q.I);
			break;

		case 'I': // I0.2  设置速度环的I参数
			PID_velocity.I = atof((const char *)(USART6_RX_BUF + 1));
			printf("I=%.2f\r\n", PID_velocity.I);
			break;

		case 'V': // V  读实时速度
			printf("Vel=%.2f\r\n", shaft_velocity);
			break;

		case 'D': // D  打印 loopFOCISR 执行耗时统计（DWT CYCCNT，@84MHz，1周期=11.9ns）
		{
			// 只读 DWT 状态（确认使能是否生效）
			printf("[D] DWT state: DEMCR=0x%lX CTRL=0x%lX\r\n",
				   (unsigned long)CoreDebug->DEMCR,
				   (unsigned long)DWT->CTRL);
			float avg = (foc_cycles_cnt) ? (float)foc_cycles_sum / foc_cycles_cnt : 0.0f;
			printf("[D] loopFOCISR cycles: min=%lu max=%lu avg=%.1f cnt=%lu\r\n",
				   (unsigned long)foc_cycles_min, (unsigned long)foc_cycles_max, avg, (unsigned long)foc_cycles_cnt);
			printf("[D] loopFOCISR time:   min=%.2fus max=%.2fus avg=%.2fus\r\n",
				   foc_cycles_min / 84.0f, foc_cycles_max / 84.0f, avg / 84.0f);
			// 复位统计
			foc_cycles_min = 0xFFFFFFFF;
			foc_cycles_max = 0;
			foc_cycles_sum = 0;
			foc_cycles_cnt = 0;
		}
		break;

		case 'A': // A  读绝对角度
			printf("Ang=%.2f\r\n", shaft_angle);
			break;

		case 'C': // C  只读电流采样（不碰闭环），用于标定符号/偏移/增益
		{
			unsigned short rawA = analogRead(ADC_SENSE_A);
			unsigned short rawB = analogRead(ADC_SENSE_B);
			PhaseCurrent_s pc = getPhaseCurrents();
			DQCurrent_s dq = getFOCCurrents(electrical_angle);
			printf("[C] rawA=%d rawB=%d | ia=%.3f ib=%.3f | theta=%.2f Id=%.3f Iq=%.3f\r\n",
				   rawA, rawB, pc.a, pc.b, electrical_angle, dq.d, dq.q);
		}
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

		case 'N': // N 切换力矩控制方式，V -> 电压模式 / C -> DC current 电流闭环
			switch (USART6_RX_BUF[1])
			{
			case 'V':
				target = 0;
				torque_controller = Type_voltage;
				printf("TorqueCtrl = Voltage!\r\n");
				break;

			case 'C':
				target = 0;
				torque_controller = Type_dc_current;
				printf("TorqueCtrl = DC current!\r\n");
				break;

			default:
				printf("ErrInput!\r\n");
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

		// printf("FreeRTOS is working!\r\n");

		vTaskDelay(500);
	}
}

// Oled刷新任务函数
void OledRefresh_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();

	while (1)
	{
		// 获取实时角度
		// 注意：改用 volatile shaft_angle（由 20kHz 中断更新），不能调 getAngle()——
		// getAngle() 含 static 多圈累加状态，任务与中断并发调用会数据竞态。
		__IntervalExecute(angle = shaft_angle, 1000);
		
		Oled_Refresh();

		__IntervalExecute(BatteryVoltage = getBetteryVolt() * 6.0f, 5000);

		__IntervalExecute(printf("[System] Voltage = %.2f\r\n", BatteryVoltage), 5000);

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


		vTaskDelayUntil(&xLastWakeTime, 20);
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
		// 循环执行FOC控制算法
		// move() 留在 1kHz 任务：速度环/位置环/力矩环 → 输出 current_sp
		// loopFOCISR() 已搬进 TIM3 20kHz 中断：电流环 → 输出 voltage.q → setPhaseVoltage
		move(target);

		// every FOC control task need at least 1ms delay otherwise cannot run normally
		vTaskDelayUntil(&xLastWakeTime, 1);
	}
}
