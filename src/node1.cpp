/*
 * NODE 1 - ESP32 + Rain Drop Sensor (WiFi)
 * Đọc cảm biến mưa và gửi đến Node 3
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ===== CẤU HÌNH WIFI =====
#define WIFI_SSID "Đăng"          
#define WIFI_PASSWORD "11111111"  

// ===== CẤU HÌNH NODE 3 =====
#define NODE3_IP "172.20.10.2"  

// ===== CẤU HÌNH CHÂN =====
#define RAIN_ANALOG_PIN 34
#define RAIN_DIGITAL_PIN 35

unsigned long lastSendTime = 0;
const long sendInterval = 2000; 

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
    Serial.print("IP Address Node 1: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n*** Không thể kết nối WiFi!");
  }
}

void sendDataToNode3(float rainPercent, int rainDigital) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("*** WiFi chưa kết nối!");
    return;
  }
  
  HTTPClient http;
  String url = "http://" + String(NODE3_IP) + "/data";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000); 
  
  // Tạo JSON
  StaticJsonDocument<200> doc;
  doc["nodeId"] = 1;
  doc["type"] = "RAIN";
  doc["value"] = rainPercent;
  doc["digital"] = rainDigital;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Gửi POST request
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("*** Gửi thành công");
    } else {
      Serial.print("*** HTTP code: ");
      Serial.println(httpCode);
    }
  } else {
    Serial.print("*** Lỗi: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== NODE 1 - Rain Sensor (WiFi) ===");
  
  // Cấu hình chân
  pinMode(RAIN_ANALOG_PIN, INPUT);
  pinMode(RAIN_DIGITAL_PIN, INPUT);
  
  // Kết nối WiFi
  setupWiFi();
  
  Serial.println("\n** Đảm bảo Node 3 đã chạy và cập nhật IP! **");
  Serial.print("Gửi dữ liệu đến: ");
  Serial.println(NODE3_IP);
  Serial.println("Node 1 sẵn sàng!\n");
}

void loop() {
  // Kiểm tra WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi mất kết nối, đang kết nối lại...");
    setupWiFi();
  }
  
  unsigned long currentTime = millis();
  
  if (currentTime - lastSendTime >= sendInterval) {
    lastSendTime = currentTime;
    
    // Đọc cảm biến
    int rainAnalog = analogRead(RAIN_ANALOG_PIN);
    int rainDigital = digitalRead(RAIN_DIGITAL_PIN);
    
    // Chuyển đổi sang phần trăm
    float rainPercent = map(rainAnalog, 4095, 0, 0, 100);
    rainPercent = constrain(rainPercent, 0, 100);
    
    // In thông tin
    Serial.println("=== Node 1 - Rain Sensor ===");
    Serial.print("Analog: ");
    Serial.print(rainAnalog);
    Serial.print(" | Digital: ");
    Serial.println(rainDigital);
    Serial.print("Mức độ mưa: ");
    Serial.print(rainPercent);
    Serial.println("%");
    
    // Gửi qua HTTP
    sendDataToNode3(rainPercent, rainDigital);
    Serial.println();
  }
  
  delay(100);
}