#define IR_USE_AVR_TIMER2
#include <IRremote.h>
#include <Stepper.h>
#include <Servo.h>
#include <LiquidCrystal.h>
#include <math.h>

#define STEPS 32

// --- Pins ---
#define IR_PIN 2
const int stepsPerRev = 2048; 
Stepper baseStepper(STEPS, 8, 10, 9, 11);
LiquidCrystal lcd(4, 7, A5, A4, A3, 12);

Servo forearmServo; 
Servo wristServo;   

// --- RGB Pins ---
#define BLUE 3
#define GREEN 5
#define RED 6

// --- Movement Settings ---
const float SERVO_INCREMENT = 3.5; 
const uint32_t TIMER_START_CODE = 0xBF40FF00;

// --- State Tracking ---
uint32_t lastValidCode = 0;
unsigned long lastSignalTime = 0;
const int RECOVERY_THRESHOLD = 200; 

float forearmPos = 120.0;
float wristPos = 180;
int stepperDir = 0; 
long currentStepperPos = 0;
bool timerRunning = false;

// ===== TIME ENTRY =====
bool enteringTime = false;
String timeBuffer = "";
int enteredSeconds = 0;

void stopStepperCoils() {
  digitalWrite(8, LOW); digitalWrite(9, LOW);
  digitalWrite(10, LOW); digitalWrite(11, LOW);
}

void setup() {
  Serial.begin(9600);
  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);

  lcd.begin(16, 2);
  showIdleMessage();

  forearmServo.attach(A1);
  wristServo.attach(A0);
  forearmServo.write((int)forearmPos);
  wristServo.write((int)wristPos);

  pinMode(8, OUTPUT); pinMode(9, OUTPUT);
  pinMode(10, OUTPUT); pinMode(11, OUTPUT);
  stopStepperCoils();

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);

  digitalWrite(RED, HIGH);
  digitalWrite(GREEN, HIGH);
  digitalWrite(BLUE, HIGH);
}

void loop() {

  // ===== IR DECODING =====
  if (IrReceiver.decode()) {

    uint32_t incomingCode = IrReceiver.decodedIRData.decodedRawData;
    bool isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT);

    // ===== START ENTERING TIME =====
    if (incomingCode == TIMER_START_CODE && !timerRunning && !enteringTime) {
      enteringTime = true;
      timeBuffer = "";
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Enter time:");
      lcd.setCursor(0,1);
      lcd.print("> ");
    }

    // ===== CONFIRM TIME =====
    else if (incomingCode == TIMER_START_CODE && enteringTime) {
      if (timeBuffer.length() > 0) {
        enteredSeconds = timeBuffer.toInt();
        enteringTime = false;
        runCountdown();
      }
    }

    // ===== HANDLE NUMBER INPUT =====
    else if (enteringTime && !isRepeat) {

      int digit = -1;

      switch (incomingCode) {
        case 0xE916FF00: digit = 0; break;
        case 0xF30CFF00: digit = 1; break;
        case 0xE718FF00: digit = 2; break;
        case 0xA15EFF00: digit = 3; break;
        case 0xF708FF00: digit = 4; break;
        case 0xE31CFF00: digit = 5; break;
        case 0xA55AFF00: digit = 6; break;
        case 0xBD42FF00: digit = 7; break;
        case 0xAD52FF00: digit = 8; break;
        case 0xB54AFF00: digit = 9; break;
      }

      if (digit != -1 && timeBuffer.length() < 5) {
        timeBuffer += String(digit);

        lcd.setCursor(0,1);
        lcd.print("> ");
        lcd.print(timeBuffer);
        lcd.print("     ");
      }
    }

    // ===== NORMAL MOVEMENT =====
    else if (!timerRunning && !enteringTime) {
      if (incomingCode != 0 && !isRepeat && isValidButton(incomingCode)) {
        lastValidCode = incomingCode;
        lastSignalTime = millis();
        updateMovement(lastValidCode);
      } 
      else if (isRepeat && lastValidCode != 0) {
        lastSignalTime = millis();
        updateMovement(lastValidCode);
      }
    }

    IrReceiver.resume();
  }

  // ===== WATCHDOG =====
  if (!timerRunning && (millis() - lastSignalTime > RECOVERY_THRESHOLD)) {
    if (stepperDir != 0) {
      stepperDir = 0;
      stopStepperCoils();
    }
    lastValidCode = 0; 
  }

  // ===== STEPPER EXECUTION =====
  if (!timerRunning && !enteringTime) {
    if (stepperDir == 1 && currentStepperPos < stepsPerRev) {
      baseStepper.setSpeed(900); 
      baseStepper.step(1);
      currentStepperPos++;
    } 
    else if (stepperDir == -1 && currentStepperPos > 0) {
      baseStepper.setSpeed(900);
      baseStepper.step(-1);
      currentStepperPos--;
    }
  }

  delay(1);
}

