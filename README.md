# OBM-IoT Firebase ESP32-C3

ESP32-C3 project that sends data to Firebase Realtime Database over Wi-Fi.

## Hardware
- ESP32-C3-DevKitM-1

## Setup Instructions

### 1. Get Firebase Credentials

1. Go to [Firebase Console](https://console.firebase.google.com/u/0/project/obm-gps)
2. Click **Project Settings** (gear icon) → **General** tab
3. Scroll down to **Your apps** section
4. Copy the **Web API Key**

### 2. Enable Authentication (if not already enabled)

1. In Firebase Console, go to **Authentication** → **Sign-in method**
2. Enable **Email/Password** provider
3. Go to **Users** tab → **Add user**
4. Add your email and password (or use existing: `padayatty90@gmail.com`)

### 3. Configure Database Rules

1. Go to **Realtime Database** → **Rules**
2. For testing, use:
```json
{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null"
  }
}
```

### 4. Update Code

Edit `src/main.cpp` and replace these values:

```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"           // Your Wi-Fi network name
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"   // Your Wi-Fi password
#define API_KEY "YOUR_FIREBASE_API_KEY"      // From Firebase Console
#define USER_PASSWORD "YOUR_USER_PASSWORD"   // Firebase Auth password
```

### 5. Build & Upload

```bash
# Install dependencies and build
pio run

# Upload to ESP32-C3
pio run --target upload

# Open serial monitor
pio device monitor
```

## What It Does

- Connects to Wi-Fi
- Authenticates with Firebase using email/password
- Sends incrementing counter values every 5 seconds to:
  - `/test/counter` - simple integer value
  - `/test/logs/{counter}` - JSON with value and timestamp

## View Data

Open Firebase Console:
https://console.firebase.google.com/u/0/project/obm-gps/database/obm-gps-default-rtdb/data

You should see data appearing under `/test/counter` and `/test/logs/`

## Troubleshooting

**"FAILED" errors:**
- Check Wi-Fi credentials
- Check Firebase API key
- Verify user email/password in Firebase Auth
- Check database rules allow authenticated writes

**Library not found:**
```bash
pio lib install
```

**Connection issues:**
- Verify Wi-Fi signal strength
- Check firewall settings
- Try 2.4GHz network (ESP32-C3 doesn't support 5GHz)
