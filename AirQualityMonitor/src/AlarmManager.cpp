#include "AlarmManager.h"

AlarmManager::AlarmManager(int buzzerPin, int btnPin)
    : _buzzerPin(buzzerPin), _btnPin(btnPin)
    , _buzzerEnabled(true), _alarmActive(false)
    , _muteStartTime(0)
    , _lastBtnReading(HIGH), _holdConfirmed(false), _lastBtnTime(0)
    , _entryBeepPhase(0), _entryBeepTime(0)
    , _alarmStartTime(0), _lastToggleTime(0), _toneOn(false)
{}

void AlarmManager::begin() {
    pinMode(_buzzerPin, OUTPUT);
    _buzzerOff();                       // HIGH = 关
    pinMode(_btnPin, INPUT_PULLUP);     // BOOT 按钮，按下 = LOW
}

void AlarmManager::setMuted(bool muted) {
    _buzzerEnabled = !muted;
    if (muted) {
        _buzzerOff();
        _alarmActive = false;
        _muteStartTime = millis();
    } else {
        // 蓝牙恢复蜂鸣器，滴滴两声确认
        if (_entryBeepPhase == 0) {
            _entryBeepPhase = 1;
            _entryBeepTime = millis();
            _buzzerOn();
        }
    }
}

void AlarmManager::handleButton() {
    unsigned long now = millis();
    bool reading = (digitalRead(_btnPin) == LOW);

    if (reading != _lastBtnReading) {
        _lastBtnTime = now;
        _lastBtnReading = reading;
        _holdConfirmed = false;
    }

    if (reading && !_holdConfirmed && (now - _lastBtnTime >= HOLD_MS)) {
        _holdConfirmed = true;
        if (_buzzerEnabled) {
            _buzzerEnabled = false;
            _buzzerOff();
            _alarmActive = false;
            _muteStartTime = now;
        } else {
            // 当前静音 → 恢复发声，滴滴两声提醒用户蜂鸣器已启用
            _buzzerEnabled = true;
            if (_entryBeepPhase == 0) {
                _entryBeepPhase = 1;
                _entryBeepTime = now;
                _buzzerOn();
            }
        }
    }

    if (!_buzzerEnabled && (now - _muteStartTime > MUTE_DURATION)) {
        _buzzerEnabled = true;
    }
}

// ---- 报警进入提示音（off→on 瞬间滴滴两声） ----

bool AlarmManager::_processEntryBeeps(unsigned long now) {
    switch (_entryBeepPhase) {
    case 1:  // 滴 1 鸣响中
        if (now - _entryBeepTime >= ENTRY_BEEP_MS) {
            _buzzerOff();
            _entryBeepPhase = 2;
            _entryBeepTime = now;
        }
        return true;

    case 2:  // 两声间隔（静音）
        if (now - _entryBeepTime >= ENTRY_GAP_MS) {
            _buzzerOn();
            _entryBeepPhase = 3;
            _entryBeepTime = now;
        }
        return true;

    case 3:  // 滴 2 鸣响中
        if (now - _entryBeepTime >= ENTRY_BEEP_MS) {
            _buzzerOff();
            _entryBeepPhase = 4;
            _entryBeepTime = now;
        }
        return true;

    case 4:  // 提示音结束
        _entryBeepPhase = 0;
        if (_alarmActive) {
            // 报警触发的滴滴 → 接入正常报警节奏
            _alarmStartTime = now;
            _lastToggleTime = now;
            _toneOn = true;
            _buzzerOn();                    // 200ms 持续蜂鸣
        } else {
            // 手动恢复触发的滴滴 → 静音
            _buzzerOff();
        }
        return false;                       // 提示音序列完成

    default:
        return false;
    }
}

// ---- 报警状态机 ----

void AlarmManager::update(bool isAlarmConditionMet) {
    unsigned long now = millis();

    handleButton();

    if (!_buzzerEnabled) {
        _buzzerOff();
        _alarmActive = false;
        _entryBeepPhase = 0;                // 静音时中断提示音
        return;
    }

    // ---- 滴滴提示音处理（优先级最高，报警/unmute 共用） ----
    if (_entryBeepPhase > 0) {
        if (_processEntryBeeps(now)) {
            return;                         // 提示音进行中，本帧跳过后续逻辑
        }
        // 提示音刚结束，fall through 检查报警状态
    }

    if (isAlarmConditionMet) {
        // ---- off→on 首次触发：启动滴滴提示音 ----
        if (!_alarmActive && _entryBeepPhase == 0) {
            _alarmActive = true;
            _entryBeepPhase = 1;            // 开始滴滴
            _entryBeepTime = now;
            _buzzerOn();
            return;                         // 下一帧开始处理提示音
        }

        // ---- 正常报警节奏 ----
        if (now - _alarmStartTime < ALARM_FIRST_TONE_MS) {
            // 阶段 1：[0, 200ms) 维持持续鸣响
            if (!_toneOn) {
                _toneOn = true;
                _buzzerOn();
            }
        } else {
            // 阶段 2：[200ms, ∞) 500ms 响 / 500ms 停循环
            if (now - _lastToggleTime >= ALARM_HALF_CYCLE_MS) {
                _lastToggleTime += ALARM_HALF_CYCLE_MS;
                _toneOn = !_toneOn;
                if (_toneOn) {
                    _buzzerOn();
                } else {
                    _buzzerOff();
                }
            }
        }
    } else {
        // ---- 报警条件解除 ----
        _buzzerOff();
        _alarmActive = false;
        _entryBeepPhase = 0;                // 清理提示音状态
    }
}
