/*
 * Cardputer ADV - BLE Slave (客户端)
 * 连接到 S3R-M12 并显示接收到的时间
 */

#include <Arduino.h>
#include <M5Cardputer.h>
#include <M5Unified.h>
#include <NimBLEDevice.h>

// BLE 配置 - 连接到 S3R-M12
#define TARGET_DEVICE_NAME "S3R-M12"
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

static BLEClient* pClient = nullptr;
static BLERemoteService* pRemoteService = nullptr;
static BLERemoteCharacteristic* pRemoteTxCharacteristic = nullptr;

bool connected = false;
String receivedTime = "Waiting...";
int reconnectAttempts = 0;
const int MAX_RECONNECT = 5;

// 屏幕状态
enum ScreenState {
  SCAN,
  CONNECTING,
  CONNECTED,
  FAILED
};

ScreenState currentState = SCAN;

void drawScreen();

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("Connected to server");
    connected = true;
    reconnectAttempts = 0;
    currentState = CONNECTED;
    drawScreen();
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("Disconnected from server");
    connected = false;
    currentState = SCAN;
    drawScreen();
  }
};

// RX 回调 - 接收时间数据
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  String data = "";
  for (size_t i = 0; i < length; i++) {
    data += (char)pData[i];
  }
  
  Serial.print("RX: ");
  Serial.println(data);
  
  receivedTime = data;
  currentState = CONNECTED;
  drawScreen();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========== Cardputer ADV BLE Start ==========");
  
  // M5Cardputer 初始化
  auto cfg = M5.config();
  cfg.fallback_board = m5gfx::board_t::board_M5CardputerADV;
  M5Cardputer.begin(cfg);
  
  // 开启背光（亮度一半）
  M5Cardputer.Display.setBrightness(127);
  
  // 屏幕旋转（横屏）
  M5Cardputer.Display.setRotation(1);
  
  Serial.printf("Screen: %d x %d\n", M5Cardputer.Display.width(), M5Cardputer.Display.height());
  
  drawScreen();
  
  // 初始化 NimBLE
  BLEDevice::init("Cardputer-Client");
  Serial.println("BLE Client initialized");
  
  // 开始扫描
  startScan();
}

void startScan() {
  Serial.println("Scanning for S3R-M12...");
  currentState = SCAN;
  drawScreen();
  
  NimBLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  pScan->start(3, true);
}

void connectToServer(BLEAdvertisedDevice& device) {
  Serial.println("Found S3R-M12! Connecting...");
  currentState = CONNECTING;
  drawScreen();
  
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  pClient->connect(&device);
  delay(1000);
  
  if (pClient->isConnected()) {
    Serial.println("Connected to BLE Server");
    pRemoteService = pClient->getService(NimBLEUUID(SERVICE_UUID));
    if (pRemoteService != nullptr) {
      pRemoteTxCharacteristic = pRemoteService->getCharacteristic(NimBLEUUID(CHARACTERISTIC_UUID_TX));
      if (pRemoteTxCharacteristic != nullptr && pRemoteTxCharacteristic->canNotify()) {
        pRemoteTxCharacteristic->subscribe(true, notifyCallback);
        Serial.println("Subscribed to notifications");
      }
    }
    connected = true;
    currentState = CONNECTED;
    reconnectAttempts = 0;
  } else {
    Serial.println("Failed to connect");
    currentState = FAILED;
    reconnectAttempts++;
  }
  drawScreen();
}

void drawScreen() {
  int w = M5Cardputer.Display.width();
  int h = M5Cardputer.Display.height();
  
  M5Cardputer.Display.fillScreen(BLACK);
  M5Cardputer.Display.setTextDatum(top_left);
  
  // 标题
  M5Cardputer.Display.setTextColor(GREEN);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.drawString("BLE Client", 5, 5);
  
  // 目标设备
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.drawString("Target:", 5, 20);
  M5Cardputer.Display.setTextColor(CYAN);
  M5Cardputer.Display.drawString(TARGET_DEVICE_NAME, 55, 20);
  
  // 连接状态
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.drawString("Status:", 5, 35);
  
  switch (currentState) {
    case SCAN:
      M5Cardputer.Display.setTextColor(YELLOW);
      M5Cardputer.Display.drawString("Scanning...", 55, 35);
      break;
    case CONNECTING:
      M5Cardputer.Display.setTextColor(ORANGE);
      M5Cardputer.Display.drawString("Connecting...", 55, 35);
      break;
    case CONNECTED:
      M5Cardputer.Display.setTextColor(GREEN);
      M5Cardputer.Display.drawString("Connected", 55, 35);
      break;
    case FAILED:
      M5Cardputer.Display.setTextColor(RED);
      M5Cardputer.Display.drawString("Failed", 55, 35);
      break;
  }
  
  // 分割线
  M5Cardputer.Display.drawLine(5, 50, w-5, 50, 0x7BEF);
  
  // 接收到的数据
  M5Cardputer.Display.setTextColor(WHITE);
  M5Cardputer.Display.drawString("Received:", 5, 58);
  
  // 解析并显示时间
  if (receivedTime.startsWith("Time:")) {
    int timeStart = receivedTime.indexOf("Time:") + 5;
    int timeEnd = receivedTime.indexOf("\n", timeStart);
    if (timeEnd == -1) timeEnd = receivedTime.length();
    
    String timeStr = receivedTime.substring(timeStart, timeEnd);
    timeStr.trim();
    
    // 大字显示时间
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.drawString(timeStr, 5, 75);
    
    // 显示完整信息（WiFi状态等）
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(0xAD55);
    
    int y = 105;
    int lineStart = 0;
    String rest = receivedTime.substring(timeEnd + 1);
    rest.trim();
    
    if (rest.length() > 0) {
      M5Cardputer.Display.drawString(rest, 5, y);
    }
  } else {
    M5Cardputer.Display.setTextColor(0x8410);
    M5Cardputer.Display.drawString(receivedTime, 5, 75);
  }
  
  // 重试次数
  if (reconnectAttempts > 0) {
    M5Cardputer.Display.setTextColor(RED);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("Retries: " + String(reconnectAttempts), 5, h - 10);
  }
}

void loop() {
  M5Cardputer.update();
  
  if (!connected && reconnectAttempts < MAX_RECONNECT) {
    NimBLEScanResults results = BLEDevice::getScan()->getResults();
    
    for (int i = 0; i < results.getCount(); i++) {
      BLEAdvertisedDevice device = results.getDevice(i);
      String name = device.getName().c_str();
      
      Serial.printf("Found: %s\n", name.c_str());
      
      if (name == TARGET_DEVICE_NAME) {
        BLEDevice::getScan()->stop();
        connectToServer(device);
        break;
      }
    }
    
    if (!connected) {
      BLEDevice::getScan()->clearResults();
      delay(500);
    }
  } else if (reconnectAttempts >= MAX_RECONNECT) {
    currentState = FAILED;
    drawScreen();
    delay(3000);
    reconnectAttempts = 0;
  }
  
  if (connected && pClient != nullptr) {
    if (!pClient->isConnected()) {
      Serial.println("Connection lost");
      connected = false;
      currentState = SCAN;
      drawScreen();
    }
  }
  
  delay(100);
}
