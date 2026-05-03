#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo myServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// RFID UID
byte allowedUID[] = {0x24, 0x61, 0x74, 0x6};

// KEYPAD
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'D','C','B','A'},
  {'#','9','6','3'},
  {'0','8','5','2'},
  {'*','7','4','1'}
};

byte rowPins[ROWS] = {A3, A2, A1, A0};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// PIN
String correctPIN = "";
String enteredPIN = "";

// CHANGE MODE
bool changeMode = false;
String newPIN = "";
String confirmPIN = "";

// LONG PRESS
unsigned long keyPressTime = 0;
bool hashPressed = false;
bool requestPinChange = false;

// SECURITY
int wrongAttempts = 0;
bool lockActive = false;
unsigned long lockTime = 0;

// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  SPI.begin();
  mfrc522.PCD_Init();

  myServo.attach(6);
  myServo.write(0);

  lcd.init();
  lcd.backlight();

  bool valid = true;
  for(int i=0;i<4;i++){
    char c = EEPROM.read(i);
    if(c < '0' || c > '9'){
      valid = false;
      break;
    }
    correctPIN += c;
  }

  if(!valid || correctPIN.length()!=4){
    correctPIN = "1234";
    for(int i=0;i<4;i++){
      EEPROM.update(i, correctPIN[i]);
    }
  }

  lcd.print("Smart Door Lock");
  lcd.setCursor(0,1);
  lcd.print("Welcome!");
  delay(2000);

  lcd.clear();
  lcd.print("Scan / Enter PIN");
}

// ===== LOOP =====
void loop() {

  // RFID FIRST
  if (checkRFID()) return;

  // LOCK MODE
  if (lockActive) {
    unsigned long elapsed = millis() - lockTime;

    if (elapsed < 300000) {
      lcd.setCursor(0,0);
      lcd.print("SYSTEM LOCKED");
      delay(1000);
      return;
    } else {
      lockActive = false;
      wrongAttempts = 4;

      lcd.clear();
      lcd.print("Try Again");
      delay(2000);

      lcd.clear();
      lcd.print("Scan / Enter PIN");
    }
  }

  // KEYPAD
  char key = keypad.getKey();

  // LONG PRESS #
  if (key == '#') {
    if (!hashPressed) {
      hashPressed = true;
      keyPressTime = millis();
    }
  }

  if (hashPressed && key == NO_KEY) {
    if (millis() - keyPressTime > 2000) {
      requestPinChange = true;

      lcd.clear();
      lcd.print("Scan RFID to");
      lcd.setCursor(0,1);
      lcd.print("Change PIN");
    }
    hashPressed = false;
  }

  if (!key && !requestPinChange) return;

  // CHANGE MODE
  if (changeMode) {

    newPIN += key;
    lcd.setCursor(0,1);
    lcd.print("*");

    if (newPIN.length() == 4) {

      lcd.clear();
      lcd.print("Confirm PIN");
      confirmPIN = "";

      while (confirmPIN.length() < 4) {
        char k = keypad.getKey();
        if (k) {
          confirmPIN += k;
          lcd.setCursor(0,1);
          lcd.print("*");
        }
      }

      if (newPIN == confirmPIN) {
        correctPIN = newPIN;

        for(int i=0;i<4;i++){
          EEPROM.update(i, correctPIN[i]);
        }

        lcd.clear();
        lcd.print("PIN Updated");
      } else {
        lcd.clear();
        lcd.print("Mismatch");
      }

      delay(2000);
      changeMode = false;

      lcd.clear();
      lcd.print("Scan / Enter PIN");
    }
    return;
  }

  // NORMAL PIN
  if (key) {
    enteredPIN += key;
    lcd.setCursor(0,1);
    lcd.print("*");

    if (enteredPIN.length() == 4) {

      lcd.clear();

      if (enteredPIN == correctPIN) {
        lcd.print("PIN Correct");
        openDoor();
        wrongAttempts = 0;
      } else {
        wrongAttempts++;
        lcd.print("Wrong PIN");
      }

      enteredPIN = "";
      delay(2000);

      lcd.clear();
      lcd.print("Scan / Enter PIN");
    }
  }
}

// ===== RFID =====
bool checkRFID(){

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

    bool match = true;

    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] != allowedUID[i]) {
        match = false;
      }
    }

    lcd.clear();

    if (match) {

      // 👉 PIN CHANGE FLOW
      if (requestPinChange) {
        changeMode = true;
        requestPinChange = false;

        lcd.print("Enter New PIN");
        return true;
      }

      // 👉 NORMAL ACCESS
      lcd.print("Access Granted");
      openDoor();

      lockActive = false;
      wrongAttempts = 0;
      lockTime = 0;

      lcd.clear();
      lcd.print("Scan / Enter PIN");

      return true;
    } 
    else {
      lcd.print("Access Denied");
      delay(2000);

      lcd.clear();
      lcd.print("Scan / Enter PIN");
      return true;
    }
  }

  return false;
}

// ===== DOOR =====
void openDoor() {
  lcd.clear();
  lcd.print("Door Opening");
  myServo.write(90);
  delay(3000);

  lcd.clear();
  lcd.print("Door Closing");
  myServo.write(0);
  delay(2000);
}
