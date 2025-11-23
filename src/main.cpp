// =======================================
// EC200U + ESP32-C3 + Firebase GPS Uploader
// Optimized for Reliability & Correct JSON
// =======================================
#include <Arduino.h>
#include "secrets.h"  // Contains Firebase URL and credentials

#define RX_PIN 4
#define TX_PIN 5

HardwareSerial gsm(1);
String gsm_response = "";

// Device identifier (loaded from secrets.h)
String deviceID = DEVICE_ID;

// Firebase URL (loaded from secrets.h)
String firebaseURL = FIREBASE_URL;

unsigned long lastSend = 0;

// FUNCTIONS
bool sendAT(String cmd, String expect, uint16_t timeout, bool debug = true);
bool sendFirebase(String json);
String getGPS();
String detectAPN();

void setup() {
    Serial.begin(115200);
    gsm.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

    delay(3000);
    Serial.println("Booting EC200U...");

    // Turn off echo
    sendAT("ATE0", "OK", 1000);

    // Check AT command
    if (!sendAT("AT", "OK", 1000)) {
        Serial.println("FATAL: GSM module not responding.");
        while(1);
    }

    // Start GPS (Wait longer, as this takes time to activate)
    if (sendAT("AT+QGPS=1", "OK", 5000))
        Serial.println("GPS Started");
    
    // Deactivate existing context before setting new one (improves stability)
    sendAT("AT+QIDEACT=1", "OK", 2000, false);

    // Auto-detect SIM operator and configure APN
    String apn = detectAPN();
    Serial.println("Using APN: " + apn);
    
    String apnCmd = "AT+QICSGP=1,1,\"" + apn + "\",\"\",\"\",1";
    if (!sendAT(apnCmd, "OK", 5000)) {
        Serial.println("FATAL: Failed to set APN.");
        while(1);
    }

    // Activate PDP (Wait longer, this is the connection step)
    if (!sendAT("AT+QIACT=1", "OK", 10000)) {
        Serial.println("FATAL: Failed to activate GPRS/LTE.");
        while(1);
    }

    Serial.println("GSM/GPS Ready!");
}

void loop() {

    // Send data every 10 seconds (2s is too frequent and can cause network/module instability)
    if (millis() - lastSend >= 10000) { 
        lastSend = millis();

        // 1. Get RAW GPS Data
        String gps_raw = getGPS();
        Serial.println("GPS Raw → " + gps_raw);

        if (gps_raw.indexOf("+QGPSLOC:") == -1) {
            Serial.println("GPS Fix Not Available / No response.");
            return;
        }

        // 2. Parse Fields
        // EC200U Format: +QGPSLOC: <time>,<lon>,<lat>,<alt>,<speed>,<course>,<fix>,<date>,<nsat>
        // Fields: time, longitude, latitude, altitude, speed, course(heading), fixmode, date, nsat, ...
        
        // Remove prefix and trim
        gps_raw.replace("+QGPSLOC: ", ""); 
        gps_raw.trim();
        
        // Find indices for all required fields
        int i1 = gps_raw.indexOf(','); // after Time
        int i2 = gps_raw.indexOf(',', i1 + 1); // after LON
        int i3 = gps_raw.indexOf(',', i2 + 1); // after LAT
        int i4 = gps_raw.indexOf(',', i3 + 1); // after ALT
        int i5 = gps_raw.indexOf(',', i4 + 1); // after SPEED
        int i6 = gps_raw.indexOf(',', i5 + 1); // after COURSE/HEADING
        int i7 = gps_raw.indexOf(',', i6 + 1); // after FIX MODE
        int i8 = gps_raw.indexOf(',', i7 + 1); // after DATE
        int i9 = gps_raw.indexOf(',', i8 + 1); // after NSAT
        int i10 = gps_raw.indexOf(',', i9 + 1); // after HDOP

        if (i9 == -1) {
            Serial.println("ERROR: Incomplete GPS data string.");
            return;
        }

        String longitude = gps_raw.substring(i1 + 1, i2);
        String latitude  = gps_raw.substring(i2 + 1, i3);
        String altitude  = gps_raw.substring(i3 + 1, i4);
        String speed     = gps_raw.substring(i4 + 1, i5);
        String heading   = gps_raw.substring(i5 + 1, i6);
        String satellites = gps_raw.substring(i8 + 1, i9);
        String hdop      = (i10 != -1) ? gps_raw.substring(i9 + 1, i10) : "0";
        
        // 3. Get GSM Signal Strength
        String signalStrength = "0";
        if (sendAT("AT+CSQ", "+CSQ:", 1000, false)) {
            int csqIndex = gsm_response.indexOf("+CSQ:");
            if (csqIndex != -1) {
                String csqData = gsm_response.substring(csqIndex + 6);
                int commaIdx = csqData.indexOf(',');
                if (commaIdx != -1) {
                    signalStrength = csqData.substring(0, commaIdx);
                }
            }
        }
        
        // 4. Build Firebase JSON (All values as numbers, not strings)
        String json = "{";
        json += "\"device_id\":\"" + deviceID + "\",";
        json += "\"latitude\":" + latitude + ",";
        json += "\"longitude\":" + longitude + ",";
        json += "\"altitude\":" + altitude + ",";
        json += "\"speed\":" + speed + ",";
        json += "\"heading\":" + heading + ",";
        json += "\"satellites\":" + satellites + ",";
        json += "\"hdop\":" + hdop + ",";
        json += "\"signal_strength\":" + signalStrength + ",";
        json += "\"timestamp\":{\"sv\":\"timestamp\"}";
        json += "}";

        // 5. Send to Firebase
        sendFirebase(json);
    }
}


