/*
 * NODE 2 - ESP32 + HC-SR04 Ultrasonic Sensor (WiFi)
 * Đo mức nước trong ống cống 20cm và gửi đến Node 3
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ===== CẤU HÌNH WIFI =====
#define WIFI_SSID "Đăng"          
#define WIFI_PASSWORD "11111111"  

// ===== CẤU HÌNH NODE 3 =====
#define NODE3_IP "172.20.10.2"  

// ===== CẤU HÌNH CHÂN HC-SR04 =====
#define TRIG_PIN 5
#define ECHO_PIN 18

// ===== CẤU HÌNH MỨC NƯỚC - ỐNG CỐNG 20CM =====
#define PIPE_DEPTH 20.0
#define SENSOR_TO_BOTTOM 20.0
#define MAX_DISTANCE 30

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
    Serial.print("IP Address Node 2: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n*** Không thể kết nối WiFi!");
  }
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  if (duration == 0) {
    return 0;
  }
  
  float distance = duration * 0.034 / 2;
  return distance;
}

float getWaterLevel() {
  float distance = measureDistance();
  
  if (distance < 0 || distance > MAX_DISTANCE) {
    return 0; // Lỗi đo
  }
  
  float waterLevel = SENSOR_TO_BOTTOM - distance;
  
  if (waterLevel < 0) waterLevel = 0;
  if (waterLevel > PIPE_DEPTH) waterLevel = PIPE_DEPTH;
  
  return waterLevel;
}

void sendDataToNode3(float waterPercent, int alertLevel, float waterLevelCm) {
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
  doc["nodeId"] = 2;
  doc["type"] = "WATER";
  doc["value"] = waterPercent;
  doc["alert"] = alertLevel;
  doc["waterCm"] = waterLevelCm;
  
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
    Serial.print("✗ Lỗi: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== NODE 2 - HC-SR04 (WiFi) ===");
  
  // Cấu hình chân
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Kết nối WiFi
  setupWiFi();
  
  Serial.println("\n** Đảm bảo Node 3 đã chạy và cập nhật IP! **");
  Serial.print("Gửi dữ liệu đến: ");
  Serial.println(NODE3_IP);
  Serial.println("Node 2 sẵn sàng!\n");
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
    
    // Đo mức nước
    float waterLevelCm = getWaterLevel();
    
    if (waterLevelCm < 0) {
      Serial.println("*** Lỗi đo mức nước!");
      return;
    }
    
    // Tính phần trăm
    float waterPercent = (waterLevelCm / PIPE_DEPTH) * 100.0;
    waterPercent = constrain(waterPercent, 0, 100);
    
    // Xác định mức cảnh báo
    int alertLevel = 0;
    if (waterPercent > 70) {
      alertLevel = 2; // Nguy hiểm
    } else if (waterPercent > 50) {
      alertLevel = 1; // Cảnh báo
    }
    
    // In thông tin
    Serial.println("=== Node 2 - Water Level ===");
    Serial.print("Khoảng cách: ");
    Serial.print(SENSOR_TO_BOTTOM - waterLevelCm);
    Serial.println(" cm");
    Serial.print("Mức nước: ");
    Serial.print(waterLevelCm);
    Serial.print(" cm / ");
    Serial.print(PIPE_DEPTH);
    Serial.println(" cm");
    Serial.print("Phần trăm: ");
    Serial.print(waterPercent);
    Serial.println("%");
    Serial.print("Cảnh báo: ");
    if (alertLevel == 2) {
      Serial.println("NGUY HIỂM (>14cm)");
    } else if (alertLevel == 1) {
      Serial.println("CẢNH BÁO (>10cm)");
    } else {
      Serial.println("BÌNH THƯỜNG");
    }
    
    // Gửi qua HTTP
    sendDataToNode3(waterPercent, alertLevel, waterLevelCm);
    Serial.println();
  }
  
  delay(100);
}