#ifndef __BOOT_JUMP_H__
#define __BOOT_JUMP_H__

#include <stdint.h>

/* 检查 App 是否合法（栈顶 + ResetHandler 地址） */
uint8_t Boot_CheckApp(uint32_t app_addr);

/* 跳转到 App（不返回，除非跳转失败） */
void Boot_JumpToApp(uint32_t app_addr);

#endif