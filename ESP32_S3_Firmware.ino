
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <math.h>

#include "mfcc.h"
#include "svm_classifier.h"

// =====================================
// WIFI CONFIGURATION
// =====================================

const char* WIFI_SSID = "Safety_Project";
const char* WIFI_PASSWORD = "12345678";

WiFiServer server(5000);
WiFiClient client;

// =====================================
// UART CONFIGURATION
// =====================================

#define UART_BAUD 9600
#define ESP_RX_PIN 44
#define ESP_TX_PIN 43

// =====================================
// WARNING AND LOCKOUT
// =====================================

#define WARNING_DURATION 5000UL

bool warningActive = false;
bool audioLockoutActive = false;
bool audioResumePending = false;

unsigned long warningStartTime = 0;

// =====================================
// AUDIO CONFIGURATION
// =====================================

#define AUDIO_BYTES 32000
#define AUDIO_SAMPLES 16000

uint8_t audioBuffer[AUDIO_BYTES];
float audioSamples[AUDIO_SAMPLES];
float features[N_FEATURES];

const char* soundLabels[N_CLASSES] = {
    "ALARM",
    "BACKGROUND",
    "IMPACT"
};

// =====================================
// TEMPORAL VOTING
// =====================================

#define HISTORY_SIZE 3
#define REQUIRED_VOTES 2

int predictionHistory[HISTORY_SIZE] = {1, 1, 1};

int historyIndex = 0;
int historyCount = 0;

int lastSentDecision = -1;

// =====================================
// FUNCTION DECLARATIONS
// =====================================

void updateWarningTimer();
void activateWarning(const char* reason);
void resetHistory();
void sendDecisionToArduino(int decision);
void serviceSystem();
bool receiveAudioWindow();
void discardAvailableAudio();
void finishAudioLockout();

// =====================================
// SYSTEM SERVICE
// =====================================

void serviceSystem() {
    updateWarningTimer();
}

// =====================================
// ACTIVATE WARNING AND LOCKOUT
// =====================================

void activateWarning(const char* reason) {

    warningStartTime = millis();

    warningActive = true;
    audioLockoutActive = true;
    audioResumePending = false;

    Serial.println();
    Serial.println("==============================");
    Serial.println("SAFETY WARNING ACTIVATED");

    Serial.print("Reason: ");
    Serial.println(reason);

    Serial.println("BUZZER: ON");
    Serial.println("LED: ON");
    Serial.println("AUDIO CLASSIFICATION PAUSED");
    Serial.println("AUDIO LOCKOUT: 5 SECONDS");

    Serial.println("==============================");
}

// =====================================
// AUTOMATIC WARNING OFF
// =====================================

void updateWarningTimer() {

    if (warningActive &&
        millis() - warningStartTime >= WARNING_DURATION) {

        Serial1.println("WARNING_OFF");

        warningActive = false;

        audioLockoutActive = false;
        audioResumePending = true;

        Serial.println();
        Serial.println("==============================");
        Serial.println("5 SECOND WARNING COMPLETED");
        Serial.println("BUZZER: OFF");
        Serial.println("LED: OFF");
        Serial.println("PREPARING AUDIO RESUMPTION...");
        Serial.println("==============================");
    }
}

// =====================================
// RESET TEMPORAL HISTORY
// =====================================

void resetHistory() {

    historyIndex = 0;
    historyCount = 0;

    lastSentDecision = -1;

    for (int i = 0; i < HISTORY_SIZE; i++) {
        predictionHistory[i] = 1;
    }
}

// =====================================
// RESET CONNECTION STATE
// =====================================

void resetConnectionState() {

    resetHistory();

    warningActive = false;
    audioLockoutActive = false;
    audioResumePending = false;
}

// =====================================
// SEND DECISION TO ARDUINO
// =====================================

