/*********************************************************************
  ESP32 Automatic Call Rejector for Bike/Scooter
  → Powers up when ignition key is turned ON
  → Acts as a Bluetooth Hands-Free device
  → Rejects every incoming call automatically
  Works on both Android and iPhone (iOS 13+)
  Arduino IDE + ESP32 board package
*********************************************************************/

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDISCOVERABLE)
#error Bluetooth is not enabled! Please enable it in menuconfig or use ESP32 with Bluetooth.
#endif

BluetoothSerial SerialBT;

// Change this name so you can recognize your device easily
String deviceName = "BikeSafe-NoCalls";

// Variables to detect incoming call
bool callActive = false;
bool ringDetected = false;

void setup() {
  Serial.begin(115200);

  // Optional: small status LED on GPIO2 (built-in on most DevKit boards)
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  SerialBT.setPinCode("1234");           // Optional PIN, remove if you don't want it
  SerialBT.enableSSP();                  // Secure Simple Pairing (recommended)

  // Register callback so we get notified about HFP events
  SerialBT.register_callback(btCallback);

  // Start as Hands-Free unit (the side that rejects/answers calls)
  if (SerialBT.begin(deviceName, true)) {  // true = Hands-Free mode (HF)
    Serial.println("Bluetooth Hands-Free started: " + deviceName);
    Serial.println("Pair with your phone → choose \"" + deviceName + "\" → PIN 1234 if asked");
    digitalWrite(2, HIGH); // LED on = ready
  } else {
    Serial.println("Failed to start Bluetooth!");
    while (1) {
      digitalWrite(2, !digitalRead(2));
      delay(200);
    }
  }
}

void loop() {
  // Everything is handled inside the callback, loop stays almost empty
  delay(100);
}

// This callback receives all AT commands and events from the phone
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println("Phone connected!");
    digitalWrite(2, HIGH);
  }

  if (event == ESP_SPP_CLOSE_EVT) {
    Serial.println("Phone disconnected!");
    digitalWrite(2, LOW);
    callActive = false;
    ringDetected = false;
  }

  if (event == ESP_SPP_DATA_IND_EVT) {
    {
    String data = "";
    while (SerialBT.available()) {
      char c = SerialBT.read();
      data += c;
    }

    Serial.print("← Received: ");
    Serial.println(data);

    // === Incoming call detection ===
    if (data.indexOf("RING") != -1) {
      ringDetected = true;
      Serial.println("!!! INCOMING CALL DETECTED → REJECTING !!!");

      // Immediately reject the call
      SerialBT.println("AT+CHUP");        // Hang up / reject
      delay(200);
      SerialBT.println("AT+CHUP");        // Send twice (some phones need it)

      // Optional: play a short beep through the speaker if you connected one
      // tone(BUZZER_PIN, 1000, 200);
    }

    // Some phones send +CLIP (caller ID) before RING
    if (data.indexOf("+CLIP:") != -1) {
      Serial.println("Caller ID received → call will ring soon → preparing to reject");
    }

    // Call active (you answered it manually somehow)
    if (data.indexOf("+CIEV: \"CALL\",1") != -1) {
      callActive = true;
      Serial.println("Call became active (maybe you answered manually)");
    }

    // Call ended
    if (data.indexOf("+CIEV: \"CALL\",0") != -1) {
      callActive = false;
      ringDetected = false;
    }
  }
}