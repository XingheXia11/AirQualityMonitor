#ifndef PMS70XX_H
#define PMS70XX_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief PMS7003 数据帧结构（32 字节）
 *
 * 对应传感器主动上报的串口数据帧，大端字节序。
 * 帧头 [0x42, 0x4D]，frameLen = 0x001C（28 字节数据 + 2 字节校验）。
 * 校验：frameHeader[0]..errorCode 共 30 字节累加和 == checksum。
 *
 * @note 传感器同时输出两种 PM 浓度：
 *   - CF1（标准粒子计数）：工厂标准环境下校准值
 *   - amb（大气环境）：根据当前温湿度修正后的值，更贴近真实环境
 *   本项目中只使用 amb 系列。
 */
typedef struct {
    uint8_t  frameHeader[2];    // 帧头固定 [0x42, 0x4D]
    uint16_t frameLen;          // 数据长度 = 0x001C（28 字节）
    uint16_t concPM1_0_CF1;    // PM1.0 浓度 — CF=1 标准环境 (μg/m³)
    uint16_t concPM2_5_CF1;    // PM2.5 浓度 — CF=1 标准环境 (μg/m³)
    uint16_t concPM10_0_CF1;   // PM10  浓度 — CF=1 标准环境 (μg/m³)
    uint16_t concPM1_0_amb;    // PM1.0 浓度 — 大气环境 (μg/m³)
    uint16_t concPM2_5_amb;    // PM2.5 浓度 — 大气环境 (μg/m³) ◀ 主要关注
    uint16_t concPM10_0_amb;   // PM10  浓度 — 大气环境 (μg/m³)
    uint16_t rawGt0_3um;       // ≥0.3μm 颗粒物个数（每 0.1L 空气）
    uint16_t rawGt0_5um;       // ≥0.5μm 颗粒物个数
    uint16_t rawGt1_0um;       // ≥1.0μm 颗粒物个数
    uint16_t rawGt2_5um;       // ≥2.5μm 颗粒物个数
    uint16_t rawGt5_0um;       // ≥5.0μm 颗粒物个数
    uint16_t rawGt10_0um;      // ≥10μm 颗粒物个数
    uint8_t  version;           // 固件版本号
    uint8_t  errorCode;         // 错误码（0=正常）
    uint16_t checksum;          // 校验和（前 30 字节累加）
} PMS7003_framestruct;

// 解析器状态
typedef enum {
    PMS_STATE_WAIT_START1,
    PMS_STATE_WAIT_START2,
    PMS_STATE_READ_LEN_H,
    PMS_STATE_READ_LEN_L,
    PMS_STATE_READ_DATA,
} pms70xx_state_t;

typedef struct {
    pms70xx_state_t state;
    uint8_t buffer[32];
    uint8_t index;
    uint16_t frame_length;
    uint16_t checksum;
} pms70xx_parser_t;

#ifdef __cplusplus
extern "C" {
#endif

void pms70xx_parser_init(pms70xx_parser_t *parser);

/**
 * @brief 喂入一个字节，大端解析
 * @return true 表示收到完整有效帧
 */
bool pms70xx_parse_byte(pms70xx_parser_t *parser, uint8_t byte, PMS7003_framestruct *frame);

/** @brief 直接从完整 32 字节帧解析（备用） */
bool pms70xx_parse_frame(const uint8_t raw32[32], PMS7003_framestruct *frame);

#ifdef __cplusplus
}
#endif

#endif
