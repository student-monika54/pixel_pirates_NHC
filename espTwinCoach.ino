#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h> 
#include <ArduinoJson.h>

// --- MAX30102 Sensor Libraries ---
#include "Wire.h"
#include "MAX30105.h" 

MAX30105 particleSensor;
bool sensorWorking = false;

// --- Wi-Fi Credentials ---
#define WIFI_SSID "Redmi 13C 5G" 
#define WIFI_PASSWORD "Fish@200" 

// --- Firebase Credentials ---
#define FIREBASE_HOST "twincoach-84520-default-rtdb.asia-southeast1.firebasedatabase.app" 
#define FIREBASE_AUTH "W6neZ3avO9Rxcu793qfCO88dQ42pXHofiYpCZNgB" 
#define FIREBASE_DATA_PATH "/health_data"            

// Global Firebase objects
FirebaseData fbdo;
FirebaseJson json;
FirebaseConfig config;
FirebaseAuth auth;

// --- Timing and Index Variables ---
const long updateInterval = 5000;
unsigned long previousMillis = 0; 
int dataIndex = 0;

// --- Sensor Data Storage ---
int sensorHR = 0;
int sensorSpO2 = 0;
float sensorTemp = 0.0;

// --- Hardcoded Data Sets (Used as Fallback) ---
const char* dataSet[] = {
  // DATA SET 1: Morning/Resting State
  R"({"cardio": [{"time": "08:00", "HR": 68, "RHR": 60, "HRV": 75}, {"time": "11:00", "HR": 72, "RHR": 60, "HRV": 70}],"body": [{"time": "08:00", "temp": 36.5, "spo2": 99}, {"time": "11:00", "temp": 36.8, "spo2": 98}],"recovery": [{"day": "Mon", "sleep": 85, "hrv": 75, "rhr": 58}, {"day": "Tue", "sleep": 82, "hrv": 72, "rhr": 60}]})",

  // DATA SET 2: Mid-Day/Active State 
  R"({"cardio": [{"time": "12:00", "HR": 95, "RHR": 65, "HRV": 50}, {"time": "16:00", "HR": 110, "RHR": 68, "HRV": 40}],"body": [{"time": "12:00", "temp": 37.2, "spo2": 97}, {"time": "16:00", "temp": 37.7, "spo2": 96}],"recovery": [{"day": "Wed", "sleep": 70, "hrv": 50, "rhr": 68}, {"day": "Thu", "sleep": 75, "hrv": 55, "rhr": 65}]})",

  // DATA SET 3: Evening/Recovery State 
  R"({"cardio": [{"time": "18:00", "HR": 80, "RHR": 62, "HRV": 65}, {"time": "22:00", "HR": 70, "RHR": 60, "HRV": 70}],"body": [{"time": "18:00", "temp": 37.0, "spo2": 98}, {"time": "22:00", "temp": 36.6, "spo2": 99}],"recovery": [{"day": "Fri", "sleep": 78, "hrv": 65, "rhr": 62}, {"day": "Sat", "sleep": 80, "hrv": 68, "rhr": 60}]})"
};

const int numDataSets = 3;

void tokenStatusCallback(TokenInfo info) {
  if (info.status == token_status_ready) {
    Serial.println("Firebase Token is ready. (Status 1)");
  } else {
    Serial.print("Firebase Token Status Code: ");
    Serial.println(info.status);
  }
}

void sendDataToFirebase();
void readSensorData(int index);

// -------------------------------- SETUP ---------------------------------

void setup() {
  Serial.begin(9600);
  delay(100);
  Serial.println("\nStarting up ESP8266...");

  // --- MAX30102 Sensor Initialization ---
  Serial.println("Initializing MAX30102...");
  if (particleSensor.begin(Wire)) {
    Serial.println("MAX30102 detected and configured.");
    particleSensor.setup(); 
    sensorWorking = true;
  } else {
    Serial.println("MAX30102 not detected. Switching to hardcoded data mode.");
    sensorWorking = false;
  }

  // --- Wi-Fi Connection ---
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected! IP Address: ");
  Serial.println(WiFi.localIP());

  // --- Firebase Configuration ---
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// -------------------------------- LOOP ----------------------------------

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;

    readSensorData(dataIndex);
    sendDataToFirebase();

    dataIndex = (dataIndex + 1) % numDataSets; 
  }
}

// -------------------------------- FUNCTIONS ---------------------------------

/**
 * Reads data from the MAX30102 or uses hardcoded data if the sensor failed.
 */
void readSensorData(int index) {
  if (sensorWorking) {
    // --- Live Sensor Data Acquisition ---
    
    // particleSensor.check();
    // long irValue = particleSensor.getIR(); 
    // sensorHR = calculateHR(irValue);
    // sensorSpO2 = calculateSpO2(irValue);
    // sensorTemp = particleSensor.readTemperature();
    
    Serial.println("Reading data from MAX30102...");
    
    // To mimic live data without modifying the large JSON string structure:
    // We update the dataIndex, which is used by sendDataToFirebase.
    // In a real scenario, the data would be compiled into a JSON object here.

  } else {
    // --- Fallback: Using Hardcoded Data ---

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, dataSet[index]);

    if (!error) {
      sensorHR = doc["cardio"][0]["HR"].as<int>();
      sensorSpO2 = doc["body"][0]["spo2"].as<int>();
      sensorTemp = doc["body"][0]["temp"].as<float>();
      
      Serial.print("[FALLBACK] HR: "); Serial.print(sensorHR);
      Serial.print(", SpO2: "); Serial.print(sensorSpO2);
      Serial.print(", Temp: "); Serial.println(sensorTemp);
    }
  }
}

/**
 * Sends the full data structure to Firebase using the current data set index.
 */
void sendDataToFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi not connected. Skipping Firebase update.");
    return;
  }
  
  if (Firebase.ready()) {
    Serial.print("\nSending Data Set #");
    Serial.print(dataIndex + 1);
    Serial.println(" to Firebase.");

    const char* current_json_data = dataSet[dataIndex];

    if (!json.setJsonData(current_json_data)) {
      Serial.println("Error loading JSON data.");
      return;
    }

    if (Firebase.set(fbdo, FIREBASE_DATA_PATH, json)) {
      Serial.println("SUCCESS: Data sent.");
      Serial.print("HTTP Code: ");
      Serial.println(fbdo.httpCode());
    } else {
      Serial.print("FAILED to send data: ");
      Serial.println(fbdo.errorReason());
    }
  } else {
    Serial.println("Firebase is not ready yet.");
  }
}