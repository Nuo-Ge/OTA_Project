#ifndef __CLOUD_MQTT_H__
#define __CLOUD_MQTT_H__

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化华为云 IoTDA MQTT 客户端并启动连接
 *
 * 云端设备信息（设备 ID / 密钥 / 接入地址 / 端口）通过
 * `idf.py menuconfig`（见 Kconfig.projbuild）配置，禁止在源码中硬编码。
 *
 * @return true  初始化成功，已启动连接
 * @return false 云端配置无效或初始化失败
 */
bool cloud_mqtt_init(void);

/**
 * @brief 向华为云 IoTDA 上报 OTA 状态
 *
 * @param status 状态字符串，例如 "idle" / "downloading" / "flashing_50%" /
 *               "success" / "failed"
 */
void cloud_mqtt_report_status(const char *status);

/**
 * @brief 阻塞等待云端下发的 OTA 固件 URL
 *
 * @param url_buffer 输出缓冲区
 * @param max_len    缓冲区大小
 * @return true  成功获取 URL
 * @return false 获取失败（当前实现会一直阻塞等待）
 */
bool cloud_mqtt_wait_for_ota_url(char *url_buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __CLOUD_MQTT_H__ */