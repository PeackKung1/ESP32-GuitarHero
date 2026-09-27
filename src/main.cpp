#include <Arduino.h>
#include <BleGamepad.h>

BleGamepad bleGamepad("Guitar Controller", "ESP32", 100);

// ลำดับขา: เขียว แดง เหลือง น้ำเงิน ส้ม ดีดขึ้น ดีดลง
const uint8_t buttonPins[7] = {
  18, 19, 21, 22, 23, 25, 26
};

const uint8_t gamepadButtons[7] = {
  BUTTON_1, BUTTON_2, BUTTON_3, BUTTON_4,
  BUTTON_5, BUTTON_6, BUTTON_7
};

const int SLIDE_POT_PIN = 34;
const unsigned long DEBOUNCE_MS = 25;

struct ButtonState {
  int lastReading;
  int stableState;
  unsigned long lastChangeTime;
};

ButtonState buttons[7];

bool buttonIsPressed(int index) {
  int reading = digitalRead(buttonPins[index]);

  if (reading != buttons[index].lastReading) {
    buttons[index].lastReading = reading;
    buttons[index].lastChangeTime = millis();
  }

  if (millis() - buttons[index].lastChangeTime >= DEBOUNCE_MS) {
    buttons[index].stableState = buttons[index].lastReading;
  }

  // ต่อปุ่มระหว่าง GPIO กับ GND: LOW คือกด
  return buttons[index].stableState == LOW;
}

int readSlidePot() {
  int total = 0;

  for (int i = 0; i < 5; i++) {
    total += analogRead(SLIDE_POT_PIN);
    delayMicroseconds(300);
  }

  return total / 5;
}

void setup() {
  for (int i = 0; i < 7; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
    buttons[i].lastReading = digitalRead(buttonPins[i]);
    buttons[i].stableState = buttons[i].lastReading;
    buttons[i].lastChangeTime = millis();
  }

  analogReadResolution(12); // ESP32 อ่านค่า 0–4095
  bleGamepad.begin();
}

void loop() {
  if (bleGamepad.isConnected()) {
    for (int i = 0; i < 7; i++) {
      if (buttonIsPressed(i)) {
        bleGamepad.press(gamepadButtons[i]);
      } else {
        bleGamepad.release(gamepadButtons[i]);
      }
    }

    bleGamepad.sendReport();

    // Slide Potentiometer คุมแกน X
    int potValue = readSlidePot();
    int axisValue = map(potValue, 0, 4095, 32767, 0);
    bleGamepad.setX(axisValue);
  }

  delay(10);
}