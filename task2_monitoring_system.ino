/*
 * SEDS BPHC — Avionics Induction 2026-27
 * Athena's Intern | Task 2: Keeping Watch Over Odysseus
 * Author: Yashvardhan Singh
 * ID NO. = 2025A7PS0112H
 * Hardware: Arduino Uno, HC-SR04, LDR, LCD (16x2), Push Button, LED, Piezo Buzzer
 */

#include <LiquidCrystal.h>

// --- PIN DEFINITIONS ---
const int PIN_BUTTON    = 2;    // Pushbutton for Anchor (Pin 2)
const int PIN_TRIG      = 7;    // Ultrasonic TRIG (Pin 7)
const int PIN_ECHO      = 6;    // Ultrasonic ECHO (Pin 6)
const int PIN_LDR       = A0;   // Photoresistor Analog Pin (A0)
const int PIN_LED       = 13;   // Storm Warning LED (Pin 13)
const int PIN_BUZZER    = 8;    // Charybdis Warning Buzzer (Pin 8)

// LiquidCrystal(rs, enable, d4, d5, d6, d7)
LiquidCrystal lcd(12, 11, 5, 4, 3, 9);

// --- SYSTEM STATES ---
enum ShipState {
  OPEN_SEA,
  ANCHOR_DROPPED,
  STORM,
  CHARYBDIS,
  WRECKED
};

ShipState currentState = OPEN_SEA;

// --- SENSOR THRESHOLDS & TIMING ---
const int LDR_HALF_THRESHOLD = 512;           // Below half brightness triggers STORM
const float DISTANCE_THRESHOLD_CM = 100.0;    // Distance under 100cm triggers CHARYBDIS
const unsigned long DANGER_TIMEOUT_MS = 5000; // 5s continuous danger -> WRECKED

unsigned long hazardStartTime = 0;
unsigned long lastLedBlinkTime = 0;
bool ledState = false;

// Debounce handling for pushbutton
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

float readUltrasonicDistanceCM() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0) return 999.0;
  return (duration * 0.0343) / 2.0;
}

void setup() {
  Serial.begin(9600);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ODYSSEUS WATCH");
  lcd.setCursor(0, 1);
  lcd.print("ATHENA'S SYSTEM");
  delay(1200);
  lcd.clear();
}

void loop() {
  if (currentState == WRECKED) {
    digitalWrite(PIN_LED, HIGH);
    tone(PIN_BUZZER, 400);
    lcd.setCursor(0, 0);
    lcd.print("STATE: WRECKED  ");
    lcd.setCursor(0, 1);
    lcd.print("SHIP IS LOST!   ");
    return;
  }

  // Pushbutton Toggle logic
  int reading = digitalRead(PIN_BUTTON);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    static int buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        if (currentState == ANCHOR_DROPPED) {
          currentState = OPEN_SEA;
        } else {
          currentState = ANCHOR_DROPPED;
          hazardStartTime = 0;
        }
      }
    }
  }
  lastButtonState = reading;

  int lightVal = analogRead(PIN_LDR);
  float distance = readUltrasonicDistanceCM();

  bool stormTriggered = (lightVal < LDR_HALF_THRESHOLD);
  bool charybdisTriggered = (distance < DISTANCE_THRESHOLD_CM);

  // FSM Logic
  if (currentState != ANCHOR_DROPPED) {
    switch (currentState) {
      case OPEN_SEA:
        noTone(PIN_BUZZER);
        digitalWrite(PIN_LED, LOW);

        if (stormTriggered) {
          currentState = STORM;
          hazardStartTime = millis();
        } else if (charybdisTriggered) {
          currentState = CHARYBDIS;
          hazardStartTime = millis();
        }
        break;

      case STORM:
        if (millis() - lastLedBlinkTime >= 250) {
          lastLedBlinkTime = millis();
          ledState = !ledState;
          digitalWrite(PIN_LED, ledState ? HIGH : LOW);
        }
        noTone(PIN_BUZZER);

        if (millis() - hazardStartTime >= DANGER_TIMEOUT_MS) {
          currentState = WRECKED;
        } else if (!stormTriggered) {
          digitalWrite(PIN_LED, LOW);
          currentState = OPEN_SEA;
          hazardStartTime = 0;
        }
        break;

      case CHARYBDIS:
        digitalWrite(PIN_LED, LOW);
        tone(PIN_BUZZER, 1000);

        if (millis() - hazardStartTime >= DANGER_TIMEOUT_MS) {
          currentState = WRECKED;
        } else if (!charybdisTriggered) {
          noTone(PIN_BUZZER);
          currentState = OPEN_SEA;
          hazardStartTime = 0;
        }
        break;

      default:
        break;
    }
  } else {
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
  }

  renderLCD(distance);
  delay(30);
}

void renderLCD(float distance) {
  static ShipState lastState = (ShipState)-1;
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate < 150 && currentState == lastState) return;
  lastUpdate = millis();
  lastState = currentState;

  lcd.clear();
  switch (currentState) {
    case OPEN_SEA:
      lcd.setCursor(0, 0);
      lcd.print("STATE: OPEN SEA ");
      lcd.setCursor(0, 1);
      lcd.print("Sailing Smoothly");
      break;

    case ANCHOR_DROPPED:
      lcd.setCursor(0, 0);
      lcd.print("ANCHOR DROPPED  ");
      lcd.setCursor(0, 1);
      lcd.print("Protected & Safe");
      break;

    case STORM: {
      int remaining = 5 - (int)((millis() - hazardStartTime) / 1000);
      if (remaining < 0) remaining = 0;
      lcd.setCursor(0, 0);
      lcd.print("STATE: STORM!   ");
      lcd.setCursor(0, 1);
      lcd.print("Wreck in: ");
      lcd.print(remaining);
      lcd.print("s   ");
      break;
    }

    case CHARYBDIS: {
      int remaining = 5 - (int)((millis() - hazardStartTime) / 1000);
      if (remaining < 0) remaining = 0;
      lcd.setCursor(0, 0);
      lcd.print("CHARYBDIS DANGER");
      lcd.setCursor(0, 1);
      lcd.print("Dist:");
      lcd.print((int)distance);
      lcd.print("cm T-");
      lcd.print(remaining);
      lcd.print("s");
      break;
    }

    case WRECKED:
      lcd.setCursor(0, 0);
      lcd.print("STATE: WRECKED  ");
      lcd.setCursor(0, 1);
      lcd.print("Voyage Terminated");
      break;
  }
}
