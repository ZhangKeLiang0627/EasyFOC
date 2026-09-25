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
float BatteryVoltage;
extern uint8_t USART6_Recive_flag;

// 电流限幅上限（A）：3505 额定 0.5A、DRV8313 过流保护 3A，留安全余量
#define MAX_CURRENT_LIMIT 2.0f

// ===== 临时波形采集（PID 调参用，@1kHz 采样）=====
#define SCOPE_N 1000 // 采样点数（@1kHz = 1000ms）
static float scope_buf[SCOPE_N][4]; // [0]=shaft_velocity [1]=shaft_angle [2]=current_sp [3]=current.q
static volatile uint16_t scope_idx = 0;
static volatile uint8_t scope_state = 0; // 0=空闲 1=采集中 2=待dump

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
void Oled_Proc(void);

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

	// BSP初始化
	OLED_Init();
	LED_Init();
	user_keyBSP_init();

	// u8g2图形库初始化
	u8g2Init(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_lubBI08_tr); // 主字体 lubBI08，小号处临时切 5x8
	// u8g2_SetFont(&u8g2, u8g2_font_wqy13_t_gb2312a); // 选择字库，若内存不够就用u8g2_font_profont15_mr

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
		// __IntervalExecute(Oled_Proc(), 1000);

		// Commander_Proc();

		// __IntervalExecute(printf("Volt = %.2f\r\n", getBetteryVolt()), 5000);
	}
}

// 读电机状态（使能态读 PC14 输出，故障态读 PC15 输入低有效，蓝牙读 PC8 高有效）
static void Oled_GetMotorState(uint8_t *enabled, uint8_t *fault, uint8_t *bt)
{
	*enabled = GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_14);
	*fault = (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_15) == 0);
	*bt = GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_8);
}

// 状态块：ERR 反色块 / 使能=圆角反色块 / 待机=无框只有字
static void Oled_DrawStateChip(uint8_t enabled, uint8_t fault)
{
	if (fault)
	{
		Oled_u8g2_DrawRBox(0, 1, 30, 13, 2);
		Oled_u8g2_SetDrawColor(0);
		Oled_u8g2_ShowStr(4, 11, "ERR");
		Oled_u8g2_SetDrawColor(1);
	}
	else if (enabled)
	{
		Oled_u8g2_DrawRBox(0, 1, 24, 13, 2);
		Oled_u8g2_SetDrawColor(0);
		Oled_u8g2_ShowStr(4, 11, "OK");
		Oled_u8g2_SetDrawColor(1);
	}
	else
	{
		Oled_u8g2_ShowStr(2, 11, "OK");
	}
}

static const char *Oled_CtrlStr(void)
{
	switch (controller)
	{
	case Type_angle:
		return "ANG";
	case Type_torque:
		return "TRQ";
	default:
		return "VEL";
	}
}

static const char *Oled_ModeStr(void)
{
	return (torque_controller == Type_voltage) ? "V" : "C";
}

