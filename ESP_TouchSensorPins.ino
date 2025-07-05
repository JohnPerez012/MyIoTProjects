#define TOUCH_PIN     T0       // T0 is GPIO 4
#define LED_BUILTIN   2        // Built-in LED on most ESP32 boards

int touchThreshold = 10;       // Lower = more sensitive

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  int touchValue = touchRead(TOUCH_PIN);
  Serial.println(touchValue);  // For debugging — see raw touch values

  if (touchValue < touchThreshold) {
    digitalWrite(LED_BUILTIN, HIGH); // Turn on LED
  } else {
    digitalWrite(LED_BUILTIN, LOW);  // Turn off LED
  }

  delay(100); // Small delay to reduce serial spam
}
