#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <Arduino.h>

/**
 * @brief 报警管理器 — 蜂鸣器 + 静音按键
 *
 * 蜂鸣器：GPIO 直驱有源蜂鸣器，低电平触发（LOW=响，HIGH=关）。
 * 直接 digitalWrite 控制，不依赖 tone/LEDC PWM。
 *
 * 两个独立状态机：
 *
 * 1. 按键状态机（非阻塞）：
 *    - BOOT 按钮（GPIO0）按下为 LOW
 *    - 长按 1s 切换静音，10 分钟自动恢复
 *
 * 2. 报警状态机（非阻塞）：
 *    - 首次触发：200ms 持续鸣响 → 随后 500ms 响/500ms 停循环
 *    - 条件解除立即停止
 *    - 被静音时不发声
 */
class AlarmManager {
public:
    AlarmManager(int buzzerPin, int btnPin);
    void begin();

    void update(bool isAlarmConditionMet);

    bool isMuted() const { return !_buzzerEnabled; }
    bool isAlarmActive() const { return _alarmActive; }
    void setMuted(bool muted);

private:
    int _buzzerPin;
    int _btnPin;

    // ---- 静音状态 ----
    bool _buzzerEnabled;
    bool _alarmActive;
    unsigned long _muteStartTime;
    static constexpr unsigned long MUTE_DURATION = 600000; // 10 分钟

    // ---- 按键状态机 ----
    static constexpr unsigned long HOLD_MS = 1000;
    bool _lastBtnReading;
    bool _holdConfirmed;
    unsigned long _lastBtnTime;

    // ---- 报警进入提示音（off→on 瞬间滴滴两声） ----
    static constexpr unsigned long ENTRY_BEEP_MS = 200;  // 每声持续 200ms
    static constexpr unsigned long ENTRY_GAP_MS  = 100;  // 两声间隔 100ms
    int _entryBeepPhase;          // 0=空闲, 1=滴1, 2=间隔, 3=滴2, 4=停顿→正常报警
    unsigned long _entryBeepTime; // 当前阶段起始时刻

    // ---- 报警状态机 ----
    static constexpr unsigned long ALARM_FIRST_TONE_MS = 200;
    static constexpr unsigned long ALARM_HALF_CYCLE_MS = 500;
    unsigned long _alarmStartTime;
    unsigned long _lastToggleTime;
    bool _toneOn;

    void handleButton();
    bool _processEntryBeeps(unsigned long now); // true=提示音进行中, false=已结束

    // 有源蜂鸣器：LOW=响, HIGH=关
    void _buzzerOn()  { digitalWrite(_buzzerPin, LOW); }
    void _buzzerOff() { digitalWrite(_buzzerPin, HIGH); }
};

#endif
