#include "FileSystem.h"

#include "ff.h"
#include "exfuns.h"
#include "fattester.h"

#include "HugoUI_User.h"


const char SystemSavePath[] = "1:/HugoUISAV.txt";
extern struct Slide_Bar Slide_space[];
extern uint8_t ui_Key_num, ui_Encoder_num; // user可以将自己的实现函数的变量直接赋值给这两个Num

/* 用于保存数据的事件函数 */
void EventSaveSettingConfig(void)
{
	static uint8_t SaveFinish_flag = 1; // 初始化该标志为1

	static float move_x = -10, move_x_trg = 10;
	static float move_y = -10, move_y_trg = (SCREEN_HEIGHT / 2 - 15);
	static float movebox_width = 140, movebox_width_trg = 105;
	static float movebox_height = 80, movebox_height_trg = 30;

	if (SaveFinish_flag)
	{
		Oled_u8g2_ShowStr(0, 13, "Config Is Saving...");

		f_open(file, "1:/HugoUISAV.txt", FA_OPEN_ALWAYS | FA_WRITE);

		/* ---------- 屏幕亮度变量 ---------- */
		f_write(file, Slide_space[Slide_space_ScreenBrightness].val, sizeof(&Slide_space[Slide_space_ScreenBrightness].val), &bw);

		/* ---------- 动画速度变量 ---------- */
		f_write(file, Slide_space[Slide_space_Fre_x_speed].val, sizeof(&Slide_space[Slide_space_Fre_x_speed].val), &bw);
		f_write(file, Slide_space[Slide_space_Fre_y_speed].val, sizeof(&Slide_space[Slide_space_Fre_y_speed].val), &bw);
		f_write(file, Slide_space[Slide_space_Fre_width_speed].val, sizeof(&Slide_space[Slide_space_Fre_width_speed].val), &bw);
		f_write(file, Slide_space[Slide_space_Slidbar_y_speed].val, sizeof(&Slide_space[Slide_space_Slidbar_y_speed].val), &bw);
		f_write(file, Slide_space[Slide_space_Page_y_forlist_speed].val, sizeof(&Slide_space[Slide_space_Page_y_forlist_speed].val), &bw);
		f_write(file, Slide_space[Slide_space_Page_x_foricon_speed].val, sizeof(&Slide_space[Slide_space_Page_x_foricon_speed].val), &bw);

		/* ---------- 用户设置变量 ---------- */
		f_write(file, Slide_space[Slide_space_WS2812_R].val, sizeof(&Slide_space[Slide_space_WS2812_R].val), &bw);
		f_write(file, Slide_space[Slide_space_WS2812_G].val, sizeof(&Slide_space[Slide_space_WS2812_G].val), &bw);
		f_write(file, Slide_space[Slide_space_WS2812_B].val, sizeof(&Slide_space[Slide_space_WS2812_B].val), &bw);
		f_write(file, Slide_space[Slide_space_val1].val, sizeof(&Slide_space[Slide_space_val1].val), &bw);
		f_write(file, Slide_space[Slide_space_val2].val, sizeof(&Slide_space[Slide_space_val2].val), &bw);
		f_write(file, Slide_space[Slide_space_val3].val, sizeof(&Slide_space[Slide_space_val3].val), &bw);
		SaveFinish_flag = f_write(file, Slide_space[Slide_space_Volume_Ctrl].val, sizeof(&Slide_space[Slide_space_Volume_Ctrl].val), &bw);
		// f_write(file, Slide_space[Slide_space_SPO2MinVal].val, sizeof(&Slide_space[Slide_space_SPO2MinVal].val), &bw);
		// SaveFinish_flag = f_write(file, Slide_space[Slide_space_HRMaxVal].val, sizeof(&Slide_space[Slide_space_HRMaxVal].val), &bw);
		// SaveFinish_flag = f_write(file, Slide_space[Slide_space_HRMinVal].val, sizeof(&Slide_space[Slide_space_HRMinVal].val), &bw); // 如果保存成功会将该标志位置0

		f_close(file);
	}
	else
	{
		Oled_u8g2_ShowStr((SCREEN_WIDTH - Oled_u8g2_Get_UTF8_ASCII_PixLen("Saved Success!")) / 2,
						  SCREEN_HEIGHT / 2 + 3, "Saved Success!"); // 居中显示

		Oled_u8g2_SetDrawColor(2); // XOR

		//		Oled_u8g2_DrawRBox(10, (SCREEN_HEIGHT/2 - 15), Oled_u8g2_Get_UTF8_ASCII_PixLen("Saved Success! "), 30, 4);

		Oled_u8g2_DrawRBox(move_x, move_y, movebox_width, movebox_height, 4);

		Oled_u8g2_SetDrawColor(1);

		HugoUI_Animation_Liner(&move_x, &move_x_trg, 65);
		HugoUI_Animation_Liner(&move_y, &move_y_trg, 65);
		HugoUI_Animation_Liner(&movebox_width, &movebox_width_trg, 65);
		HugoUI_Animation_Liner(&movebox_height, &movebox_height_trg, 65);
	}

	if (ui_Key_num == 2)
	{
		SaveFinish_flag = 1;
		move_x = -10;
		move_y = -10;
		movebox_width = 140;
		movebox_height = 80;
	}
}

