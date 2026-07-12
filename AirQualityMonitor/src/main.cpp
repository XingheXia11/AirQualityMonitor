/**
 * @file main.cpp
 * @brief 主程序入口 — 多传感器空气质量监测仪
 *
 * 整体架构：所有传感器均为非阻塞读取，每次 loop() 都尝试消费串口缓冲区的最新数据。
 * OLED 刷新和 BLE 发送则按各自节拍（500ms / 1000ms）分频执行，避免屏幕闪烁和蓝牙拥塞。
 */

#include <Arduino.h>
#include <Wire.h>

// 引入所有自定义驱动
#include "VOC21Driver.h"
#include "BH1750Driver.h"
#include "OLEDDriver.h"
#include "pms70xx.h"
#include "AlarmManager.h"
#include "BluetoothManager.h"

// ================= 引脚定义 =================
#define PIN_PMS_RX   16   // PMS7003 颗粒物传感器 UART RX
#define PIN_PMS_TX   17   // PMS7003 颗粒物传感器 UART TX
#define PIN_BUZZER   25   // 蜂鸣器报警输出（GPIO 直驱，低电平触发）
#define PIN_BTN_MUTE 0    // 静音按键（复用核心板 BOOT 按钮，长按 1s 切换）
#define PIN_VOC21_RX 18   // 21VOC 五合一（VOC/甲醛/CO₂/温湿度）UART RX
#define PIN_VOC21_TX 19   // 21VOC 五合一 UART TX

// ================= 全局对象 =================
// 各驱动类的构造函数在进入 setup() 前完成，begin() 方法在 setup() 中调用以初始化硬件
VOC21Driver voc21(PIN_VOC21_RX, PIN_VOC21_TX);     // 五合一环境传感器
BH1750Driver bh1750(0x23);                         // 光照传感器（I2C 地址 0x23）
OLEDDriver oled;                                    // SSD1306 OLED 显示屏
AlarmManager alarmMgr(PIN_BUZZER, PIN_BTN_MUTE);    // 蜂鸣器 + 静音按键管理
BluetoothManager bluetooth("AirQualityMonitor");    // BLE 蓝牙通信（NUS 服务）

// PMS7003 帧解析器状态（全局，跨 loop 调用保持解析进度）
pms70xx_parser_t pms_parser;
PMS7003_framestruct pms_frame;

// ================= 共享状态变量 =================
// 所有传感器的最新读数，由 loop() 持续更新，OLED/BLE/报警判断从中读取
float temperature = 0;
float humidity = 0;
uint16_t pm25 = 0;
uint16_t pm10 = 0;
uint16_t pm1_0 = 0;
uint16_t voc = 0;
uint16_t hcho = 0;
uint16_t eco2 = 0;
uint16_t lux = 0;

// 报警阈值
const int PM25_THRESHOLD    = 75;    // PM2.5 报警上限 (μg/m³) — 超过此值触发蜂鸣器
const int VOC_THRESHOLD     = 500;   // VOC  报警上限 (μg/m³) — 超过此值触发蜂鸣器

// ================= 辅助函数 =================

/**
 * @brief 空气质量等级信息
 */
struct GradeInfo {
    const char* label;
    uint8_t     code;
};

static const GradeInfo GRADE_TABLE[] = {
    {"Good",     0},
    {"Fair",     1},
    {"LightPol", 2},
    {"ModPol",   3},
    {"HeavyPol", 4},
};

/** PM2.5 分级上限（与 GRADE_TABLE 下标一一对应，末项为哨兵） */
static const uint16_t GRADE_BOUNDS[] = {35, 75, 115, 150};

/**
 * @brief 根据 PM2.5 浓度查表获取等级信息
 */
static const GradeInfo& lookupGrade(uint16_t pm25) {
    int i = 0;
    while (i < 4 && pm25 > GRADE_BOUNDS[i]) i++;
    return GRADE_TABLE[i];
}

/**
 * @brief 根据光照强度获取天气标签
 */
const char* getWeatherStr(uint16_t lux) {
    if (lux > 30000) return "Sunny";
    if (lux > 10000) return "Cloudy";
    return "Overcast";
}

/**
 * @brief 系统初始化，上电后执行一次
 *
 * 初始化顺序很重要：
 * 1. 调试串口 → 2. I2C 总线（OLED / BH1750 共用） → 3. OLED（可显示启动画面）
 * 4-6. 各传感器 → 7. 报警器 → 8. 蓝牙（最后，确保此前硬件已就绪）
 */
void setup() {
  // 0. 第一时间关断蜂鸣器（GPIO 直驱低电平触发：HIGH = 关）
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, HIGH);

  // 1. 调试串口：115200 baud，用于串口监视器查看日志
  Serial.begin(115200);

  // 2. I2C 总线：OLED (0x3C) 与 BH1750 (0x23) 共享 SDA=21 / SCL=22
  Wire.begin(21, 22);

  // 3. OLED 显示屏：初始化后显示启动画面，提示用户设备正在启动
  oled.begin();
  oled.showStartup();

  // 4. 21VOC 五合一（VOC / HCHO / eCO₂ / 温湿度），UART 9600
  voc21.begin();

  // 5. BH1750 光照传感器，通过 I2C 读取环境光照强度（lux）
  bh1750.begin();

  // 6. PMS7003 颗粒物传感器，UART 9600，逐字节解析状态机
  Serial2.begin(9600, SERIAL_8N1, PIN_PMS_RX, PIN_PMS_TX);
  pms70xx_parser_init(&pms_parser);   // 重置帧解析器状态

  // 7. 报警管理器：初始化蜂鸣器引脚 + 静音按键，默认蜂鸣器关闭
  alarmMgr.begin();

  // 8. 蓝牙 BLE：启动 NUS 服务，等待手机连接
  bluetooth.begin();
  Serial.println("Bluetooth Ready!");
}

