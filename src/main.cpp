#include <Arduino.h>
#include <BleGamepad.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

BleGamepad bleGamepad("Guitar Controller", "ESP32", 100);

// =====================================================
// ลำดับขา:
// เขียว แดง เหลือง น้ำเงิน ส้ม ดีดขึ้น ดีดลง
// =====================================================
const uint8_t buttonPins[7] = {
  23, 25, 27, 26, 33, 17, 16
};

const uint8_t gamepadButtons[7] = {
  BUTTON_1,
  BUTTON_2,
  BUTTON_3,
  BUTTON_4,
  BUTTON_5,
  BUTTON_6,
  BUTTON_7
};

const char* buttonNames[7] = {
  "เขียว",
  "แดง",
  "เหลือง",
  "น้ำเงิน",
  "ส้ม",
  "ดีดขึ้น",
  "ดีดลง"
};

const uint8_t comPin = 32;

const int SLIDE_POT_PIN = 34;

// Poten ทำงานแบบปุ่ม: ปล่อย ~2900, กดสุด ~3200
// ค่าสูงกว่าตำแหน่งปล่อยเกิน PRESS = กด, กลับมาต่ำกว่า RELEASE = ปล่อย
// ตำแหน่งปล่อยเลื่อนตามไฟเลี้ยง จึงปรับตามค่าที่อ่านได้ตลอดแทนค่าตายตัว
const uint8_t POT_BUTTON = BUTTON_8;
const int POT_PRESS_DELTA = 150;
const int POT_RELEASE_DELTA = 90;
const unsigned long POT_MAX_HOLD_MS = 8000;   // กดค้างนานกว่านี้ = ตั้งตำแหน่งปล่อยใหม่

const unsigned long DEBOUNCE_MS = 25;
const unsigned long REPORT_INTERVAL_MS = 20;

struct ButtonState {
  int lastReading;
  int stableState;
  unsigned long lastChangeTime;
};

ButtonState buttons[7];

bool lastPressed[7] = {
  false, false, false, false,
  false, false, false
};

bool wasConnected = false;

unsigned long lastReportTime = 0;
unsigned long lastPotPrintTime = 0;
unsigned long potTestStartTime = 0;
int potTestMin = 4095;
int potTestMax = 0;
bool potTestReported = false;

float potRestValue = 0;
bool potPressed = false;
unsigned long potPressTime = 0;


// =====================================================
// อ่านปุ่มพร้อม Debounce
// =====================================================
bool buttonIsPressed(int index) {

  int reading = digitalRead(buttonPins[index]);

  if (reading != buttons[index].lastReading) {
    buttons[index].lastReading = reading;
    buttons[index].lastChangeTime = millis();
  }

  if (millis() - buttons[index].lastChangeTime >= DEBOUNCE_MS) {
    buttons[index].stableState = buttons[index].lastReading;
  }

  // INPUT_PULLUP:
  // LOW = กด
  // HIGH = ปล่อย
  return buttons[index].stableState == LOW;
}


// =====================================================
// อ่าน Slide Potentiometer
// =====================================================
int readSlidePot() {

  int total = 0;

  for (int i = 0; i < 5; i++) {
    total += analogRead(SLIDE_POT_PIN);
    delayMicroseconds(300);
  }

  return total / 5;
}


// =====================================================
// ตรวจว่า Poten ถูกกดหรือไม่ พร้อมปรับตำแหน่งปล่อย
// ตามลงเร็ว ตามขึ้นช้าเฉพาะตอนไม่ได้กด
// =====================================================
bool potIsPressed(int potValue) {

  if (potValue < potRestValue) {
    potRestValue += (potValue - potRestValue) * 0.05f;
  } else if (!potPressed) {
    potRestValue += (potValue - potRestValue) * 0.002f;
  }

  if (potPressed && millis() - potPressTime >= POT_MAX_HOLD_MS) {
    potRestValue = potValue;
  }

  int potDelta = potValue - (int)potRestValue;

  if (potPressed) {
    return potDelta > POT_RELEASE_DELTA;
  }

  return potDelta > POT_PRESS_DELTA;
}


