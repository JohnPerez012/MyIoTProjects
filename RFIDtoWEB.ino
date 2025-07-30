#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>


/**
	RDIF --
	  SDA TO 15
	  SCK TO 18
    MOSI TO 23
    MISO TO 19
    RST TO 27
    GND TO GND
    3.3V TO 3.3V

	
	BUZZER --
	  ANODE (+) TO 5
	  CATHODE (-) TO GND

	LED --
	  RED: ANODE (+) TO  22
   	GREEN: ANODE (+) TO  21
	  CATHODE (-) TO GND (Resistor AT LEAST 220Ω)
*/



// Wi-Fi
const char* ssid = "WIFI NAME";
const char* password = "WIFI PASSWORD";

// RFID
#define SS_PIN 15
#define RST_PIN 27
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Pins
#define BUZZER_PIN 5
#define RED_LED 22
#define GREEN_LED 21

// Server & state
WebServer server(80);
bool scanningEnabled = false;

// ASP.NET endpoint
const char* serverURL = "❌ LOCAL ADDRESS. ✅ COMPUTER'S IPV4 ADDRESS POINT TO WEB'S LOCAL FILE";

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, HIGH);    // Red = not ready
  digitalWrite(GREEN_LED, LOW);   // Green OFF initially

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  SPI.begin();
  mfrc522.PCD_Init();

  // Routes
  server.on("/startScan", []() {
    scanningEnabled = true;
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    server.send(200, "text/plain", "✅ RFID Scan Enabled");
  });

  server.on("/stopScan", []() {
    scanningEnabled = false;
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    server.send(200, "text/plain", "🟥 RFID Scan Disabled");
  });

  server.begin();
  Serial.println("ESP32 ready. Waiting for scan commands...");
}

void loop() {
  server.handleClient();

  if (!scanningEnabled) return;

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(100);
    return;
  }

  // Read UID
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    char hexBuffer[4];
    sprintf(hexBuffer, "%02X", mfrc522.uid.uidByte[i]);
    uid += hexBuffer;
  }

  Serial.println("Card UID: " + uid);

  // Send to server
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String postData = "rfid=" + uid;
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode == 200) {
      tone(BUZZER_PIN, 1000, 100); delay(150);
      tone(BUZZER_PIN, 1000, 100);
    } else {
      tone(BUZZER_PIN, 500, 300);
    }

    http.end();
  }

  delay(2000);
  mfrc522.PICC_HaltA();
}
