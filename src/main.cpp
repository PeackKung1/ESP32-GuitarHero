#include <Arduino.h>
#include <BleGamepad.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

BleGamepad bleGamepad("Guitar Controller", "ESP32", 100);

// =====================================================
// ปุ่ม (INPUT_PULLUP: LOW = กด, HIGH = ปล่อย)
// =====================================================
struct Button {
  uint8_t pin;
  uint8_t gamepadButton;
  const char* name;
  int lastReading;
  int stableState;
  unsigned long lastChangeTime;
  bool pressed;
};

Button buttons[] = {
  {23, BUTTON_1, "เขียว"},
  {25, BUTTON_2, "แดง"},
  {27, BUTTON_3, "เหลือง"},
  {26, BUTTON_4, "น้ำเงิน"},
  {33, BUTTON_5, "ส้ม"},
  {17, BUTTON_6, "ดีดขึ้น"},
  {16, BUTTON_7, "ดีดลง"}
};

const int BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0]);

const uint8_t COM_PIN = 32;

const unsigned long DEBOUNCE_MS = 25;
const unsigned long REPORT_INTERVAL_MS = 20;

// =====================================================
// Poten (คันโยก) ทำงานแบบปุ่ม: ปล่อย ~2900, กดสุด ~3200
// ค่าสูงกว่าตำแหน่งปล่อยเกิน PRESS = กด, กลับมาต่ำกว่า RELEASE = ปล่อย
// ตำแหน่งปล่อยเลื่อนตามไฟเลี้ยง จึงปรับตามค่าที่อ่านได้ตลอดแทนค่าตายตัว
// =====================================================
const uint8_t POT_PIN = 34;
const uint8_t POT_BUTTON = BUTTON_8;
const char* POT_NAME = "Poten";

const int POT_SAMPLES = 5;
const int POT_PRESS_DELTA = 150;
const int POT_RELEASE_DELTA = 90;
const unsigned long POT_MAX_HOLD_MS = 8000;   // กดค้างนานกว่านี้ = ตั้งตำแหน่งปล่อยใหม่
const unsigned long POT_PRINT_INTERVAL_MS = 200;

float potRestValue = 0;
bool potPressed = false;
unsigned long potPressTime = 0;
bool potLastReading = false;
unsigned long potLastChangeTime = 0;

bool wasConnected = false;

unsigned long lastReportTime = 0;
unsigned long lastPotPrintTime = 0;


// =====================================================
// อ่านปุ่มพร้อม Debounce
// =====================================================
bool buttonIsPressed(Button& button) {

  int reading = digitalRead(button.pin);

  if (reading != button.lastReading) {
    button.lastReading = reading;
    button.lastChangeTime = millis();
  }

  if (millis() - button.lastChangeTime >= DEBOUNCE_MS) {
    button.stableState = button.lastReading;
  }

  return button.stableState == LOW;
}


// =====================================================
// อ่าน Poten (เฉลี่ยหลายค่า)
// =====================================================
int readPot() {

  int total = 0;

  for (int i = 0; i < POT_SAMPLES; i++) {
    total += analogRead(POT_PIN);
    delayMicroseconds(300);
  }

  return total / POT_SAMPLES;
}


// =====================================================
// ตรวจว่า Poten ถูกกดหรือไม่ พร้อมปรับตำแหน่งปล่อยและ Debounce
// ตำแหน่งปล่อย: ตามลงเร็ว ตามขึ้นช้าเฉพาะตอนไม่ได้กด
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

  bool reading = potDelta > (potPressed ? POT_RELEASE_DELTA : POT_PRESS_DELTA);

  if (reading != potLastReading) {
    potLastReading = reading;
    potLastChangeTime = millis();
  }

  // ค่ากระตุกจากไฟเลี้ยงยาวแค่ ~10 ms จึงต้องนิ่งครบ DEBOUNCE_MS ก่อนเปลี่ยนสถานะ
  if (millis() - potLastChangeTime >= DEBOUNCE_MS) {
    return reading;
  }

  return potPressed;
}


// =====================================================
// ส่งสถานะปุ่มไป Bluetooth Gamepad
// =====================================================
void sendButton(uint8_t gamepadButton, bool pressed) {

  if (pressed) {
    bleGamepad.press(gamepadButton);
  } else {
    bleGamepad.release(gamepadButton);
  }
}


// =====================================================
// แจ้งเตือนทาง Serial เมื่อสถานะปุ่มเปลี่ยน
// =====================================================
void printSwitchEvent(const char* name, uint8_t pin, bool pressed) {

  Serial.print(pressed ? ">>> SWITCH PRESSED: " : "<<< SWITCH RELEASED: ");
  Serial.print(name);
  Serial.print(" [GPIO ");
  Serial.print(pin);
  Serial.println("]");
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

  pinMode(COM_PIN, OUTPUT);
  digitalWrite(COM_PIN, LOW);

  for (Button& button : buttons) {

    pinMode(button.pin, INPUT_PULLUP);

    button.lastReading = digitalRead(button.pin);
    button.stableState = button.lastReading;
    button.lastChangeTime = millis();
    button.pressed = false;
  }

  // ADC 12-bit
  analogReadResolution(12);

  potRestValue = readPot();

  bleGamepad.begin();

  Serial.println("Bluetooth Gamepad Starting...");
  Serial.println("Waiting for connection...");
}


// =====================================================
// Loop
// =====================================================
void loop() {

  int potValue = readPot();

  if (millis() - lastPotPrintTime >= POT_PRINT_INTERVAL_MS) {

    Serial.print("Poten: ");
    Serial.print(potValue);
    Serial.print("  ปล่อย: ");
    Serial.println((int)potRestValue);

    lastPotPrintTime = millis();
  }

  if (bleGamepad.isConnected()) {

    if (!wasConnected) {

      Serial.println();
      Serial.println(">>> Bluetooth Connected <<<");

      wasConnected = true;
    }

    // ปุ่ม
    for (Button& button : buttons) {

      bool pressed = buttonIsPressed(button);

      sendButton(button.gamepadButton, pressed);

      if (pressed != button.pressed) {
        printSwitchEvent(button.name, button.pin, pressed);
        button.pressed = pressed;
      }
    }

    // Poten
    bool potNow = potIsPressed(potValue);

    sendButton(POT_BUTTON, potNow);

    if (potNow != potPressed) {
      printSwitchEvent(POT_NAME, POT_PIN, potNow);
      potPressed = potNow;
      potPressTime = millis();
    }

    // ส่ง Report
    if (millis() - lastReportTime >= REPORT_INTERVAL_MS) {
      bleGamepad.sendReport();
      lastReportTime = millis();
    }

  } else if (wasConnected) {

    Serial.println();
    Serial.println(">>> Bluetooth Disconnected <<<");
    Serial.println("Waiting for connection...");

    wasConnected = false;
  }

  delay(5);
}
