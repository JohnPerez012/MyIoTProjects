#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>


/**
	KEYS --
	  C1 - C4 TO D5 - D2
	  R1 - R4 TO D9 - D6

	
	BUZZER --
	  ANODE (+) TO D13 (resistor AT LEAST 220Ω)
	  CATHODE (-) TO GND


	PUSH BUTTON --
	  ANODE (+) TO D1
	  CATHODE (-) TO GND

	IIC LCD --
	  GND TO GND
	  Vcc TO 5V
	  SDA TO A4
  	  SCL TO A5
*/






LiquidCrystal_I2C lcd(0x27, 16, 2);

const int buzzerPin = 13;
const int shiftButtonPin = 1;  // Shift button pin

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', '+'},
  {'4', '5', '6', '-'},
  {'7', '8', '9', '*'},
  {'C', '0', '=', '/'}
};

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String firstStr = "";
String secondStr = "";
char op = 0;
bool enteringFirst = true;
bool shiftMode = false;
bool shiftBeeped = false;
bool showingResult = false;
bool muted = false; // Flag for mute/unmute
bool shiftCaretSet = false; // Flag to track if caret was set in shift mode
int caretX = 0;
int caretY = 0;
char displayBuffer[2][17] = {
  "                ",
  "                "
};
unsigned long lastBlinkTime = 0;
bool caretVisible = false;
const int BLINK_INTERVAL = 500; // Blink interval in milliseconds

String tempFirstStr = "*";
String tempSecondStr = "";
char tempOp = '*0';
int tempCaretX = 0;
int tempCaretY = '*0';
bool tempShiftCaretSet = false;
bool tempEnteringFirst = false;


void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(shiftButtonPin, INPUT_PULLUP);  // Active-low
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready...");
  strcpy(displayBuffer[0], "Ready...        ");
  strcpy(displayBuffer[1], "                ");
  delay(100); // Ensure LCD initializes properly
}

void loop() {
  // Handle shift mode
  bool newShiftMode = digitalRead(shiftButtonPin) == LOW;
  if (newShiftMode != shiftMode) {
    shiftMode = newShiftMode;
    if (shiftMode && !shiftBeeped) {
      beep(); // Beep twice when entering shift mode, if not muted
      delay(100);
      beep();
      shiftBeeped = true;
    } else {
      shiftBeeped = false;
    }
    refreshBufferToLCD(); // Update display when shift mode changes
  }

  // Handle caret blinking in shift mode
  if (shiftMode) {
    unsigned long currentTime = millis();
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
      caretVisible = !caretVisible;
      lastBlinkTime = currentTime;
      // Update only the caret position
      lcd.setCursor(caretX, caretY);
      lcd.print(caretVisible ? '_' : displayBuffer[caretY][caretX]);
    }
  } else if (caretVisible) {
    // Clear caret when exiting shift mode
    caretVisible = false;
    lcd.setCursor(caretX, caretY);
    lcd.print(displayBuffer[caretY][caretX]);
  }

  char key = keypad.getKey();
  if (!key) return;

  // Beep for key press, unless it's '0' in shift mode (mute toggle)
  if (!(shiftMode && key == '0')) {
    beep(); // Beep for key press, if not muted
  }

  if (shiftMode) {
    int prevCaretX = caretX;
    int prevCaretY = caretY;
    handleShiftMode(key);
    if (caretX != prevCaretX || caretY != prevCaretY) {
      // Restore previous caret position character
      lcd.setCursor(prevCaretX, prevCaretY);
      lcd.print(displayBuffer[prevCaretY][prevCaretX]);
    }
  } else {
    handleCalcMode(key);
  }

  refreshBufferToLCD();
  lcd.setCursor(caretX, caretY);
}

