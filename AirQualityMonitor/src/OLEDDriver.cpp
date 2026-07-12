#include "OLEDDriver.h"

OLEDDriver::OLEDDriver() : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET) {}

void OLEDDriver::begin() {
    // 初始化失败则死循环（串口输出不可用，只能靠肉眼判断）
    if(!_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while(1);
    }
    _display.clearDisplay();
    _display.setTextSize(1);       // 5×7 像素字符
    _display.setTextColor(SSD1306_WHITE);
}

void OLEDDriver::showStartup() {
    _display.clearDisplay();
    _display.setCursor(0,0);
    _display.println("Booting...");
    _display.display();
    delay(200);     // 短暂展示后快速进入主循环
}

void OLEDDriver::showData(uint16_t pm25, uint16_t pm10, uint16_t pm1_0,
                           float temp, float hum,
                           uint16_t voc, uint16_t hcho, uint16_t eco2,
                           uint16_t lux, const char* grade, const char* weather,
                           bool alarmActive, bool isMuted) {
    // 屏幕 64 像素高，每行 13 像素，可容纳 5 行文本
    const int ROW1 = 0;    // Y=0:  空气质量等级 + 天气
    const int ROW2 = 13;   // Y=13: PM1.0 / PM2.5 / PM10
    const int ROW3 = 26;   // Y=26: VOC / HCHO
    const int ROW4 = 39;   // Y=39: eCO₂ / Lux
    const int ROW5 = 52;   // Y=52: 温度 / 湿度

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // ---- 第一行：空气质量等级 + 天气 ----
    _display.setCursor(0, ROW1);
    _display.print("AQ:");
    _display.print(grade);
    if(alarmActive) {
        _display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);  // 反白显示 ALARM
        _display.print(" ALARM");
        _display.setTextColor(SSD1306_WHITE);
    } else if (isMuted) {
        _display.print(" MUTED");
    }
    _display.setCursor(72, ROW1);
    _display.print(weather);

    // ---- 第二行：PM 颗粒物浓度 ----
    _display.setCursor(0, ROW2);
    _display.print("P1:"); _display.print(pm1_0);
    _display.setCursor(42, ROW2);
    _display.print("P2:"); _display.print(pm25);
    _display.setCursor(84, ROW2);
    _display.print("P10:"); _display.print(pm10);

    // ---- 第三行：VOC + 甲醛 ----
    _display.setCursor(0, ROW3);
    _display.print("VOC:"); _display.print(voc);
    _display.setCursor(72, ROW3);
    _display.print("HCHO:"); _display.print(hcho);

    // ---- 第四行：CO₂ + 光照 ----
    _display.setCursor(0, ROW4);
    _display.print("eCO2:"); _display.print(eco2);
    _display.setCursor(66, ROW4);
    _display.print("Lux:"); _display.print(lux);

    // ---- 第五行：温湿度 ----
    _display.setCursor(0, ROW5);
    _display.print("T:"); _display.print(temp, 1);
    _display.setCursor(72, ROW5);
    _display.print("H:"); _display.print(hum, 1); _display.print("%");

    _display.display();
}