/* 用于上电载入数据的事件函数 */
void EventLoadSettingConfig(void)
{
	FRESULT res;
	res = f_open(file, SystemSavePath, FA_READ);
	if (res == 0)
	{
		/* ---------- 屏幕亮度变量 ---------- */
		f_read(file, Slide_space[Slide_space_ScreenBrightness].val, sizeof(&Slide_space[Slide_space_ScreenBrightness].val), &bw);

		/* ---------- 动画速度变量 ---------- */
		f_read(file, Slide_space[Slide_space_Fre_x_speed].val, sizeof(&Slide_space[Slide_space_Fre_x_speed].val), &bw);
		f_read(file, Slide_space[Slide_space_Fre_y_speed].val, sizeof(&Slide_space[Slide_space_Fre_y_speed].val), &bw);
		f_read(file, Slide_space[Slide_space_Fre_width_speed].val, sizeof(&Slide_space[Slide_space_Fre_width_speed].val), &bw);
		f_read(file, Slide_space[Slide_space_Slidbar_y_speed].val, sizeof(&Slide_space[Slide_space_Slidbar_y_speed].val), &bw);
		f_read(file, Slide_space[Slide_space_Page_y_forlist_speed].val, sizeof(&Slide_space[Slide_space_Page_y_forlist_speed].val), &bw);
		f_read(file, Slide_space[Slide_space_Page_x_foricon_speed].val, sizeof(&Slide_space[Slide_space_Page_x_foricon_speed].val), &bw);

		/* ---------- 用户设置变量 ---------- */
		f_read(file, Slide_space[Slide_space_WS2812_R].val, sizeof(&Slide_space[Slide_space_WS2812_R].val), &bw);
		f_read(file, Slide_space[Slide_space_WS2812_G].val, sizeof(&Slide_space[Slide_space_WS2812_G].val), &bw);
		f_read(file, Slide_space[Slide_space_WS2812_B].val, sizeof(&Slide_space[Slide_space_WS2812_B].val), &bw);
		f_read(file, Slide_space[Slide_space_val1].val, sizeof(&Slide_space[Slide_space_val1].val), &bw);
		f_read(file, Slide_space[Slide_space_val2].val, sizeof(&Slide_space[Slide_space_val2].val), &bw);
		f_read(file, Slide_space[Slide_space_val3].val, sizeof(&Slide_space[Slide_space_val3].val), &bw);
		f_read(file, Slide_space[Slide_space_Volume_Ctrl].val, sizeof(&Slide_space[Slide_space_Volume_Ctrl].val), &bw);
		// f_read(file, Slide_space[Slide_space_SPO2MinVal].val, sizeof(&Slide_space[Slide_space_SPO2MinVal].val), &bw);
		// f_read(file, Slide_space[Slide_space_HRMaxVal].val, sizeof(&Slide_space[Slide_space_HRMaxVal].val), &bw);
		// f_read(file, Slide_space[Slide_space_HRMinVal].val, sizeof(&Slide_space[Slide_space_HRMinVal].val), &bw);
	}
	f_close(file);
}
