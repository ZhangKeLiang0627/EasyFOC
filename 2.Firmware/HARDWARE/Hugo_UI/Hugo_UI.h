#ifndef __HUGO_UI_H
#define __HUGO_UI_H

#ifdef __cplusplus
extern "C"
{
#endif

/*User_Add*/
/*解决关键字不识别问题*/
#include "stm32f4xx.h" // Device header
#include "delay.h"
#include "oled.h"
#include "OLED_User.h"
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>

/* 定义参数 */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FONT_WIDTH 8
#define FONT_HEIGHT 15
#define SCROLL_BAR_WIDTH 3

/* USER_NEED_TO_FILL_THIESE_FUNCTION */
#define HugoUIScreenInit() (OLED_Init())
#define HugoUIDisplayStr(x, y, str) (Oled_u8g2_ShowStr(x, y, str))
#define HugoUIDisplayFloat(x, y, dat, num, pointNum) (Oled_u8g2_ShowFloat(x, y, dat, num, pointNum))
#define HugoUIDrawDot(x, y) (Oled_u8g2_DrawPoint(x, y))
#define HugoUIDrawBox(x, y, width, height) (Oled_u8g2_DrawBox(x, y, width, height))
#define HugoUIDrawFrame(x, y, width, height) (Oled_u8g2_DrawFrame(x, y, width, height))
#define HugoUIDrawRFrame(x, y, width, height, r) (Oled_u8g2_DrawRFrame(x, y, width, height, r))
#define HugoUIDrawRBox(x, y, width, height, r) (Oled_u8g2_DrawRBox(x, y, width, height, r))
#define HugoUIClearBuffer() (Oled_u8g2_ClearBuffer())
#define HugoUISendBuffer() (Oled_u8g2_SendBuffer())
#define HugoUISetDrawColor(mode) (Oled_u8g2_SetDrawColor(mode))
#define HugoUIDisplayBMP(x, y, width, height, pic) (Oled_u8g2_ShowBMP(x, y, width, height, pic))
#define HugoUIModifyColor(mode) (Oled_u8g2_ModifyColor(mode))
#define HugoUIShowUTF8(x, y, str) (Oled_u8g2_ShowUTF8(x, y, str))

#ifdef FPU
    // typedef     double      paramType;
    typedef float paramType; // 先使用int32_t
#else
typedef int32_t paramType; // 先使用int32_t
#endif

    /* UI_STATE 描述当前UI运行状态 */
    typedef enum
    {
        STATE_NONE = 0,      // 当前无状态
        STATE_RUN_PAGE_DOWN, // 当前正在运行菜单
        STATE_RUN_PAGE_UP,
        STATE_READY_TO_JUMP_PAGE, // 准备跳转状态 在此状态设置跳转参数
        STATE_JUMP_PAGE,          // 当前进行页面跳转
        STATE_JUMP_PAGE_ARRIVE,   // 当前页面跳转结束
    } HugoUIState_e;

    /* Item的functype类型 */
    typedef enum
    {
        ITEM_PAGE_DESCRIPTION, // 此页的描述
        ITEM_CALL_FUNCTION,    // 此挂件打开就是函数
        ITEM_JUMP_PAGE,        // 跳转的下一页or上一页
        ITEM_SWITCH,           // 开关on&off
        ITEM_CHANGE_VALUE,     // 改变值
        ITEM_CHECKBOX,
        ITEM_MESSAGE,
    } HugoUIItem_e;

    /* Page的functype类型 */
    typedef enum
    {
        PAGE_LIST,   // 该页是列表
        PAGE_CUSTOM, // 该页是定做的(icon)
    } HugoUIPage_e;

    /* Item的结构体 */
    typedef struct HugoUI_item // 小挂件的结构体
    {
        struct HugoUI_item *next;
        HugoUIItem_e funcType; // 作用类型
        uint8_t itemId;        // 小挂件的id
        uint8_t lineId;        // 在每一页的id
        // float item_x, item_x_trg;
        // float item_y, item_y_trg;
        uint8_t *pic;
        // uint8_t step;
        char *title; // 小挂件的名字
        char *msg;   // ITEM_MESSAGE
        char *desc;
        bool *flag; // ITEM_CHECKBOX and ITEM_RADIO_BUTTON and ITEM_SWITCH //小挂件用于这些作用时的标志位
        // bool flagDefault; // Factory default setting//恢复出厂设置
        paramType *param;                                                     // ITEM_CHANGE_VALUE and ITEM_PROGRESS_BAR//小挂件可改变的参数
        uint8_t inPage;                                                       // ITEM_JUMP_PAGE//Item在哪一页
        uint8_t JumpPage;                                                     // 将要跳转到哪一个page
        uint8_t JumpItem;                                                     // 将要跳转到哪一个Item
        void (*FuncCallBack)(void);                                           // 回调函数// ITEM_CHANGE_VALUE and ITEM_PROGRESS_BAR//该挂件的函数
        struct HugoUI_item *(*SetIconSrc)(const uint8_t *pic);                // 传入图片
        struct HugoUI_item *(*SetJumpId)(uint8_t pageId, uint8_t itemLineId); // 传入PageId和ItemLineId
        struct HugoUI_item *(*SetDescripition)(char *desc);                   // 传入descripition
        struct HugoUI_item *(*ReturnThisItem)(struct HugoUI_item *thisItem);  // 传入descripition
    } HugoUIItem_t;

    /* Page的结构体 */
    typedef struct HugoUI_page
    {
        struct HugoUI_page *next;
        HugoUIPage_e funcType;
        HugoUIItem_t *itemHead, *itemTail;
        char *title; // 该页的名字
        uint8_t pageId;
        uint8_t itemMax; // 该page含有的item数
        float page_x, page_x_trg;
        float page_y, page_y_trg;
        float page_y_forlist, page_y_forlist_trg;
        void (*FuncCallBack)(void);
        void (*PgaeUIShow)(struct HugoUI_page *thispage, HugoUIItem_t *thisitem);
        struct HugoUI_page *(*SetPgaeUIShow)(void (*PgaeUIShow)(struct HugoUI_page *thispage, HugoUIItem_t *thisitem));
        HugoUIItem_t *(*AddItem)(struct HugoUI_page *thisPage, char *title, HugoUIItem_e itemType, ...);
    } HugoUIPage_t;

    /* API-Function */
    void HugoUI_System(void);

    void HugoUI_Control(void);

    void HugoUI_JumpPage_ShowLastPage(HugoUIPage_t *lastPage);

    HugoUIItem_t *ReturnThisItem(HugoUIItem_t *thisItem);

    HugoUIItem_t *SetJumpId(uint8_t pageId, uint8_t itemLineId);

    HugoUIItem_t *SetIconSrc(const uint8_t *pic);

    HugoUIItem_t *SetDescripition(char *desc);

    HugoUIItem_t *AddItem(HugoUIPage_t *thisPage, char *title, HugoUIItem_e itemType, ...);

    HugoUIPage_t *AddPage(HugoUIPage_e mode, char *name);

    uint16_t findParam_forChangeVal(float *param);

    uint8_t HugoUI_Run(short *a, short *a_trg, u8 step, u8 slow_cnt);

    uint8_t HugoUI_Animation_Liner(float *a, float *a_trg, uint8_t n);
    uint8_t HugoUI_Animation_EasyIn(float *a, float *a_trg, uint16_t n);
    uint8_t HugoUI_Animation_EasyOut(float *a, float *a_trg, uint16_t n);
    uint8_t HugoUI_Animation_Blur(void);
    /* 在user.c中完善枚举enum Slide_space_Obj 该枚举中每一个枚举名按顺序对应Slide_space结构体数组即可*/

    /**
     *	@名称 Slide_Bar
     *	@变量
     *		x    值
     *		min  最小值
     *		max  最大值
     *		step 步进
     */
    struct Slide_Bar
    {
        float *val; // 值
        float min;
        float max;
        float step;
    };

    extern struct Slide_Bar Slide_space[];

    /* 声明变量 */
    extern char *HugoUIVersion;
    extern HugoUIPage_t *pageHead, *pageTail;
    extern HugoUIItem_t *ItemHead, *ItemTail;

    extern uint8_t KeyNum;
    extern uint8_t EncoderNum;
    // extern uint8_t KeyNum0, KeyNum1;

#ifdef __cplusplus
}
#endif

#endif
