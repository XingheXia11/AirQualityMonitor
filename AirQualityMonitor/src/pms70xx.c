#include "pms70xx.h"
#include <string.h>

#define FRAME_EXPECTED_LEN  28           // 数据段长度（帧头后至校验前）
#define FRAME_TOTAL_SIZE    32           // 完整帧字节数

/* 大端读取 uint16_t */
static inline uint16_t be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

/**
 * @brief 将 32 字节原始帧 buffer 解析到 struct（公共提取，消除重复）
 */
static void pms70xx_populate_frame(const uint8_t *b, PMS7003_framestruct *frame) {
    frame->frameHeader[0] = b[0];
    frame->frameHeader[1] = b[1];
    frame->frameLen       = be16(b + 2);
    frame->concPM1_0_CF1 = be16(b + 4);
    frame->concPM2_5_CF1 = be16(b + 6);
    frame->concPM10_0_CF1= be16(b + 8);
    frame->concPM1_0_amb = be16(b + 10);
    frame->concPM2_5_amb = be16(b + 12);
    frame->concPM10_0_amb= be16(b + 14);
    frame->rawGt0_3um    = be16(b + 16);
    frame->rawGt0_5um    = be16(b + 18);
    frame->rawGt1_0um    = be16(b + 20);
    frame->rawGt2_5um    = be16(b + 22);
    frame->rawGt5_0um    = be16(b + 24);
    frame->rawGt10_0um   = be16(b + 26);
    frame->version       = b[28];
    frame->errorCode     = b[29];
    frame->checksum      = be16(b + 30);
}

void pms70xx_parser_init(pms70xx_parser_t *parser) {
    parser->state = PMS_STATE_WAIT_START1;
    parser->index = 0;
    parser->frame_length = 0;
    parser->checksum = 0;
}

bool pms70xx_parse_byte(pms70xx_parser_t *parser, uint8_t byte, PMS7003_framestruct *frame) {
    /**
     * 逐字节状态机，5 个状态依次流转：
     *   WAIT_START1 ─[0x42]→ WAIT_START2 ─[0x4D]→ READ_LEN_H → READ_LEN_L → READ_DATA
     * 任何一步收到意外字节都会回退到 WAIT_START1 重新同步。
     */
    switch (parser->state) {
        case PMS_STATE_WAIT_START1:
            // 等待帧头第一字节 0x42（字符 'B'）
            if (byte == 0x42) {
                parser->buffer[0] = byte;
                parser->index = 1;
                parser->checksum = byte;      // 开始累加校验和
                parser->state = PMS_STATE_WAIT_START2;
            }
            break;

        case PMS_STATE_WAIT_START2:
            // 等待帧头第二字节 0x4D（字符 'M'）
            if (byte == 0x4D) {
                parser->buffer[1] = byte;
                parser->checksum += byte;
                parser->index = 2;
                parser->state = PMS_STATE_READ_LEN_H;
            } else {
                parser->state = PMS_STATE_WAIT_START1;  // 没等到 "BM"，复位
            }
            break;

        case PMS_STATE_READ_LEN_H:
            // 帧长度高字节（大端）
            parser->buffer[2] = byte;
            parser->checksum += byte;
            parser->frame_length = (uint16_t)byte << 8;
            parser->index = 3;
            parser->state = PMS_STATE_READ_LEN_L;
            break;

        case PMS_STATE_READ_LEN_L:
            // 帧长度低字节 → 拼出完整 frameLen，判断是否等于 0x001C
            parser->buffer[3] = byte;
            parser->checksum += byte;
            parser->frame_length |= byte;
            parser->index = 4;
            parser->state = (parser->frame_length == FRAME_EXPECTED_LEN)
                                ? PMS_STATE_READ_DATA     // 长度正确，继续读数据
                                : PMS_STATE_WAIT_START1;  // 长度异常，重新同步
            break;

        case PMS_STATE_READ_DATA:
            // 逐字节读入剩余数据（共 32 字节）
            parser->buffer[parser->index] = byte;
            parser->checksum += byte;
            parser->index++;
            if (parser->index >= FRAME_TOTAL_SIZE) {
                /**
                 * 收满一帧 → 校验
                 * 技巧：之前已经累加了所有 32 字节（含 checksum），
                 * 减去校验字段自身的值即可得到前 30 字节的累加和，
                 * 省去一次循环。
                 */
                uint16_t sum = parser->checksum -
                               (uint16_t)(parser->buffer[30] + parser->buffer[31]);
                uint16_t chk = be16(parser->buffer + 30);
                parser->state = PMS_STATE_WAIT_START1;  // 收完即复位，准备下一帧
                if (sum != chk) break;                   // 校验失败，丢弃

                // 校验通过 → 填入结构体返回
                pms70xx_populate_frame(parser->buffer, frame);
                return true;
            }
            break;
    }
    return false;
}

bool pms70xx_parse_frame(const uint8_t raw32[32], PMS7003_framestruct *frame) {
    if (raw32[0] != 0x42 || raw32[1] != 0x4D) return false;
    if (be16(raw32 + 2) != FRAME_EXPECTED_LEN) return false;

    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += raw32[i];
    uint16_t chk = be16(raw32 + 30);
    if (sum != chk) return false;

    pms70xx_populate_frame(raw32, frame);
    return true;
}
