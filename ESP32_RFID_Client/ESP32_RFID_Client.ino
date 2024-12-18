#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN 22
#define SS_PIN 5

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Replace with your AP credentials
const char* ssid = "ESP32-AP";
const char* password = "12345678";

const char* serverIP = "192.168.4.1";  // Change to the IP of your Access Point ESP32
const int serverPort = 80;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  Serial.println("Connecting to Access Point...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Access Point");
}

void loop() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      uid += String(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println("RFID UID: " + uid);
    sendUID(uid);
    delay(1000);  // Debounce delay
  }
}

void sendUID(String uid) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + serverIP + "/sendUID";

    http.begin(url);
    http.addHeader("Content-Type", "text/plain");

    int httpResponseCode = http.POST(uid);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response: " + response);
    } else {
      Serial.println("Error in sending UID: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}
