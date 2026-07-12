#include "BH1750Driver.h"

// BH1750 I2C 指令集（仅列出本驱动使用的）
#define BH1750_POWER_ON          0x01   // 上电
#define BH1750_RESET             0x07   // 复位（清除上次测量结果）
#define BH1750_CONT_H_RES_MODE   0x10   // 连续高分辨率模式，测量范围 1~65535 lux

BH1750Driver::BH1750Driver(uint8_t addr)
    : _addr(addr) {}

void BH1750Driver::begin() {
    // 检查设备是否在 I2C 总线上应答
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) {
        Serial.println("BH1750: device not found!");
        return;
    }
    // 上电 → 短暂稳定 → 设为连续高分辨率模式，然后等待首次测量完成（约 180ms）
    writeCmd(BH1750_POWER_ON);
    delay(10);
    writeCmd(BH1750_CONT_H_RES_MODE);
    delay(180);
    Serial.println("BH1750: ready");
}

void BH1750Driver::writeCmd(uint8_t cmd) {
    Wire.beginTransmission(_addr);
    Wire.write(cmd);
    Wire.endTransmission();
}

uint16_t BH1750Driver::readLux() {
    Wire.requestFrom(_addr, (uint8_t)2);
    if (Wire.available() < 2) return 0;  // 无应答或数据未就绪

    uint16_t raw = (Wire.read() << 8) | Wire.read();
    /**
     * 高分辨率模式下，原始值 ÷ 1.2 即为 lux。
     * 这里用整数运算避免浮点：raw × 10 ÷ 12
     * 例如 raw=120 → 1200/12=100 lux，精度 1lux 足够日常使用。
     */
    return (uint16_t)((raw * 10) / 12);
}
