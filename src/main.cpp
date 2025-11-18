// ================================================================
//  ESP32-C3 Super Mini + Quectel EC200U (4G) + Firebase Realtime DB
//  Upload GPS every 5 seconds to:
//  https://obm-gps-default-rtdb.firebaseio.com/gps.json?auth=SECRET
// ================================================================
#include <Arduino.h>
#define RX_PIN 4      // ESP32 UART1 RX -> EC200U TX
#define TX_PIN 5      // ESP32 UART1 TX -> EC200U RX

HardwareSerial gsm(1); // UART1

String gsm_response;
unsigned long lastGPS = 0;

// Firebase details
String firebaseHost = "https://obm-gps-default-rtdb.firebaseio.com/gps.json?auth=sJiErOexGgZkC2NHAZAmupP92LqEp0dV2JKLGAE0";

// Function declarations
bool getResponse(String cmd, String expected, int timeout);
bool startGPS();
String readGPS();
bool uploadToFirebase(float lat, float lon, String timeStr);

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\nStarting EC200U...");
  gsm.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(3000);

  // Basic module check
  while (!getResponse("AT", "OK", 2000)) {
    Serial.println("Retrying...");
    delay(500);
  }

  getResponse("ATE0", "OK", 1000);          // Disable echo
  getResponse("AT+CMEE=2", "OK", 1000);     // Verbose error
  getResponse("AT+CREG?", "OK", 2000);      // Check network

  // Start GNSS
  if (startGPS()) Serial.println("GPS started.");
  else Serial.println("GPS failed to start.");
}

void loop() {
  if (millis() - lastGPS >= 5000) {  // Every 5 seconds
    lastGPS = millis();

    String gps = readGPS();
    Serial.println("GPS RAW: " + gps);

    if (gps.indexOf("+QGPSLOC:") >= 0) {
      // ---------------------------------------------------------
      // Parse GPS data
      // Format: +QGPSLOC: <time>, <lat>, <lon>, ...
      // Example:
      // +QGPSLOC: 20240212 121530.0,9.987654,76.456789,...
      // ---------------------------------------------------------

      int comma1 = gps.indexOf(',');
      int comma2 = gps.indexOf(',', comma1 + 1);

      String timeStr = gps.substring(gps.indexOf(":") + 2, comma1);
      float lat = gps.substring(comma1 + 1, comma2).toFloat();
      float lon = gps.substring(comma2 + 1).toFloat();

      Serial.println("LAT: " + String(lat, 6));
      Serial.println("LON: " + String(lon, 6));
      Serial.println("TIME: " + timeStr);

      // Upload to Firebase
      uploadToFirebase(lat, lon, timeStr);
    }
  }
}



// =====================================================
// SEND AT COMMAND AND WAIT FOR RESPONSE
// =====================================================
bool getResponse(String cmd, String expected, int timeout) {
  gsm_response = "";
  Serial.println("> " + cmd);
  gsm.println(cmd);

  long start = millis();
  while (millis() - start < timeout) {
    if (gsm.available()) {
      gsm_response += gsm.readString();
      gsm_response.trim();

      if (gsm_response.indexOf(expected) >= 0) {
        Serial.println(gsm_response);
        return true;
      }
    }
  }

  Serial.println("Timeout: " + gsm_response);
  return false;
}



// =====================================================
// START GNSS
// =====================================================
bool startGPS() {
  return getResponse("AT+QGPS=1", "OK", 2000);
}



// =====================================================
// GET GPS LOCATION
// =====================================================
String readGPS() {
  gsm.println("AT+QGPSLOC=2");
  delay(1200);

  String resp = "";
  while (gsm.available()) {
    resp += gsm.readString();
  }
  resp.trim();
  return resp;
}



// =====================================================
// UPLOAD GPS DATA TO FIREBASE (4G HTTP POST)
// =====================================================
bool uploadToFirebase(float lat, float lon, String timeStr) {
  // Create JSON
  String json = "{";
  json += "\"lat\":" + String(lat, 6) + ",";
  json += "\"lon\":" + String(lon, 6) + ",";
  json += "\"time\":\"" + timeStr + "\"";
  json += "}";

  Serial.println("Uploading JSON: " + json);

  // Send URL
  gsm.println("AT+QHTTPURL=" + String(firebaseHost.length()) + ",80");
  delay(500);
  gsm.print(firebaseHost);
  delay(500);

  // POST JSON
  gsm.println("AT+QHTTPPOST=" + String(json.length()) + ",80,80");
  delay(300);
  gsm.print(json);

  delay(2000);

  String response = "";
  while (gsm.available()) {
    response += gsm.readString();
  }
  response.trim();

  Serial.println("Firebase Response: " + response);
  return (response.indexOf("200") >= 0);
}
