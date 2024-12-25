#ifndef __LED_H
#define __LED_H
#include "sys.h"

// LED端口定义
#define LED0 PCout(13)

// LED初始化
void LED_Init(void);

#endif
