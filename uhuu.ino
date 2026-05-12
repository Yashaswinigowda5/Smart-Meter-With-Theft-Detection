#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* ================= PIN DEFINITIONS ================= */
#define PULSE_PIN   2
#define RELAY_PIN   8
#define BUZZER_PIN  9
#define THEFT_PIN   10

/* ================= DEMO SCALING ================= */
/*
  Real meter: 6400 imp/kWh
  Demo mode: 12 pulses = 1 unit (FAST consumption)
*/
#define PULSES_PER_UNIT 12

/* ================= VARIABLES ================= */
volatile unsigned long pulseCount = 0;

float balanceUnits = 1.0;       // Initial credited units
float usedUnitOffset = 0.0;     // Prevents recharge reset bug

LiquidCrystal_I2C lcd(0x27, 16, 2);

/* Buzzer timing for low balance */
unsigned long lastBeepTime = 0;
bool buzzerState = false;

/* ================= INTERRUPT ================= */
void pulseISR() {
  pulseCount++;
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(9600);

  pinMode(PULSE_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(THEFT_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseISR, FALLING);

  lcd.init();
  lcd.backlight();

  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.setCursor(0, 0);
  lcd.print("PREPAID ENERGY");
  lcd.setCursor(0, 1);
  lcd.print("METER SYSTEM");

  delay(2000);
  lcd.clear();

  Serial.println("=== PREPAID ENERGY METER ===");
  Serial.println("Recharge format: R <units>");
  Serial.println("Example: R 0.5");
}

/* ================= LOOP ================= */
void loop() {

  /* -------- THEFT DETECTION (HIGHEST PRIORITY) -------- */
  if (digitalRead(THEFT_PIN) == HIGH) {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);   // Continuous alarm

    lcd.setCursor(0, 0);
    lcd.print("THEFT DETECTED");
    lcd.setCursor(0, 1);
    lcd.print("SUPPLY OFF    ");
    return;
  }

  /* -------- ENERGY CALCULATION -------- */
  float totalUnitsUsed = (float)pulseCount / PULSES_PER_UNIT;
  float unitsUsed = totalUnitsUsed - usedUnitOffset;
  float unitsLeft = balanceUnits - unitsUsed;

  /* -------- UNITS EXHAUSTED -------- */
  if (unitsLeft <= 0) {
    unitsLeft = 0;
    digitalWrite(RELAY_PIN, LOW);

    // Intermittent beep (non-irritating)
    if (millis() - lastBeepTime >= 1000) {
      lastBeepTime = millis();
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState);
    }

    lcd.setCursor(0, 0);
    lcd.print("BALANCE ZERO  ");
    lcd.setCursor(0, 1);
    lcd.print("SUPPLY OFF    ");
  }
  else {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;

    lcd.setCursor(0, 0);
    lcd.print("UNITS LEFT:   ");
    lcd.setCursor(11, 0);
    lcd.print(unitsLeft, 2);

    lcd.setCursor(0, 1);
    lcd.print("PULSES: ");
    lcd.print(pulseCount);
    lcd.print("   ");
  }

  /* -------- SERIAL RECHARGE -------- */
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd == 'R' || cmd == 'r') {
      float recharge = Serial.parseFloat();

      if (recharge > 0) {
        balanceUnits += recharge;
        usedUnitOffset = (float)pulseCount / PULSES_PER_UNIT;

        digitalWrite(BUZZER_PIN, LOW);
        buzzerState = false;

        Serial.print("Recharged: ");
        Serial.print(recharge);
        Serial.println(" units");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("RECHARGED");
        lcd.setCursor(0, 1);
        lcd.print(recharge);
        lcd.print(" UNITS");

        delay(2000);
        lcd.clear();
      }
    }
  }

  delay(200);
}
