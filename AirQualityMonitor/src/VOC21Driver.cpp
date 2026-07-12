#include "VOC21Driver.h"

VOC21Driver::VOC21Driver(uint8_t rxPin, uint8_t txPin)
    : _rxPin(rxPin), _txPin(txPin) {
    // Serial1 在 begin() 中初始化
}

void VOC21Driver::begin() {
    _serial = &Serial1;
    _serial->begin(9600, SERIAL_8N1, _rxPin, _txPin);

    // 清空缓冲区中的旧数据
    while (_serial->available()) {
        _serial->read();
    }

    _bufferIndex = 0;
    _dataValid = false;
}

void VOC21Driver::update() {
    // 消费串口所有可用字节，逐字节送入帧解析缓冲区
    while (_serial->available()) {
        uint8_t b = _serial->read();

        // 帧头重同步：如果当前不是在等待帧头，却收到了 0x2C，
        // 说明上个帧的某个尾部字节丢了，立即重置从头开始
        if (b == FRAME_HEADER && _bufferIndex != 0) {
            _bufferIndex = 0;
        }

        if (_bufferIndex == 0) {
            // 状态 0：等待帧头 0x2C
            if (b == FRAME_HEADER) {
                _buffer[0] = b;
                _bufferIndex = 1;
            }
            // 非帧头字节直接丢弃
        } else {
            // 已收到帧头，持续读取后续 11 个负载字节
            if (_bufferIndex < FRAME_SIZE) {
                _buffer[_bufferIndex] = b;
            }
            _bufferIndex++;

            // 收满完整一帧（12 字节）→ 校验 + 解析
            if (_bufferIndex >= FRAME_SIZE) {
                if (parseFrame(_buffer)) {
                    _dataValid = true;
                    _lastValidTime = millis();
                }
                // 无论校验是否通过，都清空缓冲区等待下一帧
                _bufferIndex = 0;
            }
        }
    }
}

bool VOC21Driver::parseFrame(const uint8_t* frame) {
    // 校验和：前 11 字节累加，取补码（使得整帧 12 字节之和 mod 256 = 0）
    if (calcChecksum(frame) != frame[11]) {
        return false;
    }

    // 解析数据（大端：高字节在前）
    _voc    = (uint16_t)frame[1] << 8 | frame[2];
    _hcho   = (uint16_t)frame[3] << 8 | frame[4];
    _eco2   = (uint16_t)frame[5] << 8 | frame[6];

    // 温度：16 位有符号值（补码），除以 10 得 ℃
    int16_t rawTemp = (int16_t)((uint16_t)frame[7] << 8 | frame[8]);
    _temperature = rawTemp / 10.0f;

    // 湿度：16 位无符号值，除以 10 得 %RH
    uint16_t rawHumi = (uint16_t)frame[9] << 8 | frame[10];
    _humidity    = rawHumi / 10.0f;

    return true;
}

uint8_t VOC21Driver::calcChecksum(const uint8_t* frame) const {
    uint8_t sum = 0;
    for (int i = 0; i < FRAME_SIZE - 1; i++) {
        sum += frame[i];
    }
    // 补码校验：校验字节应使得所有 12 字节累加和低 8 位为 0
    return (uint8_t)((256 - sum) % 256);
}

uint32_t VOC21Driver::getLastUpdateAge() const {
    if (!_dataValid) return UINT32_MAX;
    return millis() - _lastValidTime;
}
