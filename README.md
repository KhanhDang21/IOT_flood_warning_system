# 🌊 IOT Flood Warning System

Hệ thống cảnh báo lũ lụt thông minh sử dụng mạng lưới các thiết bị ESP32 giao tiếp với nhau để thu thập dữ liệu thời tiết, đo mực nước, và kích hoạt cảnh báo âm thanh khi phát hiện nguy cơ lũ lụt. Dữ liệu được đẩy lên nền tảng **ThingsBoard** để giám sát từ xa theo thời gian thực.

---

## 📐 Kiến trúc hệ thống

```
┌─────────────────┐     ESP-NOW / WiFi     ┌─────────────────────────────┐
│   NODE 1        │ ─────────────────────► │   NODE 3 (Gateway)          │
│  ESP32          │                         │   ESP32                     │
│  Rain Sensor    │                         │   Buzzer                    │
│  (COM5)         │                         │   MQTT → ThingsBoard        │
└─────────────────┘                         │   (COM3)                    │
                                            └─────────────────────────────┘
┌─────────────────┐                                    ▲
│   NODE 2        │ ─────────────────────────────────► │
│  ESP32          │
│  HC-SR04        │
│  (Ultrasonic)   │
│  (COM7)         │
└─────────────────┘
```

Hệ thống gồm **3 node ESP32** hoạt động độc lập và phối hợp với nhau:

| Node | Vai trò | Cảm biến | Cổng COM |
|------|---------|----------|----------|
| Node 1 | Đo lượng mưa | Rain Sensor | COM5 |
| Node 2 | Đo mực nước | HC-SR04 (Ultrasonic) | COM7 |
| Node 3 | Gateway / Cảnh báo | Buzzer + MQTT | COM3 |

---

## ✨ Tính năng

- **Thu thập dữ liệu thời gian thực** từ cảm biến mưa và cảm biến siêu âm đo mực nước
- **Giao tiếp không dây** giữa các node (ESP-NOW hoặc WiFi)
- **Cảnh báo âm thanh** qua buzzer khi mực nước vượt ngưỡng nguy hiểm
- **Giám sát từ xa** qua nền tảng ThingsBoard (MQTT)
- **Dữ liệu dạng JSON** được serialize/deserialize bằng ArduinoJson

---

## 🛠️ Công nghệ sử dụng

| Thành phần | Chi tiết |
|-----------|----------|
| Vi điều khiển | ESP32 Dev Module |
| Framework | Arduino |
| Build system | PlatformIO |
| Ngôn ngữ | C++ |
| Thư viện | ArduinoJson v6, PubSubClient (MQTT) |
| IoT Platform | ThingsBoard |
| Giao thức | MQTT, ESP-NOW / WiFi |

---

## 📦 Yêu cầu cài đặt

### Phần mềm

- [VS Code](https://code.visualstudio.com/) + [PlatformIO IDE Extension](https://platformio.org/install/ide?install=vscode)
- Hoặc [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation/index.html)

### Phần cứng

- 3x ESP32 Dev Module
- 1x Cảm biến mưa (Rain Sensor)
- 1x Cảm biến siêu âm HC-SR04
- 1x Buzzer
- Dây jumper, breadboard
- Cáp USB (kết nối lập trình)

---

## 🚀 Hướng dẫn cài đặt & chạy

### 1. Clone repository

```bash
git clone https://github.com/KhanhDang21/IOT_flood_warning_system.git
cd IOT_flood_warning_system
```

### 2. Cấu hình WiFi & ThingsBoard

Mở file cấu hình tương ứng trong `src/` và cập nhật các thông tin sau:

```cpp
// WiFi
const char* WIFI_SSID = "YourSSID";
const char* WIFI_PASSWORD = "YourPassword";

// ThingsBoard (Node 3)
const char* TB_SERVER = "thingsboard.cloud";
const char* TB_TOKEN = "YourDeviceToken";
```

### 3. Build & Upload từng node

**Node 1 (Rain Sensor – COM5):**
```bash
pio run -e node1 --target upload
```

**Node 2 (Ultrasonic – COM7):**
```bash
pio run -e node2 --target upload
```

**Node 3 (Gateway – COM3):**
```bash
pio run -e node3 --target upload
```

### 4. Theo dõi Serial Monitor

```bash
# Ví dụ xem log Node 3
pio device monitor -e node3
```

---

## 📁 Cấu trúc thư mục

```
IOT_flood_warning_system/
├── src/
│   ├── node1.cpp          # Logic đọc cảm biến mưa
│   ├── node2.cpp          # Logic đo mực nước HC-SR04
│   └── node3.cpp          # Gateway: nhận dữ liệu, buzzer, MQTT
├── include/               # Header files dùng chung
├── lib/                   # Thư viện nội bộ
├── test/                  # Unit tests
├── platformio.ini         # Cấu hình build cho cả 3 node
└── README.md
```

---

## ⚙️ Cấu hình `platformio.ini`

```ini
[platformio]
default_envs = node1, node2, node3

[env:node1]
platform = espressif32
board = esp32dev
framework = arduino
upload_port = COM5
lib_deps = bblanchon/ArduinoJson@^6.21.3
build_flags = -D NODE_ID=1 -D NODE_TYPE='"RAIN_SENSOR"'
build_src_filter = +<node1.cpp>

[env:node2]
platform = espressif32
board = esp32dev
framework = arduino
upload_port = COM7
lib_deps = bblanchon/ArduinoJson@^6.21.3
build_flags = -D NODE_ID=2 -D NODE_TYPE='"ULTRASONIC"'
build_src_filter = +<node2.cpp>

[env:node3]
platform = espressif32
board = esp32dev
framework = arduino
upload_port = COM3
lib_deps =
  bblanchon/ArduinoJson@^6.21.3
  knolleary/PubSubClient@^2.8
build_flags = -D NODE_ID=3 -D NODE_TYPE='"GATEWAY"'
build_src_filter = +<node3.cpp>
```

> ⚠️ **Lưu ý:** Thay `COM5`, `COM7`, `COM3` bằng cổng COM thực tế trên máy của bạn (Linux: `/dev/ttyUSB0`, macOS: `/dev/cu.usbserial-*`).

---

## 📡 ThingsBoard Dashboard

Sau khi Node 3 kết nối thành công, bạn có thể tạo dashboard trên [ThingsBoard](https://thingsboard.io/) để hiển thị:

- 📊 Biểu đồ mực nước theo thời gian
- 🌧️ Lượng mưa theo thời gian
- 🚨 Trạng thái cảnh báo

---

## 🤝 Đóng góp

Mọi đóng góp đều được chào đón! Vui lòng tạo **Issue** hoặc **Pull Request** nếu bạn muốn cải thiện hệ thống.

---

## 📄 Giấy phép

Dự án này được phát hành theo giấy phép [MIT](LICENSE).

---

<div align="center">
  Made with ❤️ by <a href="https://github.com/KhanhDang21">KhanhDang21</a>
</div>
