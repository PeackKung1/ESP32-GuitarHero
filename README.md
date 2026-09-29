# จอยกีตาร์ไร้สายด้วย ESP32

โปรเจกต์จอยกีตาร์ไร้สายที่ใช้ ESP32 ส่งปุ่มควบคุมไปยังคอมพิวเตอร์ผ่าน Bluetooth LE Gamepad

## ความสามารถ

- ปุ่มเฟรตกีตาร์ 5 สี: เขียว แดง เหลือง น้ำเงิน และส้ม
- ปุ่มดีด 2 ปุ่ม: ดีดขึ้นและดีดลง
- Slide Potentiometer สำหรับควบคุมแกน X
- เชื่อมต่อคอมพิวเตอร์ผ่าน Bluetooth LE

## อุปกรณ์

- ESP32 Dev Module
- สวิตช์ปุ่มกีตาร์ 5 ปุ่ม
- ปุ่มดีด 2 ปุ่ม
- Slide Potentiometer
- สายไฟและแหล่งจ่ายไฟสำหรับ ESP32

## การต่อวงจร

| อุปกรณ์ | ขา ESP32 |
|---|---:|
| ปุ่มเขียว | GPIO 23 |
| ปุ่มแดง | GPIO 25 |
| ปุ่มเหลือง | GPIO 27 |
| ปุ่มน้ำเงิน | GPIO 26 |
| ปุ่มส้ม | GPIO 33 |
| ปุ่มดีดขึ้น | GPIO 17 |
| ปุ่มดีดลง | GPIO 16 |
| สายดำ Common | GPIO 32 |
| ขากลางของ Slide Potentiometer | GPIO 34 |

ต่อสวิตช์แต่ละปุ่มอีกขาหนึ่งเข้ากับ GND โค้ดใช้ `INPUT_PULLUP` อยู่แล้ว ส่วนขาด้านข้างของโพเทนชิออมิเตอร์ต่อกับ 3.3V และ GND ตามลำดับ

> ใช้แรงดัน 3.3V กับขา ESP32 ห้ามป้อน 5V เข้า GPIO

## การติดตั้งและอัปโหลด

โปรเจกต์นี้ใช้ VS Code และ PlatformIO

1. ติดตั้งส่วนขยาย PlatformIO IDE ใน VS Code
2. เปิดโปรเจกต์และตรวจสอบไฟล์ `platformio.ini`
3. กด **Build** เพื่อตรวจสอบโค้ด
4. เชื่อมต่อ ESP32 แล้วกด **Upload**

ตัวอย่างส่วนตั้งค่าใน `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

lib_deps =
    https://github.com/lemmingDev/ESP32-BLE-Gamepad.git
    h2zero/NimBLE-Arduino
```

## การเชื่อมต่อกับคอมพิวเตอร์

หลังอัปโหลดโค้ด ให้เปิด Bluetooth ในคอมพิวเตอร์และจับคู่กับอุปกรณ์ชื่อ **Guitar Controller** จากนั้นเลือกจอยในเกมและตั้งค่าปุ่มให้ตรงกับการควบคุมของเกม

## ไลบรารี

- [ESP32-BLE-Gamepad](https://github.com/lemmingDev/ESP32-BLE-Gamepad)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
