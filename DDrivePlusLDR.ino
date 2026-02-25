#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

/* ---------- WIFI CONFIG ---------- */
const char* ssid = "ZirconA_115_2.4G";
const char* password = "zircon1964";
const unsigned int localPort = 1234;

// Server details
const char* serverIP = "192.168.1.41";  // Replace with your server IP
const int serverPort = 8000;
String serverURL = "http://" + String(serverIP) + ":" + String(serverPort) + "/api/esp/request";

// ESP ID - CHANGE THIS TO "ESP_1" or "ESP_2"
const char* espID = "ESP_1";  // Change to "ESP_2" for the second ESP

/* ---------- UDP ---------- */
WiFiUDP udp;
char packetBuffer[255];

/* ---------- MOTOR PINS ---------- */
#define ENA 5                 
#define IN1 4
#define IN2 0

#define ENB 14
#define IN3 12
#define IN4 13

// -------- LDR Section --------
#define LDR_PIN A0
#define LDR_THRESHOLD 1000   // adjust experimentally

unsigned long lockoutStartTime = 0;
const unsigned long lockoutDuration = 10000; // 10 seconds

bool lockoutActive = false;
bool wasAboveThreshold = false;

int laserCount = 0;

/* ---------- JOYSTICK ---------- */
float xVal = 128.0;
float yVal = 128.0;

unsigned long lastPacketTime = 0;
const unsigned long timeoutMs = 1000;

/* ---------- SETUP ---------- */
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  stopMotors();

  /* ---------- START SOFT AP ---------- */
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP ID: ");
  Serial.println(espID);
  Serial.print("Server URL: ");
  Serial.println(serverURL);
}

/* ---------- LOOP ---------- */
void loop() {
  int packetSize = udp.parsePacket();

  if (packetSize) {
    int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
    if (len > 0) packetBuffer[len] = '\0';

    lastPacketTime = millis();

    /* ---- RAW UDP PRINT (use only for debugging) ---- */ 
    // Serial.print("RAW UDP [");
    // Serial.print(len);
    // Serial.print(" bytes] -> ");
    // Serial.println(packetBuffer);

    // Serial.print("BYTES: ");
    // for (int i = 0; i < len; i++) {
    //   Serial.print(packetBuffer[i], DEC);
    //   Serial.print(" ");
    // }
    // Serial.println();

    /* ---- PARSE CSV FLOATS ---- */
    char* comma = strchr(packetBuffer, ',');
    if (comma) {
      *comma = '\0';
      xVal = atof(packetBuffer);
      yVal = atof(comma + 1);

      Serial.print("Parsed X: ");
      Serial.print(xVal);
      Serial.print(" | Y: ");
      Serial.println(yVal);

      driveDifferential(xVal, yVal);
    }
  }
  else {
    // if (millis() - lastPacketTime > timeoutMs) {
    //   Serial.println("NO UDP PACKETS RECEIVED");
    //   stopMotors();
    //   lastPacketTime = millis();
    // }
  }

  handleLDR();
}

/* ---------- DIFFERENTIAL DRIVE ---------- */
void driveDifferential(float x, float y) {
  // Center = (128,128)
  float turn = (x - 128.0) / 128.0;
  float speed = (128.0 - y) / 128.0;

  int leftSpeed  = constrain((speed - turn) * 255, -255, 255);
  int rightSpeed = constrain((speed + turn) * 255, -255, 255);

  setMotor(leftSpeed, rightSpeed);
}

/* ---------- MOTOR CONTROL ---------- */
void setMotor(int left, int right) {
  // LEFT MOTOR
  digitalWrite(IN1, left >= 0);
  digitalWrite(IN2, left < 0);
  analogWrite(ENA, abs(left));

  // RIGHT MOTOR
  digitalWrite(IN3, right >= 0);
  digitalWrite(IN4, right < 0);
  analogWrite(ENB, abs(right));
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

/* ---------- LDR CODE ---------- */
void handleLDR() {
  int ldrValue = analogRead(LDR_PIN);
  //Serial.println(ldrValue);

  unsigned long currentTime = millis();

  // If lockout is active, check if 10 seconds passed
  if (lockoutActive) {
    if (currentTime - lockoutStartTime >= lockoutDuration) {
      lockoutActive = false;
      wasAboveThreshold = false;   // re-arm detection
      Serial.println("LDR lockout ended");
    }
    return; // ignore LDR during lockout
  }

  // Detect rising edge (LOW → HIGH threshold crossing)
  if (ldrValue >= LDR_THRESHOLD && !wasAboveThreshold) {
    laserCount++;
    lockoutActive = true;
    lockoutStartTime = currentTime;
    wasAboveThreshold = true;

    Serial.print("Laser detected! Count = ");
    Serial.println(laserCount);

    sendRequest();
  }

  // Reset edge detector when signal goes LOW again
  if (ldrValue < LDR_THRESHOLD) {
    wasAboveThreshold = false;
  }
}

void sendRequest() {
  // if (WiFi.status() != WL_CONNECTED) {
  //     Serial.println("\nConnecting to WiFi...");
  //   WiFi.begin(ssid, password);
  
  //   while (WiFi.status() != WL_CONNECTED) {
  //     delay(500);
  //     Serial.print(".");
  //   }
  // }
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    
    // Create JSON payload
    StaticJsonDocument<200> jsonDoc;
    
    jsonDoc["esp_id"] = espID;
    
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    
    // Send POST request - ESP8266 requires WiFiClient as parameter
    http.begin(client, serverURL);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Response code: " + String(httpResponseCode));
      Serial.println("Response: " + response);
      
      // Parse response to see who got points
      StaticJsonDocument<512> responseDoc;
      DeserializationError error = deserializeJson(responseDoc, response);
      
      if (!error) {
        const char* pointsAwardedTo = responseDoc["points_awarded_to"];
        int points = responseDoc["points"];
        
        Serial.print("✓ Request successful! ");
        Serial.print(pointsAwardedTo);
        Serial.print(" got +");
        Serial.print(points);
        Serial.println(" points");
      }
    } else {
      Serial.print("✗ Error sending request: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("✗ WiFi not connected!");
  }
}