void runCountdown() {

  timerRunning = true;
  lcd.clear();

  for (int seconds = enteredSeconds; seconds >= 0; seconds--) {

    lcd.setCursor(0, 0);
    lcd.print("Timer: ");
    if (seconds < 10) lcd.print("0");
    lcd.print(seconds);
    lcd.print(" sec   ");

    lcd.setCursor(0, 1);
    lcd.print(seconds == 0 ? "DONE!          " : "Counting...    ");

    delay(1000);
  }

  performWave();

  delay(1000);

  timerRunning = false;
  showIdleMessage();
}

void rainbowDelay(int durationMs) {
  unsigned long start = millis();
  int colorIndex = 0;

  while (millis() - start < durationMs) {

    switch (colorIndex) {
      case 0:  setColor(255, 0, 0); break;       // Red
      case 1:  setColor(255, 80, 0); break;      // Orange
      case 2:  setColor(255, 255, 0); break;     // Yellow
      case 3:  setColor(0, 255, 0); break;       // Green
      case 4:  setColor(0, 255, 150); break;     // Mint
      case 5:  setColor(0, 255, 255); break;     // Cyan
      case 6:  setColor(0, 120, 255); break;     // Sky Blue
      case 7:  setColor(0, 0, 255); break;       // Blue
      case 8:  setColor(120, 0, 255); break;     // Purple
      case 9:  setColor(255, 0, 255); break;     // Magenta
      case 10: setColor(255, 0, 120); break;     // Hot Pink
      case 11: setColor(255, 255, 255); break;   // White flash
    }

    colorIndex++;
    if (colorIndex > 11) colorIndex = 0;

    delay(120);  // adjust speed (lower = crazier)
  }
}


void setColor(int r, int g, int b) {
  analogWrite(RED, 255 - r);
  analogWrite(GREEN, 255 - g);
  analogWrite(BLUE, 255 - b);
}

void performWave() {

  lcd.setCursor(0, 1);
  lcd.print("Waving!        ");

  int originalPos = 90;

  forearmServo.write(90);
  wristServo.write(90);
  rainbowDelay(300);
  for(int i=0; i<4; i++) {
    wristServo.write(originalPos + 30);
    forearmServo.write(originalPos - 10);
    rainbowDelay(400);

    wristServo.write(originalPos - 30);
    forearmServo.write(originalPos + 10);
    rainbowDelay(400);

  }

    forearmPos = 120;
    wristPos = 180;
    forearmServo.write((int)forearmPos);
    wristServo.write((int)wristPos);
}

void showIdleMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready PomoRobo");
  lcd.setCursor(0, 1);
  lcd.print("Press Start");
}

bool isValidButton(uint32_t code) {
  return (code == 0xBB44FF00 ||
          code == 0xBC43FF00 ||
          code == 0xF609FF00 ||
          code == 0xF807FF00 ||
          code == 0xE619FF00 ||
          code == 0xF20DFF00 ||
          code == 0xBA45FF00);
}

void updateMovement(uint32_t code) {
  switch (code) {
    case 0xBB44FF00: stepperDir = 1; break;
    case 0xBC43FF00: stepperDir = -1; break;

    case 0xF609FF00:
      forearmPos = constrain(forearmPos + SERVO_INCREMENT, 0, 180);
      forearmServo.write((int)forearmPos);
      stepperDir = 0; break;

    case 0xF807FF00:
      forearmPos = constrain(forearmPos - SERVO_INCREMENT, 0, 180);
      forearmServo.write((int)forearmPos);
      stepperDir = 0; break;

    case 0xE619FF00:
      wristPos = constrain(wristPos + SERVO_INCREMENT, 0, 180);
      wristServo.write((int)wristPos);
      stepperDir = 0; break;

    case 0xF20DFF00:
      wristPos = constrain(wristPos - SERVO_INCREMENT, 0, 180);
      wristServo.write((int)wristPos);
      stepperDir = 0; break;

    case 0xBA45FF00:
      forearmPos = 120;
      wristPos = 180;
      forearmServo.write((int)forearmPos);
      wristServo.write((int)wristPos);
      break;
  }
}