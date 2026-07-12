#ifndef VOC21DRIVER_H
#define VOC21DRIVER_H

#include <Arduino.h>

/**
 * @brief 21VOC 五合一空气质量传感器驱动
 *
 * 该模块通过 UART（9600,8,N,1）主动上报 12 字节数据帧：
 *   - VOC (μg/m³)
 *   - 甲醛 HCHO (μg/m³)
 *   - eCO₂ (ppm)
 *   - 温度 (℃, 需除10，有符号补码)
 *   - 湿度 (%RH, 需除10)
 *
 * 帧头 0x2C，校验方式为前 11 字节累加后取补码（使整帧 12 字节之和 mod 256 = 0）。
 */
class VOC21Driver {
public:
    VOC21Driver(uint8_t rxPin, uint8_t txPin);

    /**
     * @brief 初始化串口并启动接收缓冲区
     */
    void begin();

    /**
     * @brief 从串口缓冲区解析最新一帧有效数据
     *
     * 应在每次主循环中调用。内部持续消费串口数据，
     * 缓存最新一帧通过校验的数据。
     */
    void update();

    /** @brief 获取最近一次有效的 VOC 值 (μg/m³) */
    uint16_t getVOC() const { return _voc; }

    /** @brief 获取最近一次有效的甲醛浓度 (μg/m³) */
    uint16_t getHCHO() const { return _hcho; }

    /** @brief 获取最近一次有效的 eCO₂ 浓度 (ppm) */
    uint16_t getECO2() const { return _eco2; }

    /** @brief 获取最近一次有效的温度值 (℃) */
    float getTemperature() const { return _temperature; }

    /** @brief 获取最近一次有效的湿度值 (%RH) */
    float getHumidity() const { return _humidity; }

    /** @brief 检查是否已收到至少一帧有效数据 */
    bool isDataValid() const { return _dataValid; }

    /** @brief 获取上次数据更新距今的毫秒数 */
    uint32_t getLastUpdateAge() const;

private:
    static const int FRAME_SIZE = 12;
    static const uint8_t FRAME_HEADER = 0x2C;

    HardwareSerial* _serial;
    uint8_t _rxPin;
    uint8_t _txPin;

    // 缓存的最新有效值
    uint16_t _voc = 0;
    uint16_t _hcho = 0;
    uint16_t _eco2 = 0;
    float _temperature = 0;
    float _humidity = 0;

    bool _dataValid = false;
    uint32_t _lastValidTime = 0;

    // 帧解析缓冲区
    uint8_t _buffer[FRAME_SIZE];
    uint8_t _bufferIndex = 0;

    /**
     * @brief 校验并解析一帧数据
     * @return true 如果校验通过且解析成功
     */
    bool parseFrame(const uint8_t* frame);

    /**
     * @brief 计算校验和（前 11 字节累加取补码，使整帧 12 字节之和 mod 256 = 0）
     */
    uint8_t calcChecksum(const uint8_t* frame) const;
};

#endif // VOC21DRIVER_H
