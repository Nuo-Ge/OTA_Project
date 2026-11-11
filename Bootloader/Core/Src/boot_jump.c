#include "boot_jump.h"
#include "main.h"
#include "boot_config.h"

/* 检查 App 是否合法 */
uint8_t Boot_CheckApp(uint32_t app_addr) {
    uint32_t stack_top     = *(volatile uint32_t*)app_addr;
    uint32_t reset_handler = *(volatile uint32_t*)(app_addr + 4);

    /* 栈顶地址必须在 SRAM 范围：0x20000000 ~ 0x20004FFF */
    if ((stack_top & 0x2FFE0000) != 0x20000000) {
        return 0;
    }

    /* ResetHandler 必须在 Flash 范围 */
    if (reset_handler < 0x08000000 || reset_handler > 0x08010000) {
        return 0;
    }

    return 1;
}

/* 跳转到 App */
void Boot_JumpToApp(uint32_t app_addr) {
    typedef void (*pFunction)(void);
    pFunction JumpToApplication;
    uint32_t JumpAddress;

    if (Boot_CheckApp(app_addr) == 0) {
        return;  /* App 无效，不跳转 */
    }

    /* 1. 关闭所有中断 */
    __disable_irq();

    /* 2. 关闭 SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* 3. 清除所有中断使能与挂起标志 */
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* 4. 重定位向量表到 App */
    SCB->VTOR = app_addr;

    /* 5. 设置主栈指针为 App 的栈顶 */
    __set_MSP(*(volatile uint32_t*)app_addr);

    /* 6. 取出 App 的 ResetHandler 并跳转 */
    JumpAddress = *(volatile uint32_t*)(app_addr + 4);
    JumpToApplication = (pFunction)JumpAddress;
    JumpToApplication();

    while (1) {}
}