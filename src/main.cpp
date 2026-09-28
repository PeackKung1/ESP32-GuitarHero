#include <Arduino.h>
#include <BleGamepad.h>

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
// Setup
// =====================================================
void setup() {

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


  // เริ่ม Bluetooth Gamepad
  bleGamepad.begin();

  Serial.println("Bluetooth Gamepad Starting...");
  Serial.println("Waiting for connection...");
}


// =====================================================
// Loop
// =====================================================
void loop() {

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
    int potValue = readSlidePot();

    int axisValue = map(
      potValue,
      0,
      4095,
      32767,
      0
    );

    bleGamepad.setX(axisValue);


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
