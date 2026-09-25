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

// FOC 环（loopFOC 电流环）运行位置：
//   1 = 跑在 TIM10 10kHz 中断里（默认，配 AS5047P：SPI 读一次约 15us，中断装得下）
//   0 = 跑在 1kHz FOCLoop_task 里（配 AS5600：I2C 读一次 100-450us，中断装不下，
//       强行放中断会占满 CPU 导致系统卡死/冻结——实测过）
// 由 S0/S1 切换传感器时同步设置；中断本身也随之开关。
static uint8_t foc_loop_in_isr = 1;

// FOC 环暂停标志：切换传感器(S0/S1) 与重新标定(S2) 期间置 1。
// 两件事都必须停掉闭环：
//   1) 标定靠 setPhaseVoltage() 开环给固定电角度，闭环会每周期覆盖它 → 标定结果作废；
//   2) Motor_init() 内部会 M1_Enable()，上电默认又是位置闭环 ——
//      不停闭环就可能在重新初始化的中途把电机驱动起来（实测过"一发 S0 就爆转"）。
static volatile uint8_t foc_pause = 0;

// 电流限幅上限（A）：DRV8313 的过流保护点是 3A，取 3A 作硬顶。
// 注意 2804/3505 都是小电机（3505 额定 0.5A），长时间跑大限幅会发热。
#define MAX_CURRENT_LIMIT 3.0f

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
	Oled_u8g2_ShowStr(30 - 1, 9 + 2, buf);

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
	Oled_u8g2_ShowStr(70 + 6, 63, buf);

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

				// 切换期间：停闭环 + 保持失能。
				// Motor_init() 内部会无条件 M1_Enable()，而本项目上电默认是位置闭环，
				// 若不在这里压住，切完传感器时驱动已被使能、位置环直接开始守位 ——
				// 标定与新装配不匹配时会立刻满电流跑飞（实测过：一发 S0 就爆转）。
				foc_pause = 1;
				M1_Disable();

				// AS5600 走 I2C，单次读 100-450us，10kHz 中断装不下 → 关掉 TIM10 中断，
				// 把整个 FOC 环放回 1kHz 任务里跑（= 搬进中断之前的原始架构）。
				// 同样注意不能用 taskENTER_CRITICAL：它会屏蔽 SysTick，使 _micros()/delay_ms 死等。
				foc_loop_in_isr = 0;
				TIM_ITConfig(TIM10, TIM_IT_Update, DISABLE);

				MagneticSensor_OptionSelect(MAGNETIC_SENSOR_AS5600); // 磁编码器选择AS5600
				MagneticSensor_Init();

				vTaskDelay(200);

				pole_pairs = 7;
				// 2804 电机（AS5600 装配）的电流环 PI —— 用户实测整定值。
				// 放这里是为了让"切到 2804"变成一条命令搞定，不用每次上电手动发 Q/W 调增益。
				PID_current_q.P = 0.8f;
				PID_current_q.I = 35.0f;
				PID_current_d.P = 0.8f;
				PID_current_d.I = 35.0f;
				printf("[S0] 2804 current-loop PI loaded: Iq P=%.2f I=%.1f | Id P=%.2f I=%.1f\r\n",
					   PID_current_q.P, PID_current_q.I, PID_current_d.P, PID_current_d.I);

				Motor_init();
				// 零点偏移已按本装配实测固化（2026-09-25 用 S2 量得 1.5018）。
				// 旧值 5.1895 属于上一套装配，偏差 211° 电角度 → 转矩反向 → 使能后满电流跑飞。
				// 若改动过磁铁/传感器的装配，重新发 S2 标定并更新这个数。
				Motor_initFOC(1.5018f, CW);

				M1_Disable(); // 切换完成保持失能，必须显式 EU 才使能
				foc_pause = 0;

				printf("SensorChance, AS5600, Motor restart! (disabled, send EU to enable)\r\n");
				break;

			case '1':
				target = 0;

				// 同 S0：切换期间停闭环 + 保持失能（Motor_init() 会 M1_Enable()）
				foc_pause = 1;
				M1_Disable();

				// AS5047P 走 SPI（约 15us），可以留在 10kHz 中断里保证电流环带宽
				TIM_ITConfig(TIM10, TIM_IT_Update, DISABLE);

				MagneticSensor_OptionSelect(MAGNETIC_SENSOR_AS5047P); // 磁编码器选择AS5047P
				MagneticSensor_Init();

				vTaskDelay(200);

				pole_pairs = 11;
				// 3505 电机（AS5047P 装配）的电流环 PI —— 保持与 FOCBaseConfig.c 的默认值一致。
				// 必须显式恢复，否则从 S0 切回来会残留 2804 的增益（0.8/35）。
				PID_current_q.P = 1.2f;
				PID_current_q.I = 75.0f;
				PID_current_d.P = 0.0f;
				PID_current_d.I = 0.0f;
				printf("[S1] 3505 current-loop PI restored: Iq P=%.2f I=%.1f | Id P=%.2f I=%.1f\r\n",
					   PID_current_q.P, PID_current_q.I, PID_current_d.P, PID_current_d.I);

				Motor_init();
				Motor_initFOC(1.3760f, CW);

				M1_Disable(); // 切换完成保持失能，必须显式 EU 才使能

				TIM_ClearFlag(TIM10, TIM_FLAG_Update);
				TIM_ITConfig(TIM10, TIM_IT_Update, ENABLE);
				foc_loop_in_isr = 1;
				foc_pause = 0;

				printf("SensorChance, AS5047P, Motor restart! (disabled, send EU to enable)\r\n");
				break;

			case '2':
				// S2 = 对当前电机重新标定（现场实测 sensor_direction + 电角度零点）
				//
				// 为什么需要它：S0/S1 用的是硬编码标定值（AS5600: 1.5018/CW 本装配实测，
				// AS5047P: 1.3760/CW），只对当初那台电机 + 磁铁 + 传感器的装配成立。
				// 换电机或改变装配后零点/方向都会变，症状就是使能后立刻满电流跑飞
				// （方向反了 → 位置环变正反馈；零点偏超过 90° 电角度 → 转矩反向）。
				target = 0;

				// 标定全程靠 setPhaseVoltage() 开环给一个固定电角度，必须停掉闭环，
				// 否则 move()/loopFOC() 每个周期都会覆盖它，测出来的零点/方向是错的。
				foc_pause = 1;
				M1_Disable();
				TIM_ITConfig(TIM10, TIM_IT_Update, DISABLE); // 标定全在任务里做
				foc_loop_in_isr = 0;

				// 关键：Motor_initFOC() 只在 (偏移 != 0 && 方向 != UNKNOWN) 时才赋值，
				// 直接传 (0, UNKNOWN) 不会改动全局量，alignSensor() 会因为
				// zero_electric_angle != 0 而继续 "Skip offset calib"。必须先手工清零。
				zero_electric_angle = 0;
				sensor_direction = UNKNOWN;

				printf("[S2] Calibrating: motor will be driven open-loop ~3s, keep shaft free...\r\n");
				vTaskDelay(100);

				Motor_init();
				Motor_initFOC(0, UNKNOWN); // 触发 alignSensor() 实测方向 + 零点

				M1_Disable(); // 标定完立刻失能，等显式 EU
				foc_pause = 0;

				printf("[S2] Done: sensor_direction=%s  zero_electric_angle=%.4f  pole_pairs=%d\r\n",
					   (sensor_direction == CW) ? "CW" : "CCW",
					   zero_electric_angle, (int)pole_pairs);
				printf("[S2] FOC loop left in TASK mode(1kHz); re-send S0/S1 to restore.\r\n");
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

		case 'L': // L1.5  设置电流限幅（A），等价于"允许的最大力矩"，上限 MAX_CURRENT_LIMIT
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

				// 必须同步速度环输出限幅：Motor_init() 只是把 current_limit 快照进
				// PID_velocity.limit，之后不会自动跟随。不同步的话本命令在速度/位置模式下
				// 完全不生效（历史上的真实 bug，只改了 current_limit 却看不到任何效果）。
				PID_velocity.limit = (torque_controller == Type_voltage) ? voltage_limit : current_limit;

				printf("CurrentLimit=%.2fA\r\n", current_limit);
			}
		}
		break;

		case 'V': // V  读实时速度
			printf("Vel=%.2f\r\n", shaft_velocity);
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
				// 使能后进入位置闭环并守住当前位置（target=shaft_angle），不会转向绝对 0 度
				controller = Type_angle;
				target = shaft_angle;
				M1_Enable();
				printf("PowerUP, AngleMODE!\r\n");
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
				target = shaft_angle; // 进入角度闭环时以当前位置为目标，避免跳到绝对角度
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
	BatteryVoltage = getBetteryVolt() * 6.0f; // 上电先读一次，避免首屏显示 0V

	while (1)
	{
		Oled_Proc();

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
		// foc_pause=1（切换传感器 / 重新标定中）：整个闭环停手。
		// 否则 move() 会用闭环输出覆盖标定用的开环电压向量（标定作废），
		// 且在重新初始化的中途可能被 M1_Enable() 带着驱动电机。
		if (!foc_pause)
		{
			// 外环：move() 恒在 1kHz 任务（位置环/速度环/力矩环 → 输出 current_sp）
			move(target);

			// 电流环 loopFOC() 的运行位置由 foc_loop_in_isr 决定：
			//   1 -> 在 TIM10 10kHz 中断里跑（AS5047P/SPI），这里不能再调用，否则双跑
			//   0 -> 在本任务里跑（AS5600/I2C），此时 TIM10 中断已关闭
			if (!foc_loop_in_isr)
				loopFOC();
		}

		// every FOC control task need at least 1ms delay otherwise cannot run normally
		vTaskDelayUntil(&xLastWakeTime, 1);
	}
}
