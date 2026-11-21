#include <SoftwareSerial.h>

// =====================================
// ESP32 UART COMMUNICATION
// =====================================

SoftwareSerial espSerial(10, 11);

// D10 = RX
// D11 = TX

#define UART_BAUD 9600

// =====================================
// OUTPUT PINS
// =====================================

#define BUZZER_PIN 7
#define LED_PIN 8

// =====================================
// WARNING TIMER
// =====================================

#define WARNING_DURATION 5000UL

bool warningActive = false;

unsigned long warningStartTime = 0;
unsigned long lastStatusTime = 0;

String incomingMessage = "";
String currentSound = "BACKGROUND";

// =====================================
// WARNING ON
// =====================================

void warningON(const char* reason) {

    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);

    warningActive = true;

    warningStartTime = millis();

    currentSound = reason;

    Serial.println();
    Serial.println("==============================");
    Serial.println("!!! SAFETY WARNING !!!");

    Serial.print("Detected Sound: ");
    Serial.println(reason);

    Serial.println("BUZZER: ON");
    Serial.println("LED: ON");

    Serial.println("AUTO OFF: 5 SECONDS");

    Serial.println("==============================");
}

// =====================================
// WARNING OFF
// =====================================

void warningOFF() {

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    warningActive = false;

    currentSound = "BACKGROUND";

    Serial.println();
    Serial.println("==============================");
    Serial.println("WARNING DEACTIVATED");

    Serial.println("BUZZER: OFF");
    Serial.println("LED: OFF");

    Serial.println("==============================");
}

// =====================================
// PROCESS ESP32 MESSAGE
// =====================================

void processMessage(String message) {

    message.trim();

    Serial.print("Received from ESP32: ");
    Serial.println(message);

    if (message == "ALARM") {

        warningON("ALARM");
    }

    else if (message == "IMPACT") {

        warningON("IMPACT");
    }

    else if (message == "BACKGROUND") {

        // Keep the warning active until
        // the 5-second timer finishes.

    }

    else if (message == "WARNING_OFF") {

        warningOFF();
    }

    else {

        Serial.println("Unknown command received.");
    }
}

// =====================================
// RECEIVE ESP32 UART DATA
// =====================================

void receiveESP32Data() {

    while (espSerial.available() > 0) {

        char receivedChar = espSerial.read();

        if (receivedChar == '\n') {

            incomingMessage.trim();

            if (incomingMessage.length() > 0) {

                processMessage(incomingMessage);
            }

            incomingMessage = "";
        }

        else if (receivedChar != '\r') {

            if (incomingMessage.length() < 30) {

                incomingMessage += receivedChar;
            }

            else {

                incomingMessage = "";
            }
        }
    }
}

// =====================================
// AUTOMATIC 5-SECOND SHUTDOWN
// =====================================

void checkWarningTimer() {

    if (warningActive &&
        millis() - warningStartTime >= WARNING_DURATION) {

        Serial.println();
        Serial.println("5 SECONDS COMPLETED");

        warningOFF();
    }
}

// =====================================
// SYSTEM STATUS
// =====================================

void displayStatus() {

    if (millis() - lastStatusTime >= 3000) {

        lastStatusTime = millis();

        Serial.println();
        Serial.println("------- SYSTEM STATUS -------");

        Serial.print("Sound: ");
        Serial.println(currentSound);

        Serial.print("Warning: ");
        Serial.println(warningActive ? "ACTIVE" : "NORMAL");

        Serial.print("Buzzer: ");
        Serial.println(warningActive ? "ON" : "OFF");

        Serial.print("LED: ");
        Serial.println(warningActive ? "ON" : "OFF");

        Serial.println("-----------------------------");
    }
}

// =====================================
// SETUP
// =====================================

void setup() {

    Serial.begin(9600);

    espSerial.begin(UART_BAUD);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    Serial.println();
    Serial.println("================================");
    Serial.println("INTELLIGENT SAFETY MONITORING");
    Serial.println("ESP32-S3 + ARDUINO UNO");
    Serial.println("MFCC + SVM SOUND CLASSIFICATION");
    Serial.println("================================");

    Serial.println("PIR SENSOR: REMOVED");

    Serial.println("BUZZER PIN: D7");
    Serial.println("LED PIN: D8");

    Serial.println("UART RX: D10");
    Serial.println("UART TX: D11");

    Serial.println("WARNING AUTO-OFF: 5 SECONDS");

    Serial.println();
    Serial.println("SYSTEM READY");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

    receiveESP32Data();

    checkWarningTimer();

    displayStatus();
}