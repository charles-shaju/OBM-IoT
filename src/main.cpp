// =======================================
// EC200U + ESP32-C3 + Firebase GPS Uploader
// Airtel APN + RAW HTTP REST (no API key)
// =======================================

#include <Arduino.h>

#define SerialMon Serial
#define SerialAT Serial1  // UART1 on ESP32-C3

// ESP32-C3 UART pins
#define EC200U_TX 4    // ESP32-C3 TX → EC200U RX
#define EC200U_RX 5    // ESP32-C3 RX → EC200U TX

// Airtel APN
String APN = "airtelgprs.com";

// Firebase URL
String FIREBASE_URL = "https://obm-gps-default-rtdb.firebaseio.com/gps.json";

// Buffer
String atResponse = "";

// ----------------------------------------------------
// Send AT command and wait for response
// ----------------------------------------------------
String sendAT(String cmd, uint16_t timeout = 3000) {
  SerialAT.println(cmd);
  long t = millis();
  String resp = "";

  while (millis() - t < timeout) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      resp += c;
    }
  }

  // Debug output
  SerialMon.println("CMD: " + cmd);
  SerialMon.println("RSP: " + resp);
  SerialMon.println("-----------------------------");

  return resp;
}

// ----------------------------------------------------
// Parse +QGPSLOC
// Format: +QGPSLOC: hhmmss.sss,lat,lon,....
// ----------------------------------------------------
bool parseGPS(String raw, String &lat, String &lon, String &timeStr) {
  if (!raw.startsWith("+QGPSLOC")) return false;

  raw.replace("+QGPSLOC: ", "");
  raw.replace("\r", "");
  raw.replace("\n", "");

  int part = 0;
  String temp = "";

  for (int i = 0; i < raw.length(); i++) {
    if (raw[i] == ',' || i == raw.length() - 1) {
      if (i == raw.length() - 1) temp += raw[i];

      if (part == 0) timeStr = temp;
      if (part == 1) lat = temp;
      if (part == 2) lon = temp;

      temp = "";
      part++;
    } else {
      temp += raw[i];
    }
  }

  return true;
}

// ----------------------------------------------------
// Upload JSON to Firebase
// ----------------------------------------------------
void uploadToFirebase(String lat, String lon, String t) {
  String json = "{\"lat\":" + lat + ",\"lon\":" + lon + ",\"time\":\"" + t + "\"}";

  SerialMon.println("Uploading JSON:");
  SerialMon.println(json);

  // Set URL
  sendAT("AT+QHTTPURL=" + String(FIREBASE_URL.length()) + ",80");
  delay(200);
  SerialAT.print(FIREBASE_URL);
  delay(500);

  // POST JSON
  sendAT("AT+QHTTPPOST=" + String(json.length()) + ",80,80");
  delay(200);
  SerialAT.print(json);

  delay(1000);

  // Read response
  sendAT("AT+QHTTPREAD");
}

// ----------------------------------------------------
// Setup
// ----------------------------------------------------
void setup() {
  SerialMon.begin(115200);
  delay(2000);

  SerialMon.println("=== EC200U + ESP32-C3 Firebase GPS Uploader ===");

  SerialAT.begin(115200, SERIAL_8N1, EC200U_RX, EC200U_TX);
  delay(1000);

  // Basic init
  sendAT("AT");
  sendAT("AT+CPIN?");
  sendAT("AT+CFUN=1");
  sendAT("AT+CREG?");
  sendAT("AT+CGATT=1");

  // Set APN
  sendAT("AT+QICSGP=1,1,\"" + APN + "\",\"\",\"\",1");

  // Activate PDP
  sendAT("AT+QIACT=1", 8000);
  sendAT("AT+QIACT?");

  // Enable GPS
  sendAT("AT+QGPS=1", 2000);
}

// ----------------------------------------------------
// Loop
// ----------------------------------------------------
void loop() {

  // read GPS
  String raw = sendAT("AT+QGPSLOC=1", 1500);

  String lat, lon, t;

  if (parseGPS(raw, lat, lon, t)) {
    SerialMon.println("Parsed lat=" + lat + " lon=" + lon + " time=" + t);

    uploadToFirebase(lat, lon, t);
  } else {
    SerialMon.println("GPS Fix Not Available");
  }

  delay(5000); // upload every 5 seconds
}