void handleShiftMode(char key) {
  if (key == '2' && caretY > 0) {
    caretY--;
    shiftCaretSet = true; // Mark caret as set in shift mode
  } else if (key == '8' && caretY < 1) {
    caretY++;
    shiftCaretSet = true;
  } else if (key == '4' && caretX > 0) {
    caretX--;
    shiftCaretSet = true;
  } else if (key == '6' && caretX < 15) {
    caretX++;
    shiftCaretSet = true;
  } else if (key == '5') {
    // Delete character at caret position
    if (caretY == 0 && caretX < firstStr.length()) {
      firstStr.remove(caretX, 1); // Remove character from firstStr
    } else if (caretY == 0 && caretX == 15 && op != 0) {
      op = 0; // Clear operator
      enteringFirst = true;
    } else if (caretY == 1 && caretX < secondStr.length()) {
      secondStr.remove(caretX, 1); // Remove character from secondStr
    }
    displayBuffer[caretY][caretX] = ' ';
    updateDisplay(); // Synchronize state after deletion
  } else if (key == 'C') {
    // Backspace: delete character to the left of caret
    if (caretY == 0 && caretX > 0 && caretX <= firstStr.length()) {
      firstStr.remove(caretX - 1, 1); // Remove character to the left
      caretX--; // Move caret left
    } else if (caretY == 0 && caretX == 15 && op != 0) {
      op = 0; // Clear operator
      enteringFirst = true;
      caretX--; // Move caret left
    } else if (caretY == 1 && caretX > 0 && caretX <= secondStr.length()) {
      secondStr.remove(caretX - 1, 1); // Remove character to the left
      caretX--; // Move caret left
    }
    updateDisplay(); // Synchronize state after deletion
  } else if (key >= '0' && key <= '9') {
    // Handle mute/unmute toggle with '0' key
    if (key == '0') {
      muted = !muted; // Toggle mute state
      memset(displayBuffer[0], ' ', 16);
      memset(displayBuffer[1], ' ', 16);
      displayBuffer[0][16] = '\0';
      displayBuffer[1][16] = '\0';
      strcpy(displayBuffer[0], muted ? "Muted           " : "Unmuted         ");
      strcpy(displayBuffer[1], "                ");
      refreshBufferToLCD();
      delay(1000); // Display mute status for 1 second
      updateDisplay(); // Restore previous display
    } else {
      // Update state variables with new digit
      if (caretY == 0 && caretX <= firstStr.length() && caretX < 15) {
        if (caretX == firstStr.length()) {
          firstStr += key;
        } else {
          if (caretX < firstStr.length()) {
            firstStr[caretX] = key;
          } else {
            firstStr += key;
          }
        }
      } else if (caretY == 1 && caretX <= secondStr.length()) {
        if (caretX == secondStr.length()) {
          secondStr += key;
        } else {
          if (caretX < secondStr.length()) {
            secondStr[caretX] = key;
          } else {
            secondStr += key;
          }
        }
      }
      displayBuffer[caretY][caretX] = key;
      updateDisplay(); // Synchronize state after digit input
    }
  }
}