void sendDecisionToArduino(int decision) {

    if (decision < 0 || decision >= N_CLASSES) {
        return;
    }

    if (decision != lastSentDecision) {

        Serial1.println(soundLabels[decision]);

        Serial.print("UART SOUND MESSAGE SENT: ");
        Serial.println(soundLabels[decision]);

        lastSentDecision = decision;

        if (decision == 0) {
            activateWarning("SOUND ALARM");
        }

        else if (decision == 2) {
            activateWarning("SOUND IMPACT");
        }

        else if (decision == 1) {
            Serial.println("BACKGROUND SOUND");
            // Do not cancel an active warning.
        }
    }
}

// =====================================
// WIFI CONNECTION
// =====================================

void connectWiFi() {

    Serial.println("Connecting to phone hotspot...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {

        serviceSystem();

        delay(100);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi connected!");

    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    server.begin();

    Serial.println("TCP server started on port 5000");
}

// =====================================
// RECEIVE ONE AUDIO WINDOW
// =====================================

bool receiveAudioWindow() {

    size_t totalBytes = 0;

    while (totalBytes < AUDIO_BYTES) {

        serviceSystem();

        if (!client.connected()) {

            Serial.println("Phone disconnected.");
            return false;
        }

        int availableBytes = client.available();

        if (availableBytes > 0) {

            size_t remaining = AUDIO_BYTES - totalBytes;

            size_t bytesToRead =
                (availableBytes < (int)remaining)
                ? availableBytes
                : remaining;

            int received = client.read(
                audioBuffer + totalBytes,
                bytesToRead
            );

            if (received > 0) {
                totalBytes += received;
            }
        }
        else {
            delay(1);
        }
    }

    return true;
}

// =====================================
// DISCARD AUDIO DURING LOCKOUT
// =====================================

void discardAvailableAudio() {

    if (!client || !client.connected()) {
        return;
    }

    uint8_t discardBuffer[512];

    while (client.available() >= 2) {

        int availableBytes = client.available();

        int bytesToRead = availableBytes;

        if (bytesToRead > (int)sizeof(discardBuffer)) {
            bytesToRead = sizeof(discardBuffer);
        }

        // Preserve PCM16 byte alignment
        if (bytesToRead % 2 != 0) {
            bytesToRead--;
        }

        int received = client.read(
            discardBuffer,
            bytesToRead
        );

        if (received <= 0) {
            break;
        }
    }
}

// =====================================
// FINISH AUDIO LOCKOUT
// =====================================

void finishAudioLockout() {

    Serial.println();
    Serial.println("Flushing remaining microphone audio...");

    unsigned long flushStart = millis();

    // Discard buffered audio before resuming
    while (millis() - flushStart < 100) {

        if (!client.connected()) {
            break;
        }

        discardAvailableAudio();

        serviceSystem();

        delay(1);
    }

    resetHistory();

    audioResumePending = false;

    Serial.println();
    Serial.println("==============================");
    Serial.println("AUDIO MONITORING RESUMED");
    Serial.println("MFCC: READY");
    Serial.println("SVM: READY");
    Serial.println("TEMPORAL VOTING: RESET");
    Serial.println("==============================");
}

// =====================================
// PCM16 CONVERSION
// =====================================

void convertAudio() {

    for (int i = 0; i < AUDIO_SAMPLES; i++) {

        uint16_t raw =
            (uint16_t)audioBuffer[2 * i] |
            ((uint16_t)audioBuffer[2 * i + 1] << 8);

        int16_t sample = (int16_t)raw;

        audioSamples[i] =
            (float)sample / 32768.0f;
    }
}

// =====================================
// RMS CALCULATION
// =====================================

float calculateRMS() {

    double sum = 0.0;

    for (int i = 0; i < AUDIO_SAMPLES; i++) {

        sum +=
            (double)audioSamples[i] *
            audioSamples[i];
    }

    return sqrt(sum / AUDIO_SAMPLES);
}

// =====================================
// ADD PREDICTION TO HISTORY
// =====================================

void addPrediction(int prediction) {

    predictionHistory[historyIndex] = prediction;

    historyIndex =
        (historyIndex + 1) % HISTORY_SIZE;

    if (historyCount < HISTORY_SIZE) {
        historyCount++;
    }
}

// =====================================
// TEMPORAL VOTING
// =====================================

void applyTemporalVoting() {

    int votes[N_CLASSES] = {0};

    for (int i = 0; i < historyCount; i++) {

        int prediction = predictionHistory[i];

        if (prediction >= 0 && prediction < N_CLASSES) {
            votes[prediction]++;
        }
    }

    Serial.println();
    Serial.println("TEMPORAL VOTING");

    Serial.printf("ALARM votes: %d\n", votes[0]);
    Serial.printf("BACKGROUND votes: %d\n", votes[1]);
    Serial.printf("IMPACT votes: %d\n", votes[2]);

    if (historyCount < HISTORY_SIZE) {

        Serial.println("Decision: COLLECTING PREDICTIONS");
        return;
    }

    int finalDecision;

    if (votes[0] >= REQUIRED_VOTES) {
        finalDecision = 0;
    }
    else if (votes[2] >= REQUIRED_VOTES) {
        finalDecision = 2;
    }
    else {
        finalDecision = 1;
    }

    Serial.print("FINAL DECISION: ");
    Serial.println(soundLabels[finalDecision]);

    sendDecisionToArduino(finalDecision);
}

// =====================================
// CLASSIFY AUDIO
// =====================================

void classifyAudio() {

    convertAudio();

    serviceSystem();

    float rms = calculateRMS();

    serviceSystem();

    Serial.println();
    Serial.println("------------------------------");

    Serial.printf("Audio RMS: %.6f\n", rms);

    int prediction;

    if (rms < 0.005f) {

        prediction = 1;

        Serial.println("SVM prediction: BACKGROUND (silence)");
    }

    else {

        Serial.println("Extracting MFCC features...");

        bool success = extractMFCCFeatures(
            audioSamples,
            features
        );

        serviceSystem();

        if (!success) {

            Serial.println("MFCC extraction failed!");
            return;
        }

        Serial.println("Running SVM classification...");

        prediction = predictSoundClass(features);

        serviceSystem();

        if (prediction < 0 || prediction >= N_CLASSES) {

            Serial.println("Invalid prediction!");
            return;
        }

        Serial.print("SVM prediction: ");
        Serial.println(soundLabels[prediction]);
    }

    serviceSystem();

    addPrediction(prediction);

    applyTemporalVoting();

    serviceSystem();

    Serial.println("------------------------------");
}

// =====================================
// SETUP
// =====================================

void setup() {

    Serial.begin(115200);

    Serial1.begin(
        UART_BAUD,
        SERIAL_8N1,
        ESP_RX_PIN,
        ESP_TX_PIN
    );

    delay(1500);

    Serial.println();
    Serial.println("================================");
    Serial.println("ESP32-S3 INTELLIGENT SAFETY SYSTEM");
    Serial.println("MFCC + SVM + TEMPORAL VOTING");
    Serial.println("PIR REMOVED");
    Serial.println("BUZZER + LED WARNING SYSTEM");
    Serial.println("5 SECOND WARNING TIMER");
    Serial.println("5 SECOND AUDIO LOCKOUT");
    Serial.println("================================");

    Serial.println("UART communication initialized.");
    Serial.println("ESP32 RX: GPIO44");
    Serial.println("ESP32 TX: GPIO43");

    connectWiFi();
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

    serviceSystem();

    // AUDIO LOCKOUT MODE
    if (audioLockoutActive) {

        // Read and discard microphone audio.
        // No RMS, MFCC or SVM classification.

        discardAvailableAudio();

        delay(1);
        return;
    }

    // RESUME AUDIO AFTER LOCKOUT
    if (audioResumePending) {

        finishAudioLockout();
        return;
    }

    // PHONE CONNECTION
    if (!client || !client.connected()) {

        if (client) {
            client.stop();
        }

        client = server.available();

        if (client) {

            Serial.println();
            Serial.println("Phone connected!");

            resetConnectionState();
        }

        delay(10);
        return;
    }

    // NORMAL AUDIO CLASSIFICATION
    if (receiveAudioWindow()) {

        Serial.println("Received 32000 audio bytes.");

        classifyAudio();
    }

    else {

        client.stop();

        resetConnectionState();

        Serial.println("Waiting for phone connection...");
    }
}