/**
 * @brief 主循环 — 非阻塞「读传感器 → 判报警 → 刷显示 → 发数据」流水线
 *
 * 这是一个协作式任务调度循环，关键设计原则：
 *
 * 1. 传感器全部非阻塞读取，每次 loop() 都从串口缓冲区消费最新数据
 * 2. 耗时操作（OLED 渲染、BLE 发送）按各自节拍（500ms / 1000ms）分频，
 *    用 `millis()` 差值判断"到时间了没"，绝不调用 delay() 阻塞
 * 3. 这样 loop() 每秒能执行数千次，UART 缓冲区始终保持低位，避免丢失数据
 */
void loop() {
  static unsigned long lastDisplayTime = 0;   // 上次 OLED 刷新时间点
  static unsigned long lastBleTime = 0;       // 上次 BLE 发送时间点
  unsigned long now = millis();               // 当前时间戳（上电以来 ms）

  // ===================== 1. 处理蓝牙指令 =====================
  // 每次循环检查手机端发来的指令（非阻塞），确保及时响应
  char cmd = bluetooth.checkCommand();
  if (cmd != 0) {
      switch (cmd) {
      case '1':                               // 手机端：启用蜂鸣器
        alarmMgr.setMuted(false);
        break;
      case '0':                               // 手机端：静音 10 分钟
        alarmMgr.setMuted(true);
        break;
      case 'R':                               // 手机端：强制刷新一次数据推送
        bluetooth.sendData(pm25, pm10, temperature, humidity, !alarmMgr.isMuted(),
                           voc, hcho, eco2, lux, lookupGrade(pm25).code);
        break;
    }
  }

  // ===================== 2. 读取传感器数据 =====================
  // 所有传感器的读取逻辑：
  //   - UART 类（PMS7003、21VOC）：传感器主动上报，我们只管消费缓冲区
  //   - I2C 类（BH1750）：主控主动发起请求，但间隔读取避免占用 I2C 总线

  // --- 2a. PMS7003 颗粒物传感器 ---
  // 输出 @ 9600 baud，每秒约 10 帧；逐字节送入状态机，解析出一帧立即更新全局变量
  while (Serial2.available() > 0) {
      uint8_t b = Serial2.read();
      if (pms70xx_parse_byte(&pms_parser, b, &pms_frame)) {
          // 成功解析一帧，更新 PM1.0 / PM2.5 / PM10 读数
          pm25 = pms_frame.concPM2_5_amb;
          pm10 = pms_frame.concPM10_0_amb;
          pm1_0 = pms_frame.concPM1_0_amb;
      }
  }

  // --- 2b. 21VOC 五合一传感器 ---
  // 内部同样以 UART 解析最新帧，调用 update() 后通过 getter 取出各指标
  voc21.update();
  temperature = voc21.getTemperature();
  humidity    = voc21.getHumidity();
  voc         = voc21.getVOC();       // TVOC 总挥发性有机物 (μg/m³)
  hcho        = voc21.getHCHO();      // 甲醛浓度 (μg/m³)
  eco2        = voc21.getECO2();      // 等效 CO₂ 浓度 (ppm)

  // --- 2c. BH1750 光照传感器 ---
  // 光照变化较慢，每 500ms 读取一次即可节省 I2C 总线开销
  static unsigned long lastLuxTime = 0;
  if (now - lastLuxTime >= 500) {
      lastLuxTime = now;
      lux = bh1750.readLux();         // 原始值 ÷1.2 → lux（整数运算 raw*10/12）
  }

  // ===================== 3. 报警判断 =====================
  // 任一关键指标超阈值 → 蜂鸣器鸣叫；AlarmManager 内部还处理静音计时
  bool shouldAlarm = (pm25 > PM25_THRESHOLD || voc > VOC_THRESHOLD);
  alarmMgr.update(shouldAlarm);

  // ===================== 4. OLED 屏幕刷新 =====================
  // 500ms 刷新一次，避免屏幕闪烁，同时减少 I2C 写入频率
  if (now - lastDisplayTime >= 500) {
    lastDisplayTime = now;

    // 查表得出当前空气质量等级（Good / Fair / LightPol / ModPol / HeavyPol）
    const GradeInfo& grade = lookupGrade(pm25);
    // 根据光照强度给出天气标签（Sunny / Cloudy / Overcast）
    const char* weather = getWeatherStr(lux);

    oled.showData(pm25, pm10, pm1_0,
                  temperature, humidity, voc, hcho, eco2,
                  lux, grade.label, weather,
                  alarmMgr.isAlarmActive(), alarmMgr.isMuted());
  }

  // ===================== 5. 数据输出（串口 + BLE） =====================
  // 每秒汇总推送一次，兼顾日志可读性与蓝牙信道负载
  if (now - lastBleTime >= 1000) {
    lastBleTime = now;

    // 串口日志：供调试时通过 USB 监视器查看
    Serial.printf("PM1.0=%d PM2.5=%d PM10=%d T=%.1f H=%.1f VOC=%d HCHO=%d eCO2=%d LUX=%d Grade=%s\n",
                  pm1_0, pm25, pm10, temperature, humidity, voc, hcho, eco2, lux, lookupGrade(pm25).label);

    // BLE 推送：发送标签格式字符串给手机端 App
    bluetooth.sendData(pm25, pm10, temperature, humidity, !alarmMgr.isMuted(),
                       voc, hcho, eco2, lux, lookupGrade(pm25).code);
  }
}