#include "BluetoothManager.h"

// ==================== BLEManagerServerCallbacks ====================
// 连接/断开事件回调，仅记录状态 + 断开时自动重开广播

void BLEManagerServerCallbacks::onConnect(BLEServer* pServer) {
    *_pConnected = true;
    Serial.println("BLE client connected");
}

void BLEManagerServerCallbacks::onDisconnect(BLEServer* pServer) {
    *_pConnected = false;
    Serial.println("BLE client disconnected — restarting advertising");
    // 断开后重新开启广播，便于手机端重新扫描连接
    pServer->startAdvertising();
}

// ==================== BleManagerRxCallbacks ====================
// 收到手机端写入数据时，追加到接收缓冲区

void BleManagerRxCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
        *_pBuffer += String(value.c_str());  // 暂存缓冲，等待主循环 checkCommand 取走
    }
}

// ==================== BluetoothManager ====================

BluetoothManager::BluetoothManager(const char* deviceName)
    : _deviceName(deviceName)
    , _pServer(nullptr)
    , _pTxCharacteristic(nullptr)
    , _deviceConnected(false)
    , _rxBuffer("")
{}

void BluetoothManager::begin() {
    // 1. 初始化 BLE 协议栈，设置设备广播名
    BLEDevice::init(_deviceName);

    // 2. 创建 BLE 服务器，注册连接回调（记录 _deviceConnected 状态）
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new BLEManagerServerCallbacks(&_deviceConnected));

    // 3. 创建 Nordic UART Service (NUS)
    BLEService* pService = _pServer->createService(SERVICE_UUID);

    // 4. TX 特征：ESP32 → 手机，Notify 方式推送，需要 BL2902 描述符开启 CCCD
    _pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    _pTxCharacteristic->addDescriptor(new BLE2902());

    // 5. RX 特征：手机 → ESP32，Write 方式接收指令
    BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new BleManagerRxCallbacks(&_rxBuffer));

    // 6. 启动服务
    pService->start();

    // 7. 开始广播，让手机端能扫描到设备
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);         // 使 iOS 能正确读取设备名
    pAdvertising->setMinPreferred(0x06);          // 优化 iOS 连接速度
    pAdvertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("BLE server \"%s\" started, waiting for connections...\n", _deviceName);
}

void BluetoothManager::sendData(uint16_t pm25, uint16_t pm10, float temp, float hum, bool buzzerOn,
                                uint16_t voc, uint16_t hcho, uint16_t eco2,
                                uint16_t lux, uint8_t grade) {
    if (!_deviceConnected) return;  // 无客户端连接时直接跳过，避免空通知

    /**
     * TX 数据格式（标签格式，空格分隔）：
     * PM2.5:xx PM10:xx T:xx.x H:xx.x VOC:xx HCHO:xx eCO2:xx LUX:xx Grade:x Alarm:ON/OFF
     *
     * 使用固定栈缓冲区（128 字节）代替 String 堆分配，避免堆碎片。
     */
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
                       "PM2.5:%u PM10:%u T:%.1f H:%.1f "
                       "VOC:%u HCHO:%u eCO2:%u LUX:%u "
                       "Grade:%u Alarm:%s\r\n",
                       pm25, pm10, temp, hum,
                       voc, hcho, eco2, lux,
                       grade, buzzerOn ? "ON" : "OFF");

    _pTxCharacteristic->setValue(buf);
    _pTxCharacteristic->notify();   // 推送 Notification 给手机
}

char BluetoothManager::checkCommand() {
    /**
     * 从接收缓冲区取第一个字符作为指令。
     * 支持指令：
     *   '1' — 启用蜂鸣器
     *   '0' — 静音 10 分钟
     *   'R' — 强制刷新发送当前数据
     * 取完后从缓冲区移除该字符。
     */
    if (_rxBuffer.length() > 0) {
        char cmd = _rxBuffer.charAt(0);
        _rxBuffer.remove(0, 1);
        return cmd;
    }
    return 0;  // 无待处理指令
}