void handleCalcMode(char key) {
  // If showing result, clear it on new input (digit or operator)
 if (showingResult && (key >= '0' && key <= '9' || key == '+' || key == '-' || key == '*' || key == '/')) {
    showingResult = false;
    firstStr = tempFirstStr;
    secondStr = tempSecondStr;
    op = tempOp;
    caretX = tempCaretX;
    caretY = tempCaretY;
    shiftCaretSet = tempShiftCaretSet;
    enteringFirst = tempEnteringFirst;
    updateDisplay();
  }

  if (key >= '0' && key <= '9') {
    if (shiftCaretSet) {
      // Insert or replace digit at caret position
      if (caretY == 0 && caretX <= firstStr.length() && caretX < 15) {
        if (caretX == firstStr.length()) {
          firstStr += key;
        } else {
          if (caretX < firstStr.length()) {
            firstStr[caretX] = key;
          } else {
            firstStr += key;
          }
        }
        caretX++; // Move caret right after insertion
      } else if (caretY == 1 && caretX <= secondStr.length()) {
        if (caretX == secondStr.length()) {
          secondStr += key;
        } else {
          if (caretX < secondStr.length()) {
            secondStr[caretX] = key;
          } else {
            secondStr += key;
          }
        }
        caretX++;
      }
      updateDisplay();
    } else {
      // Default behavior: append digit
      if (enteringFirst) {
        firstStr += key;
      } else {
        secondStr += key;
      }
      updateDisplay();
    }
  } else if (key == '+' || key == '-' || key == '*' || key == '/') {
    if (firstStr != "") {
      op = key; // Allow changing the operator
      enteringFirst = false;
      shiftCaretSet = false; // Reset shift caret when setting operator
      updateDisplay();
    }
  } else if (key == '=') {
    if (op == 0) {
      tempFirstStr = firstStr;
      tempSecondStr = secondStr;
      tempOp = op;
      tempCaretX = caretX;
      tempCaretY = caretY;
      tempShiftCaretSet = shiftCaretSet;
      tempEnteringFirst = enteringFirst;
      memset(displayBuffer[0], ' ', 16);
      memset(displayBuffer[1], ' ', 16);
      displayBuffer[0][16] = '\0';
      displayBuffer[1][16] = '\0';
      strcpy(displayBuffer[0], "Error:  ENTER AN");
      strcpy(displayBuffer[1], "NO OP.  OPERATOR");
      showingResult = true;
      refreshBufferToLCD();
    } else if (firstStr != "" && secondStr != "" && op != 0) {
      float num1 = firstStr.toFloat();
      float num2 = secondStr.toFloat();
      if (op == '/' && num2 == 0) {
        memset(displayBuffer[0], ' ', 16);
        memset(displayBuffer[1], ' ', 16);
        displayBuffer[0][16] = '\0';
        displayBuffer[1][16] = '\0';
        strcpy(displayBuffer[0], "Error: Divide 0 ");
        strcpy(displayBuffer[1], "                ");
        caretX = 0;
        caretY = 0;
        showingResult = true;
        shiftCaretSet = false; // Reset shift caret on result
        refreshBufferToLCD();
        delay(2000); // Display error for 2 seconds
        resetCalc();
        updateDisplay();
      } else {
        float result = calculate(num1, num2, op);
        memset(displayBuffer[0], ' ', 16);
        memset(displayBuffer[1], ' ', 16);
        displayBuffer[0][16] = '\0';
        displayBuffer[1][16] = '\0';
        strcpy(displayBuffer[0], "Result:         ");
        String resultStr = String(result, 2); // Format to 2 decimal places
        strncpy(displayBuffer[1], resultStr.c_str(), 16);
        caretX = 0;
        caretY = 1;
        showingResult = true;
        shiftCaretSet = false; // Reset shift caret on result
        refreshBufferToLCD();
    showingResult = false;resetCalc();
        // Result persists until new input
      }
    }
  } else if (key == 'C') {
    resetCalc();
    showingResult = false;
    shiftCaretSet = false; // Reset shift caret on clear
    memset(displayBuffer[0], ' ', 16);
    memset(displayBuffer[1], ' ', 16);
    displayBuffer[0][16] = '\0';
    displayBuffer[1][16] = '\0';
    strcpy(displayBuffer[0], "Cleared         ");
    strcpy(displayBuffer[1], "                ");
    caretX = 0;
    caretY = 0;
    refreshBufferToLCD();
    delay(100); // Ensure LCD updates
  }
}

void updateDisplay() {
  memset(displayBuffer[0], ' ', 16);
  memset(displayBuffer[1], ' ', 16);
  displayBuffer[0][16] = '\0';
  displayBuffer[1][16] = '\0';

  for (int i = 0; i < firstStr.length() && i < 15; i++) {
    displayBuffer[0][i] = firstStr[i];
  }
  if (op != 0) {
    displayBuffer[0][15] = op;
    for (int i = 0; i < secondStr.length() && i < 16; i++) {
      displayBuffer[1][i] = secondStr[i];
    }
    if (!shiftMode && !showingResult && !shiftCaretSet) {
      caretX = secondStr.length() < 16 ? secondStr.length() : 15;
      caretY = 1;
    }
  } else {
    if (!shiftMode && !showingResult && !shiftCaretSet) {
      caretX = firstStr.length() < 16 ? firstStr.length() : 15;
      caretY = 0;
    }
  }
}

void refreshBufferToLCD() {
  lcd.clear();
  delay(10); // Small delay to ensure LCD processes clear
  lcd.setCursor(0, 0);
  lcd.print(displayBuffer[0]);
  lcd.setCursor(0, 1);
  lcd.print(displayBuffer[1]);
}

float calculate(float num1, float num2, char oper) {
  switch (oper) {
    case '+': return num1 + num2;
    case '-': return num1 - num2;
    case '*': return num1 * num2;
    case '/': return num2 != 0 ? num1 / num2 : 0;
    default: return 0;
  }
}

void beep() {
  if (!muted) { // Only beep if not muted
    digitalWrite(buzzerPin, HIGH);
    delay(50);
    digitalWrite(buzzerPin, LOW);
  }
}

void resetCalc() {
  firstStr = "";
  secondStr = "";
  op = 0;
  enteringFirst = true;
}