// =====================================================
// Setup
// =====================================================
void setup() {

  // ปิด Brownout Detector: ไฟตกชั่วขณะตอนเริ่ม Bluetooth ทำให้บอร์ดรีบูตวน
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("     ESP32 Guitar Controller");
  Serial.println("==============================");

  pinMode(comPin, OUTPUT);
  digitalWrite(comPin, LOW);

  // ตั้งค่าปุ่ม
  for (int i = 0; i < 7; i++) {

    pinMode(buttonPins[i], INPUT_PULLUP);

    buttons[i].lastReading = digitalRead(buttonPins[i]);
    buttons[i].stableState = buttons[i].lastReading;
    buttons[i].lastChangeTime = millis();

    lastPressed[i] = false;
  }


  // ADC 12-bit
  analogReadResolution(12);

  potRestValue = readSlidePot();

  potTestStartTime = millis();
  Serial.println("ทดสอบ Poten: ต่อวงจรให้ครบ แล้วเลื่อนจากสุดด้านหนึ่งไปอีกด้านภายใน 5 วินาที");


  // เริ่ม Bluetooth Gamepad
  bleGamepad.begin();

  Serial.println("Bluetooth Gamepad Starting...");
  Serial.println("Waiting for connection...");
}


// =====================================================
// Loop
// =====================================================
void loop() {

  int potValue = readSlidePot();

  if (!potTestReported) {
    potTestMin = min(potTestMin, potValue);
    potTestMax = max(potTestMax, potValue);

    if (millis() - potTestStartTime >= 5000) {
      Serial.print("ช่วงค่าที่อ่านได้: ");
      Serial.print(potTestMin);
      Serial.print(" - ");
      Serial.println(potTestMax);

      if (potTestMax - potTestMin >= 500) {
        Serial.println("ผลทดสอบ Poten: เชื่อมต่อและตอบสนอง (ค่ามีการเปลี่ยนแปลง)");
      } else {
        Serial.println("ผลทดสอบ Poten: ไม่พบการเชื่อมต่อ/การเปลี่ยนแปลง ตรวจสายและเลื่อน Poten ให้สุดช่วง");
      }

      potTestReported = true;
    }
  }

  if (millis() - lastPotPrintTime >= 200) {
    Serial.print("Poten: ");
    Serial.print(potValue);
    Serial.print("  ปล่อย: ");
    Serial.println((int)potRestValue);
    lastPotPrintTime = millis();
  }

  // ===================================================
  // Bluetooth Connected
  // ===================================================
  if (bleGamepad.isConnected()) {

    if (!wasConnected) {

      Serial.println();
      Serial.println(">>> Bluetooth Connected <<<");

      wasConnected = true;
    }


    // =================================================
    // อ่านปุ่ม
    // =================================================
    for (int i = 0; i < 7; i++) {

      bool pressed = buttonIsPressed(i);


      // -----------------------------------------------
      // ส่งปุ่มไป Bluetooth Gamepad
      // -----------------------------------------------
      if (pressed) {
        bleGamepad.press(gamepadButtons[i]);
      } else {
        bleGamepad.release(gamepadButtons[i]);
      }


      // -----------------------------------------------
      // แจ้งเตือนเมื่อสถานะเปลี่ยน
      // -----------------------------------------------
      if (pressed != lastPressed[i]) {

        if (pressed) {

          // ===========================================
          // กดปุ่ม
          // ===========================================
          Serial.print(">>> SWITCH PRESSED: ");
          Serial.print(buttonNames[i]);

          Serial.print(" [GPIO ");
          Serial.print(buttonPins[i]);
          Serial.println("]");


        } else {

          // ===========================================
          // ปล่อยปุ่ม
          // ===========================================
          Serial.print("<<< SWITCH RELEASED: ");
          Serial.print(buttonNames[i]);

          Serial.print(" [GPIO ");
          Serial.print(buttonPins[i]);
          Serial.println("]");
        }


        // จำสถานะล่าสุด
        lastPressed[i] = pressed;
      }
    }


    // =================================================
    // Slide Potentiometer
    // =================================================
    bool potNow = potIsPressed(potValue);

    if (potNow) {
      bleGamepad.press(POT_BUTTON);
    } else {
      bleGamepad.release(POT_BUTTON);
    }

    if (potNow != potPressed) {

      if (potNow) {
        Serial.print(">>> SWITCH PRESSED: Poten [GPIO ");
      } else {
        Serial.print("<<< SWITCH RELEASED: Poten [GPIO ");
      }

      Serial.print(SLIDE_POT_PIN);
      Serial.println("]");

      potPressed = potNow;
      potPressTime = millis();
    }


    // =================================================
    // ส่ง Report
    // =================================================
    if (millis() - lastReportTime >= REPORT_INTERVAL_MS) {

      bleGamepad.sendReport();

      lastReportTime = millis();
    }


  } else {

    // =================================================
    // Bluetooth Disconnected
    // =================================================
    if (wasConnected) {

      Serial.println();
      Serial.println(">>> Bluetooth Disconnected <<<");
      Serial.println("Waiting for connection...");

      wasConnected = false;
    }
  }


  delay(5);
}
