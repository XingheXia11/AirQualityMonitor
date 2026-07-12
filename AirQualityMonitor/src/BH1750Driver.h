#ifndef BH1750DRIVER_H
#define BH1750DRIVER_H

#include <Arduino.h>
#include <Wire.h>

/**
 * @brief BH1750 光强度传感器驱动（I2C）
 *
 * 数字式环境光传感器，直接输出 lux（勒克斯）。
 * 使用连续高分辨率模式，测量范围 1 ~ 65535 lux。
 */
class BH1750Driver {
public:
    BH1750Driver(uint8_t addr = 0x23);

    /**
     * @brief 初始化传感器并上电
     */
    void begin();

    /**
     * @brief 读取当前光照强度
     * @return 光照值 (lux)，失败返回 0
     */
    uint16_t readLux();

private:
    uint8_t _addr;

    /** @brief 发送 I2C 命令 */
    void writeCmd(uint8_t cmd);
};

#endif // BH1750DRIVER_H
