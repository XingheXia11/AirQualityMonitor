#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define SCREEN_WIDTH  128   // OLED 横向像素数
#define SCREEN_HEIGHT  64   // OLED 纵向像素数
#define OLED_RESET    -1    // 复位引脚（-1 = 与主控共用）

/**
 * @brief SSD1306 OLED 128×64 显示驱动
 *
 * 负责将各传感器读数布局到屏幕上，每行 13 像素高，共 5 行。
 * 使用 Adafruit_SSD1306 + Adafruit_GFX 库。
 */
class OLEDDriver {
public:
    OLEDDriver();

    /** @brief 初始化 I2C OLED，设置默认文字尺寸与颜色 */
    void begin();

    /** @brief 显示启动画面 "Booting..." */
    void showStartup();

    /** @brief 刷新生效数据（5 行布局），由主循环每 500ms 调用 */
    void showData(uint16_t pm25, uint16_t pm10, uint16_t pm1_0,
                  float temp, float hum,
                  uint16_t voc, uint16_t hcho, uint16_t eco2,
                  uint16_t lux, const char* grade, const char* weather,
                  bool alarmActive, bool isMuted);

private:
    Adafruit_SSD1306 _display;
};

#endif