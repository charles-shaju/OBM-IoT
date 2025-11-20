// =======================================
// EC200U + ESP32-C3 + Firebase GPS Uploader
// Optimized for Reliability & Correct JSON
// =======================================

#define RX_PIN 4
#define TX_PIN 5

HardwareSerial gsm(1);
String gsm_response = "";

// Use the explicit numeric JSON type in the URL
String firebaseURL = "Your database URL";

unsigned long lastSend = 0;

// FUNCTIONS
bool sendAT(String cmd, String expect, uint16_t timeout, bool debug = true);
bool sendFirebase(String json);
String getGPS();

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

    // Setup APN (Airtel) - Setting mode 1 = PAP/CHAP auto-selected
    if (!sendAT("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1", "OK", 5000)) {
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
        // EC200U Format: +QGPSLOC: <time>,<lon>,<lat>,<alt>,...
        // We use split() method since String.indexOf is error-prone when fields are missing
        
        // Remove prefix and trim
        gps_raw.replace("+QGPSLOC: ", ""); 
        gps_raw.trim();
        
        // Find indices
        int i1 = gps_raw.indexOf(','); // after Time
        int i2 = gps_raw.indexOf(',', i1 + 1); // after LON
        int i3 = gps_raw.indexOf(',', i2 + 1); // after LAT

        if (i3 == -1) {
            Serial.println("ERROR: Incomplete GPS data string.");
            return;
        }

        String longitude = gps_raw.substring(i1 + 1, i2);
        String latitude  = gps_raw.substring(i2 + 1, i3);
        
        // 3. Build Firebase JSON (Lat/Lon as numbers, not strings)
        String json = "{";
        json += "\"latitude\":" + latitude + ","; // NO QUOTES HERE
        json += "\"longitude\":" + longitude + ","; // NO QUOTES HERE
        // RECOMMENDED: Use Firebase Server Timestamp
        json += "\"timestamp\":{\".sv\":\"timestamp\"}";
        json += "}";

        // 4. Send to Firebase
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
