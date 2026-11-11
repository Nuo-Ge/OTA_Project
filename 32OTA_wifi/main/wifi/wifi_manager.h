#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化 Wi-Fi 并连接 AP，连接失败时自动回退到 BLE 配网
 *
 * 内部流程（时序固定，不能颠倒）：
 *   1. 初始化 netif / 事件循环 / 默认 STA 网络接口
 *   2. 初始化 Wi-Fi 并启动（必须在配网前启动，配网期间才能验证连接）
 *   3. 初始化 network_prov_mgr（BLE 方案 + FREE_BTDM 释放钩子）
 *   4. 循环直到连接成功：
 *      - 已有凭据：尝试连接，单次超时后自动进入 BLE 配网
 *      - 无凭据：直接进入 BLE 配网
 *      - BLE 配网结束后回到循环顶部重新检测凭据并重试连接
 *   5. network_prov_mgr_deinit()：100% 释放 BLE/BT 栈，避免与 TLS/HTTP 死锁
 *
 * @return true  已成功连接并获取 IP（仅在连接成功后返回）
 */
bool wifi_init_sta(void);

/**
 * @brief  启动重新配网按键检测任务
 *
 * BOOT 键（GPIO0）长按 3 秒 → 擦除 NVS 中的 Wi-Fi 凭据 → 重启进入配网模式
 */
void wifi_reconfig_button_start(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */