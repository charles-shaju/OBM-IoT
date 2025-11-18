#include <Arduino.h>
// Define the built-in LED pin. On many ESP32-C3 boards, this is GPIO 2.
// Using the predefined LED_BUILTIN constant is generally safer.
#define LED_PIN 2 

void setup() {
  // Initialize the LED_PIN as an output. 
  // This checks the GPIO initialization capability.
  pinMode(LED_PIN, OUTPUT); 

  // Initialize the Serial port for debugging output.
  // This checks the basic functionality of the UART hardware.
  Serial.begin(115200);
  Serial.println("--- ESP32-C3 Blink Test Started ---");
  Serial.println("If the LED blinks, the module is working!");
}

void loop() {
  // 1. Turn the LED ON (Output HIGH voltage)
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED: ON");

  // 2. Wait for 500 milliseconds (0.5 second)
  // This checks the internal clock and delay function accuracy.
  delay(500);

  // 3. Turn the LED OFF (Output LOW voltage)
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED: OFF");

  // 4. Wait for 500 milliseconds
  delay(500);
}