// ===============================================================
// SEND AT COMMAND (Improved for Debugging)
// ===============================================================
bool sendAT(String cmd, String expect, uint16_t timeout, bool debug) {
    if (debug) Serial.println("> " + cmd);
    gsm_response = "";

    gsm.println(cmd);
    uint32_t start = millis();

    while (millis() - start < timeout) {
        while (gsm.available()) {
            // Read character by character for better synchronization
            gsm_response += (char)gsm.read();
            
            if (gsm_response.indexOf(expect) >= 0) {
                if (debug) Serial.println(gsm_response);
                return true;
            }
        }
    }

    if (debug) Serial.println("TIMEOUT: " + gsm_response);
    return false;
}


// ===============================================================
// READ GPS
// ===============================================================
String getGPS() {
    // AT+QGPSLOC=2 is preferred for more fix details
    if (!sendAT("AT+QGPSLOC=2", "+QGPSLOC:", 2000, false)) {
        return "NO FIX";
    }
    // The response is already in gsm_response from sendAT
    return gsm_response;
}


// ===============================================================
// AUTO-DETECT APN BASED ON SIM OPERATOR
// ===============================================================
String detectAPN() {
    Serial.println("Detecting SIM operator...");
    
    // Get operator name
    if (sendAT("AT+COPS?", "+COPS:", 3000, false)) {
        String response = gsm_response;
        response.toUpperCase();
        
        // Indian Operators
        if (response.indexOf("AIRTEL") >= 0) {
            Serial.println("Detected: Airtel India");
            return "airtelgprs.com";
        }
        else if (response.indexOf("JIO") >= 0 || response.indexOf("RELIANCE") >= 0) {
            Serial.println("Detected: Jio India");
            return "jionet";
        }
        else if (response.indexOf("VODAFONE") >= 0 || response.indexOf("VI") >= 0 || response.indexOf("IDEA") >= 0) {
            Serial.println("Detected: Vi/Vodafone-Idea");
            return "www";
        }
        else if (response.indexOf("BSNL") >= 0) {
            Serial.println("Detected: BSNL India");
            return "bsnlnet";
        }
        // International Operators
        else if (response.indexOf("T-MOBILE") >= 0) {
            Serial.println("Detected: T-Mobile");
            return "fast.t-mobile.com";
        }
        else if (response.indexOf("AT&T") >= 0) {
            Serial.println("Detected: AT&T");
            return "phone";
        }
        else if (response.indexOf("VERIZON") >= 0) {
            Serial.println("Detected: Verizon");
            return "vzwinternet";
        }
        else if (response.indexOf("VODAFONE") >= 0) {
            Serial.println("Detected: Vodafone International");
            return "internet";
        }
        else if (response.indexOf("ORANGE") >= 0) {
            Serial.println("Detected: Orange");
            return "internet";
        }
    }
    
    // Fallback: Try to detect by IMSI (first 3 digits = MCC+MNC)
    if (sendAT("AT+CIMI", "OK", 3000, false)) {
        String imsi = gsm_response;
        imsi.trim();
        
        // India - 404/405
        if (imsi.startsWith("404") || imsi.startsWith("405")) {
            String mccmnc = imsi.substring(0, 5);
            
            // Airtel variations
            if (mccmnc == "40402" || mccmnc == "40403" || mccmnc == "40410" || 
                mccmnc == "40416" || mccmnc == "40431" || mccmnc == "40445" ||
                mccmnc == "40449" || mccmnc == "40470" || mccmnc == "40492" ||
                mccmnc == "40493" || mccmnc == "40494" || mccmnc == "40495" ||
                mccmnc == "40496" || mccmnc == "40497" || mccmnc == "40498") {
                Serial.println("Detected: Airtel (by IMSI)");
                return "airtelgprs.com";
            }
            // Jio
            else if (mccmnc == "40589" || mccmnc == "40590") {
                Serial.println("Detected: Jio (by IMSI)");
                return "jionet";
            }
            // Vi/Vodafone-Idea
            else if (mccmnc == "40401" || mccmnc == "40404" || mccmnc == "40405" ||
                     mccmnc == "40411" || mccmnc == "40413" || mccmnc == "40420" ||
                     mccmnc == "40427" || mccmnc == "40430" || mccmnc == "40446" ||
                     mccmnc == "40456" || mccmnc == "40486") {
                Serial.println("Detected: Vi/Vodafone-Idea (by IMSI)");
                return "www";
            }
            // BSNL
            else if (mccmnc == "40434" || mccmnc == "40438" || mccmnc == "40453" ||
                     mccmnc == "40454" || mccmnc == "40455" || mccmnc == "40457" ||
                     mccmnc == "40458" || mccmnc == "40459" || mccmnc == "40462" ||
                     mccmnc == "40464" || mccmnc == "40466" || mccmnc == "40471" ||
                     mccmnc == "40472" || mccmnc == "40474" || mccmnc == "40476" ||
                     mccmnc == "40480" || mccmnc == "40481") {
                Serial.println("Detected: BSNL (by IMSI)");
                return "bsnlnet";
            }
        }
    }
    
    // Ultimate fallback: Use from secrets.h or default
    Serial.println("Could not detect operator, using default APN");
    return APN_NAME;
}

// ===============================================================
// SEND GPS TO FIREBASE (Updated HTTP Protocol)
// ===============================================================
bool sendFirebase(String json) {
    Serial.println("Uploading to Firebase...");

    // 1. Send URL length (Expect CONNECT)
    String urlCmd = "AT+QHTTPURL=" + String(firebaseURL.length()) + ",60";
    if (!sendAT(urlCmd, "CONNECT", 3000)) return false;

    // Send URL string
    gsm.print(firebaseURL);
    // Wait for the URL to be processed (Expect OK)
    if (!sendAT("", "OK", 1000, false)) return false; 

    // 2. POST JSON (Expect CONNECT)
    String postCmd = "AT+QHTTPPOST=" + String(json.length()) + ",10,10";
    if (!sendAT(postCmd, "CONNECT", 3000)) return false;

    // Send JSON payload
    gsm.print(json);
    
    // 3. Wait for the final server response notification
    // We expect "+QHTTPPOST: 0,200" for success (0=no error, 200=HTTP OK)
    if (!sendAT("", "+QHTTPPOST: 0,200", 15000)) {
        Serial.println("ERROR: HTTP POST failed or non-200 status code.");
        return false;
    }

    Serial.println("Firebase upload complete!");
    return true;
}