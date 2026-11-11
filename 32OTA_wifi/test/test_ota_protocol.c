/*
 * 主机侧单元测试：OTA 协议层（CRC32 / 帧打包 / ACK 状态机）
 *
 * 编译运行（任意主机 C 编译器，如 gcc）：
 *   gcc -std=c11 -Wall -Wextra test/test_ota_protocol.c -o test/test_ota_protocol
 *   ./test/test_ota_protocol
 *
 * 说明：直接包含被测源文件 ota_protocol.c，无需 ESP-IDF 目标环境。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../main/protocol/ota_protocol.c"

/* ==================== 极简测试框架 ==================== */
static int s_checks = 0;
static int s_failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        s_checks++;                                                     \
        if (!(cond)) {                                                  \
            s_failures++;                                               \
            printf("    [FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

#define CHECK_EQ_INT(expected, actual)                                      \
    do {                                                                    \
        long _e = (long)(expected);                                         \
        long _a = (long)(actual);                                           \
        s_checks++;                                                         \
        if (_e != _a) {                                                     \
            s_failures++;                                                   \
            printf("    [FAIL] %s:%d  expected=%ld actual=%ld\n",           \
                   __FILE__, __LINE__, _e, _a);                             \
        }                                                                   \
    } while (0)

#define CHECK_EQ_U32(expected, actual)                                      \
    do {                                                                    \
        uint32_t _e = (uint32_t)(expected);                                 \
        uint32_t _a = (uint32_t)(actual);                                   \
        s_checks++;                                                         \
        if (_e != _a) {                                                     \
            s_failures++;                                                   \
            printf("    [FAIL] %s:%d  expected=0x%08X actual=0x%08X\n",     \
                   __FILE__, __LINE__, (unsigned)_e, (unsigned)_a);         \
        }                                                                   \
    } while (0)

typedef void (*test_fn_t)(void);

typedef struct {
    const char *name;
    test_fn_t   fn;
} test_case_t;

/* ==================== 测试用例 ==================== */
static void test_crc32_known_vector(void)
{
    /* 行业标准校验值：字符串 "123456789" 的 CRC-32 必须为 0xCBF43926 */
    CHECK_EQ_U32(0xCBF43926u, ota_crc32((const uint8_t *)"123456789", 9));
    /* 空数据的 CRC32 必须为 0 */
    CHECK_EQ_U32(0x00000000u, ota_crc32(NULL, 0));
}

static void test_crc32_incremental_equals_oneshot(void)
{
    uint8_t data[256];
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)i;

    uint32_t oneshot = ota_crc32(data, sizeof(data));

    ota_crc32_t crc;
    ota_crc32_init(&crc);
    ota_crc32_update(&crc, data, 100);
    ota_crc32_update(&crc, data + 100, 100);
    ota_crc32_update(&crc, data + 200, 56);
    uint32_t incremental = ota_crc32_finalize(&crc);

    CHECK_EQ_U32(oneshot, incremental);
}

static void test_crc32_null_safety(void)
{
    ota_crc32_t crc;

    /* NULL 指针不应崩溃，且返回值确定 */
    ota_crc32_init(NULL);
    ota_crc32_update(NULL, NULL, 0);
    CHECK_EQ_U32(0u, ota_crc32_finalize(NULL));

    /* 数据为空但长度非零时，update 应安全忽略 */
    ota_crc32_init(&crc);
    ota_crc32_update(&crc, NULL, 10);
    CHECK_EQ_U32(0u, ota_crc32_finalize(&crc));
}

static void test_frame_build_layout(void)
{
    uint8_t buf[64];
    const uint8_t data[] = {0x41, 0x42};

    size_t n = ota_frame_build(buf, OTA_TYPE_PREPARE, 0x1234, 0x89ABCDEFu, data, 2);
    CHECK_EQ_INT(18, n);

    /* 帧头 */
    CHECK_EQ_INT(0xAA, buf[0]);
    CHECK_EQ_INT(0x55, buf[1]);
    /* 类型 */
    CHECK_EQ_INT(OTA_TYPE_PREPARE, buf[2]);
    /* 序号（大端） */
    CHECK_EQ_INT(0x12, buf[3]);
    CHECK_EQ_INT(0x34, buf[4]);
    /* 偏移（大端） */
    CHECK_EQ_INT(0x89, buf[5]);
    CHECK_EQ_INT(0xAB, buf[6]);
    CHECK_EQ_INT(0xCD, buf[7]);
    CHECK_EQ_INT(0xEF, buf[8]);
    /* 长度（大端） */
    CHECK_EQ_INT(0x00, buf[9]);
    CHECK_EQ_INT(0x02, buf[10]);
    /* 数据 */
    CHECK_EQ_INT(0x41, buf[11]);
    CHECK_EQ_INT(0x42, buf[12]);
    /* 帧尾 */
    CHECK_EQ_INT(0x5A, buf[17]);

    /* CRC 字段一致性：buf[13..16] 必须等于 CRC32(buf[2..12]) */
    uint32_t expected_crc = ota_crc32(&buf[2], 11);
    uint32_t actual_crc = ((uint32_t)buf[13] << 24) | ((uint32_t)buf[14] << 16) |
                          ((uint32_t)buf[15] << 8)  | (uint32_t)buf[16];
    CHECK_EQ_U32(expected_crc, actual_crc);
}

static void test_frame_build_no_data(void)
{
    uint8_t buf[64];

    size_t n = ota_frame_build(buf, OTA_TYPE_FINISH, 5, 0, NULL, 0);
    CHECK_EQ_INT(16, n);

    /* 无数据时帧尾应在 buf[15] */
    CHECK_EQ_INT(0x5A, buf[15]);

    /* CRC 覆盖 type + seq(2) + offset(4) + len(2) = 9 字节 */
    uint32_t expected_crc = ota_crc32(&buf[2], 9);
    uint32_t actual_crc = ((uint32_t)buf[11] << 24) | ((uint32_t)buf[12] << 16) |
                          ((uint32_t)buf[13] << 8)  | (uint32_t)buf[14];
    CHECK_EQ_U32(expected_crc, actual_crc);
}

static void test_ack_parser_valid(void)
{
    ota_ack_parser_t p;
    ota_ack_parser_init(&p);

    const uint8_t frame[] = {OTA_MAGIC_0, OTA_MAGIC_1, OTA_TYPE_ACK, 0x00, 0x05};

    for (size_t i = 0; i < sizeof(frame) - 1; i++) {
        CHECK(!ota_ack_parser_feed(&p, frame[i], 5));
    }
    /* 最后一个字节应返回 true（序号 5 匹配） */
    CHECK(ota_ack_parser_feed(&p, frame[sizeof(frame) - 1], 5));
}

static void test_ack_parser_wrong_seq(void)
{
    ota_ack_parser_t p;
    ota_ack_parser_init(&p);

    const uint8_t frame[] = {OTA_MAGIC_0, OTA_MAGIC_1, OTA_TYPE_ACK, 0x00, 0x05};

    /* 期望序号 6，收到的却是 5，全程都应返回 false */
    for (size_t i = 0; i < sizeof(frame); i++) {
        CHECK(!ota_ack_parser_feed(&p, frame[i], 6));
    }
}

static void test_ack_parser_noise_resync(void)
{
    ota_ack_parser_t p;
    ota_ack_parser_init(&p);

    /* 前面混入噪声字节，应能重新同步到帧头 */
    const uint8_t noise[] = {0x00, 0xFF, 0x13};
    for (size_t i = 0; i < sizeof(noise); i++) {
        CHECK(!ota_ack_parser_feed(&p, noise[i], 7));
    }

    const uint8_t frame[] = {OTA_MAGIC_0, OTA_MAGIC_1, OTA_TYPE_ACK, 0x00, 0x07};
    for (size_t i = 0; i < sizeof(frame) - 1; i++) {
        CHECK(!ota_ack_parser_feed(&p, frame[i], 7));
    }
    CHECK(ota_ack_parser_feed(&p, frame[sizeof(frame) - 1], 7));
}

static void test_ack_parser_magic0_within_frame(void)
{
    ota_ack_parser_t p;
    ota_ack_parser_init(&p);

    /* 帧头 MAGIC1 之后又出现 MAGIC0（0xAA 0x55 0xAA ...），应能继续识别完整 ACK */
    const uint8_t frame[] = {OTA_MAGIC_0, OTA_MAGIC_1, OTA_MAGIC_0,
                             OTA_MAGIC_1, OTA_TYPE_ACK, 0x00, 0x07};

    for (size_t i = 0; i < sizeof(frame) - 1; i++) {
        CHECK(!ota_ack_parser_feed(&p, frame[i], 7));
    }
    CHECK(ota_ack_parser_feed(&p, frame[sizeof(frame) - 1], 7));
}

/* ==================== 运行入口 ==================== */
int main(void)
{
    test_case_t cases[] = {
        {"crc32_known_vector",            test_crc32_known_vector},
        {"crc32_incremental_equals_oneshot", test_crc32_incremental_equals_oneshot},
        {"crc32_null_safety",             test_crc32_null_safety},
        {"frame_build_layout",            test_frame_build_layout},
        {"frame_build_no_data",           test_frame_build_no_data},
        {"ack_parser_valid",              test_ack_parser_valid},
        {"ack_parser_wrong_seq",          test_ack_parser_wrong_seq},
        {"ack_parser_noise_resync",       test_ack_parser_noise_resync},
        {"ack_parser_magic0_within_frame", test_ack_parser_magic0_within_frame},
    };

    size_t total = sizeof(cases) / sizeof(cases[0]);

    for (size_t i = 0; i < total; i++) {
        int before = s_failures;
        cases[i].fn();
        printf("[%s] %s\n", (s_failures == before) ? "PASS" : "FAIL", cases[i].name);
    }

    printf("\n总计: %d 项断言, %d 失败\n", s_checks, s_failures);
    return (s_failures == 0) ? 0 : 1;
}