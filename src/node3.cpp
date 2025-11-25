/*
 * NODE 3 - ESP32 + Buzzer
 * Gửi dữ liệu lên ThingsBoard và cảnh báo lũ lụt qua còi
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ===== CẤU HÌNH BUZZER =====
// Chọn loại buzzer: 1 = ACTIVE (chỉ HIGH/LOW), 0 = PASSIVE (cần PWM/tone)
#define BUZZER_IS_ACTIVE 1

#define BUZZER_PIN 15
#define BUZZER_CHANNEL 0
#define BUZZER_FREQUENCY 2000
#define BUZZER_RESOLUTION 8

// ===== CẤU HÌNH WIFI =====
#define WIFI_SSID "Đăng"
#define WIFI_PASSWORD "11111111"

// ===== CẤU HÌNH THINGSBOARD =====
#define TB_SERVER "eu.thingsboard.cloud"
#define TB_TOKEN "WGPaVOGsYE0w66HVpaUH"
#define TB_PORT 1883

// ===== CẤU HÌNH CẢNH BÁO =====
#define ALARM_DURATION 5000
#define ALARM_PAUSE 3000
#define ALARM_PATTERN_BEEP 200
#define ALARM_PATTERN_PAUSE 100

// ===== NGƯỠNG CẢNH BÁO =====
#define DANGER_RAIN_LEVEL 80.0
#define DANGER_WATER_LEVEL 80.0

// Đối tượng
WebServer server(80);
WiFiClient tbClient;
PubSubClient tbMqtt(tbClient);

// Biến lưu dữ liệu cảm biến
float rainLevel = 0;
float waterLevel = 0;
float waterLevelCm = 0;
int waterAlert = 0;
int rainDigital = 0;

// Trạng thái còi và chế độ
bool buzzerActive = false;
bool autoMode = true;
bool manualBuzzer = false;
unsigned long alarmStartTime = 0;
unsigned long lastBeepTime = 0;
bool isBeeping = false;

unsigned long lastSendTime = 0;
const long sendInterval = 3000;

// Forward declarations
void controlBuzzer(bool state);
void alarmPattern();

// ===== HTTP SERVER - Nhận dữ liệu từ Node 1 & 2 =====
void handleData() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }
  
  String body = server.arg("plain");
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, body);
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  int nodeId = doc["nodeId"];
  const char* type = doc["type"];
  float value = doc["value"];
  
  Serial.println("=== Nhận dữ liệu HTTP ===");
  Serial.print("Từ Node: ");
  Serial.println(nodeId);
  Serial.print("Loại: ");
  Serial.println(type);
  Serial.print("Giá trị: ");
  Serial.println(value);
  
  // Lưu dữ liệu
  if (nodeId == 1) {
    rainLevel = value;
    rainDigital = doc["digital"];
    Serial.print("Mưa: ");
    Serial.print(rainLevel);
    Serial.println("%");
  } else if (nodeId == 2) {
    waterLevel = value;
    waterAlert = doc["alert"];
    waterLevelCm = doc["waterCm"];
    Serial.print("Nước: ");
    Serial.print(waterLevel);
    Serial.print("% (");
    Serial.print(waterLevelCm);
    Serial.println(" cm)");
  }
  
  server.send(200, "application/json", "{\"status\":\"OK\"}");
  Serial.println("✓ Đã nhận và xử lý\n");
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Node 3 - Flood Warning</title>";
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;background:#f0f0f0}";
  html += ".container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}";
  html += "h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px}";
  html += ".data{margin:15px 0;padding:10px;background:#f9f9f9;border-radius:5px}";
  html += ".label{font-weight:bold;color:#555}";
  html += ".value{color:#000;font-size:18px}";
  html += ".status{display:inline-block;padding:5px 10px;border-radius:5px;margin-left:10px}";
  html += ".status.on{background:#4CAF50;color:white}";
  html += ".status.off{background:#ccc;color:#666}";
  html += ".mode{background:#2196F3;color:white;padding:8px 15px;border-radius:5px;display:inline-block;margin-top:10px}";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>Hệ Thống Cảnh Báo Lũ Lụt</h1>";
  
  html += "<div class='data'><span class='label'>Mức độ mưa:</span> ";
  html += "<span class='value'>" + String(rainLevel, 1) + "%</span></div>";
  
  html += "<div class='data'><span class='label'>Mức nước:</span> ";
  html += "<span class='value'>" + String(waterLevel, 1) + "% (" + String(waterLevelCm, 1) + " cm)</span></div>";
  
  html += "<div class='data'><span class='label'>Còi:</span> ";
  html += "<span class='status " + String(buzzerActive ? "on" : "off") + "'>";
  html += buzzerActive ? "ĐANG KÊU" : "TẮT";
  html += "</span></div>";
  
  html += "<div class='data'><span class='mode'>";
  html += autoMode ? "Chế độ: TỰ ĐỘNG" : "Chế độ: THỦ CÔNG";
  html += "</span></div>";
  
  html += "<div style='margin-top:20px;padding:10px;background:#e3f2fd;border-radius:5px'>";
  html += "<strong>IP Address:</strong> " + WiFi.localIP().toString() + "<br>";
  html += "<strong>WiFi RSSI:</strong> " + String(WiFi.RSSI()) + " dBm";
  html += "</div>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Not found\"}");
}

// ===== THINGSBOARD =====
void tbCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("=== Nhận RPC từ ThingsBoard ===");
  
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  if (error) {
    Serial.println("*** Lỗi parse JSON");
    return;
  }
  
  const char* method = doc["method"];
  
  if (method && String(method) == "setAutoMode") {
    bool newMode = doc["params"].as<bool>();
    autoMode = newMode;
    
    Serial.print("Chuyển chế độ: ");
    Serial.println(autoMode ? "TỰ ĐỘNG" : "THỦ CÔNG");
    
    if (!autoMode) {
      controlBuzzer(false);
    }
    
    // Response
    StaticJsonDocument<100> respDoc;
    respDoc["mode"] = autoMode ? "AUTO" : "MANUAL";
    char respBuffer[100];
    serializeJson(respDoc, respBuffer);
    
    String respTopic = String(topic);
    respTopic.replace("request", "response");
    tbMqtt.publish(respTopic.c_str(), respBuffer);
  }
  else if (method && String(method) == "setBuzzer") {
    if (!autoMode) {
      bool buzzerState = doc["params"].as<bool>();
      manualBuzzer = buzzerState;
      controlBuzzer(buzzerState);
      
      Serial.print("Lệnh THỦ CÔNG: ");
      Serial.println(buzzerState ? "BẬT còi" : "TẮT còi");
      
      // Response
      StaticJsonDocument<100> respDoc;
      respDoc["buzzer"] = buzzerState ? "ON" : "OFF";
      char respBuffer[100];
      serializeJson(respDoc, respBuffer);
      
      String respTopic = String(topic);
      respTopic.replace("request", "response");
      tbMqtt.publish(respTopic.c_str(), respBuffer);
    } else {
      Serial.println("*** Chỉ điều khiển khi ở chế độ THỦ CÔNG!");
    }
  }
}

void setupWiFi() {
  Serial.println("Đang kết nối WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n*** WiFi đã kết nối");
    Serial.print("IP Address Node 3: ");
    Serial.println(WiFi.localIP());
    Serial.println("\n** Cập nhật IP này vào Node 1 & Node 2! **\n");
  } else {
    Serial.println("\n*** Không thể kết nối WiFi!");
  }
}

void reconnectThingsBoard() {
  while (!tbMqtt.connected()) {
    Serial.print("Đang kết nối ThingsBoard...");
    
    if (tbMqtt.connect("ESP32_Gateway", TB_TOKEN, NULL)) {
      Serial.println("*** ThingsBoard đã kết nối");
      tbMqtt.subscribe("v1/devices/me/rpc/request/+");
      Serial.println("*** Đã subscribe RPC");
    } else {
      Serial.print("*** Thất bại, rc=");
      Serial.println(tbMqtt.state());
      delay(5000);
    }
  }
}

// Điều khiển buzzer: nếu ACTIVE thì digitalWrite, nếu PASSIVE thì PWM (ledc)
void controlBuzzer(bool state) {
#if BUZZER_IS_ACTIVE
  digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
  buzzerActive = state;
#else
  if (state) {
    ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQUENCY);
    ledcWrite(BUZZER_CHANNEL, 128); // duty
    buzzerActive = true;
  } else {
    ledcWrite(BUZZER_CHANNEL, 0);
    buzzerActive = false;
  }
#endif
}

void alarmPattern() {
  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - alarmStartTime;
  
  if (elapsedTime < ALARM_DURATION) {
    if (!isBeeping && (currentTime - lastBeepTime >= ALARM_PATTERN_PAUSE)) {
#if BUZZER_IS_ACTIVE
      digitalWrite(BUZZER_PIN, HIGH);
#else
      ledcWriteTone(BUZZER_CHANNEL, BUZZER_FREQUENCY);
      ledcWrite(BUZZER_CHANNEL, 128);
#endif
      isBeeping = true;
      lastBeepTime = currentTime;
    } else if (isBeeping && (currentTime - lastBeepTime >= ALARM_PATTERN_BEEP)) {
#if BUZZER_IS_ACTIVE
      digitalWrite(BUZZER_PIN, LOW);
#else
      ledcWrite(BUZZER_CHANNEL, 0);
#endif
      isBeeping = false;
      lastBeepTime = currentTime;
    }
  } else if (elapsedTime < ALARM_DURATION + ALARM_PAUSE) {
#if BUZZER_IS_ACTIVE
    digitalWrite(BUZZER_PIN, LOW);
#else
    ledcWrite(BUZZER_CHANNEL, 0);
#endif
    buzzerActive = false;
  } else {
    alarmStartTime = currentTime;
    buzzerActive = true;
  }
}

void autoControl() {
  if (!autoMode) return;
  
  bool shouldAlarm = false;
  
  if (waterLevel > DANGER_WATER_LEVEL && rainLevel > DANGER_RAIN_LEVEL) {
    shouldAlarm = true;
  }
  
  if (shouldAlarm) {
    if (!buzzerActive && !isBeeping) {
      Serial.println("*** CẢNH BÁO: Mưa lớn Và nước cao!");
      alarmStartTime = millis();
      buzzerActive = true;
    }
    alarmPattern();
  } else {
    if (buzzerActive || isBeeping) {
      controlBuzzer(false);
    }
  }
}

void sendToThingsBoard() {
  if (!tbMqtt.connected()) {
    reconnectThingsBoard();
  }
  
  StaticJsonDocument<400> doc;
  doc["rain_level"] = rainLevel;
  doc["water_level"] = waterLevel;
  doc["water_cm"] = waterLevelCm;
  doc["water_alert"] = waterAlert;
  doc["rain_digital"] = rainDigital;
  doc["buzzer_active"] = buzzerActive;
  doc["auto_mode"] = autoMode;
  
  char jsonBuffer[400];
  serializeJson(doc, jsonBuffer);
  
  if (tbMqtt.publish("v1/devices/me/telemetry", jsonBuffer)) {
    Serial.println("*** Đã gửi dữ liệu lên ThingsBoard");
  } else {
    Serial.println("*** Lỗi gửi ThingsBoard");
  }
  
  StaticJsonDocument<100> attrDoc;
  attrDoc["autoMode"] = autoMode;
  attrDoc["buzzerState"] = buzzerActive;
  
  char attrBuffer[100];
  serializeJson(attrDoc, attrBuffer);
  tbMqtt.publish("v1/devices/me/attributes", attrBuffer);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== NODE 3 - Flood Warning System (WiFi) ===");
  
#if BUZZER_IS_ACTIVE
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#else
  // Khởi tạo PWM chỉ khi dùng passive buzzer
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQUENCY, BUZZER_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  ledcWrite(BUZZER_CHANNEL, 0);
#endif
  
  // Test còi
  Serial.println("Test còi (1 giây)...");
  controlBuzzer(true);
  delay(1000);
  controlBuzzer(false);
  
  // Kết nối WiFi
  setupWiFi();
  
  // Khởi động HTTP Server
  server.on("/", handleRoot);
  server.on("/data", HTTP_POST, handleData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("*** HTTP Server đã khởi động");
  
  // Cấu hình ThingsBoard
  tbMqtt.setServer(TB_SERVER, TB_PORT);
  tbMqtt.setCallback(tbCallback);
  tbMqtt.setBufferSize(512);
  
  Serial.println("\n=== HỆ THỐNG SẴN SÀNG ===");
  Serial.print("Web Interface: http://");
  Serial.println(WiFi.localIP());
  Serial.println("Chế độ: TỰ ĐỘNG (mặc định)\n");
}

void loop() {
  // Kiểm tra WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi mất kết nối, đang kết nối lại...");
    setupWiFi();
  }
  
  // Xử lý HTTP requests
  server.handleClient();
  
  // Duy trì ThingsBoard
  if (!tbMqtt.connected()) {
    reconnectThingsBoard();
  }
  tbMqtt.loop();
  
  // Xử lý logic tự động
  autoControl();
  
  // Gửi dữ liệu định kỳ
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;
    sendToThingsBoard();
    
    Serial.println("=== TRẠNG THÁI HỆ THỐNG ===");
    Serial.print("Mưa: ");
    Serial.print(rainLevel);
    Serial.println("%");
    Serial.print("Nước: ");
    Serial.print(waterLevel);
    Serial.print("% (");
    Serial.print(waterLevelCm);
    Serial.println(" cm)");
    Serial.print("Còi: ");
    Serial.println(buzzerActive ? "ĐANG KÊU" : "TẮT");
    Serial.print("Chế độ: ");
    Serial.println(autoMode ? "TỰ ĐỘNG" : "THỦ CÔNG");
    Serial.println();
  }
  
  delay(10);
}