void Oled_Proc(void)
{
	static uint8_t enabled, fault, bt;
	char buf[32];

	Oled_GetMotorState(&enabled, &fault, &bt);

	Oled_u8g2_ClearBuffer();

	// ===== 第一栏：状态 + 模式 + 供电（y 0~28）=====

	// 行1：OK 状态块（lubBI08，左上）+ BT（5x8，OK 右边）+ 电压（lubBI08，右对齐）
	Oled_u8g2_SetFont(u8g2_font_lubBI08_tr);
	Oled_DrawStateChip(enabled, fault);

	Oled_u8g2_SetFont(u8g2_font_5x8_tr);
	sprintf(buf, "BT-Comm [%c]", bt ? '*' : ' ');
	Oled_u8g2_ShowStr(30-1, 9+2, buf);

	Oled_u8g2_SetFont(u8g2_font_lubBI08_tr);
	sprintf(buf, "%.2fV", BatteryVoltage);
	Oled_u8g2_ShowStr(127 - Oled_u8g2_Get_UTF8_ASCII_PixLen(buf), 12, buf);

	// 行2：CTRL/MODE（5x8，左对齐）+ 电流（lubBI08，右对齐）
	Oled_u8g2_SetFont(u8g2_font_5x8_tr);
	sprintf(buf, "CL: [%s] ME: [%s]", Oled_CtrlStr(), Oled_ModeStr());
	Oled_u8g2_ShowStr(0, 26, buf);

	Oled_u8g2_SetFont(u8g2_font_lubBI08_tr);
	sprintf(buf, "%.2fA", current.q);
	Oled_u8g2_ShowStr(127 - Oled_u8g2_Get_UTF8_ASCII_PixLen(buf), 26, buf);

	// 水平分隔线（3px 白条）
	Oled_u8g2_DrawBox(0, 29, 128, 3);

	// ===== 第二栏：PID + 实时量（y 32~63，3 行 5x8）=====
	Oled_u8g2_SetFont(u8g2_font_5x8_tr);

	// 行1：电流环 q 轴 P + 角度
	sprintf(buf, "Pq: %.2f      ANG: %.2f", PID_current_q.P, shaft_angle);
	Oled_u8g2_ShowStr(6, 41, buf);
	// 行2：电流环 q 轴 I + 目标值
	sprintf(buf, "Iq: %.2f     TGT: %.2f", PID_current_q.I, target);
	Oled_u8g2_ShowStr(6, 52, buf);
	// 行3：电流限幅 + 速度
	sprintf(buf, "LIMT: %.2fA", current_limit);
	Oled_u8g2_ShowStr(6, 63, buf);
	sprintf(buf, "%.2frad/s", shaft_velocity);
	Oled_u8g2_ShowStr(70+6, 63, buf);

	// 竖线1
	Oled_u8g2_DrawBox(0, 34, 2, 30);

	// 竖线2
	Oled_u8g2_DrawBox(70, 34, 2, 30);

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
			printf("Target=%.2f\r\n", target);
			break;

		case 'P': // P0.5  设置速度环的P参数
			PID_velocity.P = atof((const char *)(USART6_RX_BUF + 1));
			printf("VelocityP=%.2f\r\n", PID_velocity.P);
			break;

		case 'I': // I0.2  设置速度环的I参数
			PID_velocity.I = atof((const char *)(USART6_RX_BUF + 1));
			printf("VelocityI=%.2f\r\n", PID_velocity.I);
			break;

		case 'Q': // Q3.0  设置电流环P参数（DC current / foc current 模式用，单位欧姆）
			PID_current_q.P = atof((const char *)(USART6_RX_BUF + 1));
			printf("CurrentP=%.2f\r\n", PID_current_q.P);
			break;

		case 'W': // W100  设置电流环I参数（DC current / foc current 模式用，单位欧姆/秒）
			PID_current_q.I = atof((const char *)(USART6_RX_BUF + 1));
			printf("CurrentI=%.2f\r\n", PID_current_q.I);
			break;

		case 'V': // V  读实时速度
			printf("Velocity=%.2f\r\n", shaft_velocity);
			break;

		case 'A': // A  读绝对角度
			printf("Angle=%.2f\r\n", shaft_angle);
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

		case 'R': // R  读电流环内部状态（current_sp / current.q / voltage.q），用于调试电流环
			printf("[R] sp=%.3f Iq=%.3f Id=%.3f | Vq=%.3f Vd=%.3f\r\n",
				   current_sp, current.q, current.d, voltage.q, voltage.d);
			break;

		case 'E': // E 电机使能，U -> PowerUP使能 / D -> PowerDown失能
			switch (USART6_RX_BUF[1])
			{
			case 'U':
				target = 0;
				controller = Type_angle;
				M1_Enable();
				printf("PowerUP, VelocityMODE!\r\n");
				break;

			case 'D':
				target = 0;
				controller = Type_angle;
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
				target = shaft_angle;
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

		case 'L': // L1.5  设置电流限幅（A），上限 MAX_CURRENT_LIMIT
		{
			float lim = atof((const char *)(USART6_RX_BUF + 1));
			if (lim > MAX_CURRENT_LIMIT)
			{
				printf("Limit too high! Max %.2fA\r\n", MAX_CURRENT_LIMIT);
			}
			else if (lim <= 0.0f)
			{
				printf("Limit must be > 0!\r\n");
			}
			else
			{
				current_limit = lim;
				printf("CurrentLimit=%.2fA\r\n", current_limit);
			}
		}
		break;

		case 'G': // G  启动波形采集（500 点 @1kHz = 500ms，采完自动 dump）
			scope_idx = 0;
			scope_state = 1;
			printf("ScopeArm\r\n");
			break;

		case 'Y': // Y0.008  设置速度反馈 LPF 时间常数 Tf(s)，越小滞后越小、噪声越大
			LPF_velocity.Tf = atof((const char *)(USART6_RX_BUF + 1));
			printf("VelocityLPF=%.4f\r\n", LPF_velocity.Tf);
			break;

		case 'Z': // Z0.001  设置电流反馈 LPF 时间常数 Tf(s)
			LPF_current_q.Tf = atof((const char *)(USART6_RX_BUF + 1));
			printf("CurrentLPF=%.4f\r\n", LPF_current_q.Tf);
			break;

		case 'B': // B20  设置位置环 P 参数
			P_angle.P = atof((const char *)(USART6_RX_BUF + 1));
			printf("AngleP=%.2f\r\n", P_angle.P);
			break;
		}
		// 清空接收数组，长度覆盖接收的字节数即可
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

		vTaskDelay(500);
	}
}

// Oled刷新任务函数
void OledRefresh_task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	BatteryVoltage = getBetteryVolt() * 6.0f;
	
	while (1)
	{
		Oled_Proc();

		__IntervalExecute(BatteryVoltage = getBetteryVolt() * 6.0f, 5000);

		__IntervalExecute(printf("[System] Voltage = %.2f\r\n", BatteryVoltage), 10000);

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

		// 临时：波形采集完成则 dump（本任务优先级 6 < FOCLoop 8，dump 不会阻塞控制环）
		if (scope_state == 2)
		{
			uint16_t i;
			printf("ScopeStart\r\n");
			for (i = 0; i < SCOPE_N; i++)
			{
				printf("%.3f,%.3f,%.3f,%.3f\r\n",
					   scope_buf[i][0], scope_buf[i][1], scope_buf[i][2], scope_buf[i][3]);
			}
			printf("ScopeEnd\r\n");
			scope_state = 0;
		}

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
		// loopFOCISR() 已搬进 TIM10 10kHz 中断：电流环 → 输出 voltage.q → setPhaseVoltage
		move(target);

		// 临时：波形采集 @1kHz（G 命令触发）
		if (scope_state == 1)
		{
			if (scope_idx < SCOPE_N)
			{
				scope_buf[scope_idx][0] = shaft_velocity;
				scope_buf[scope_idx][1] = shaft_angle;
				scope_buf[scope_idx][2] = current_sp;
				scope_buf[scope_idx][3] = current.q;
				scope_idx++;
			}
			else
				scope_state = 2;
		}

		// every FOC control task need at least 1ms delay otherwise cannot run normally
		vTaskDelayUntil(&xLastWakeTime, 1);
	}
}
