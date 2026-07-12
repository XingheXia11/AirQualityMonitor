#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Nordic UART Service (NUS) UUIDs — standard, supported by most BLE terminal apps
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

/// BLE Server callback — tracks connection state
class BLEManagerServerCallbacks : public BLEServerCallbacks {
    bool* _pConnected;
public:
    BLEManagerServerCallbacks(bool* pConnected) : _pConnected(pConnected) {}
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
};

/// BLE RX Characteristic callback — receives commands from phone
class BleManagerRxCallbacks : public BLECharacteristicCallbacks {
    String* _pBuffer;
public:
    BleManagerRxCallbacks(String* pBuffer) : _pBuffer(pBuffer) {}
    void onWrite(BLECharacteristic* pCharacteristic) override;
};

class BluetoothManager {
public:
    BluetoothManager(const char* deviceName);
    void begin();
    void sendData(uint16_t pm25, uint16_t pm10, float temp, float hum, bool buzzerOn,
                  uint16_t voc, uint16_t hcho, uint16_t eco2,
                  uint16_t lux = 0, uint8_t grade = 0);
    char checkCommand();

private:
    const char* _deviceName;
    BLEServer* _pServer;
    BLECharacteristic* _pTxCharacteristic;
    bool _deviceConnected;
    String _rxBuffer;
};

#endif
