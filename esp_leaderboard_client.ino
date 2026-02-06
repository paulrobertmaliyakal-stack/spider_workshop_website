#include <WiFi.h>  // For ESP32, use <ESP8266WiFi.h> for ESP8266
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Server details
const char* serverIP = "192.168.1.100";  // Replace with your server IP
const int serverPort = 8000;
String serverURL = "http://" + String(serverIP) + ":" + String(serverPort) + "/api/esp/request";

// ESP ID - CHANGE THIS TO "ESP_1" or "ESP_2"
const char* espID = "ESP_1";  // Change to "ESP_2" for the second ESP

// Button pin (optional - for manual triggering)
const int buttonPin = 0;  // GPIO 0 (boot button on most ESP boards)
bool lastButtonState = HIGH;

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  
  // Connect to WiFi
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

void loop() {
  // Check if button is pressed
  bool buttonState = digitalRead(buttonPin);
  
  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50);  // Debounce
    
    if (digitalRead(buttonPin) == LOW) {
      Serial.println("\nButton pressed! Sending request...");
      sendRequest();
      
      // Wait for button release
      while (digitalRead(buttonPin) == LOW) {
        delay(10);
      }
    }
  }
  
  lastButtonState = buttonState;
  
  // Optional: Send automatic requests every 10 seconds
  // Uncomment the lines below if you want automatic requests
  /*
  static unsigned long lastAutoRequest = 0;
  if (millis() - lastAutoRequest > 10000) {
    sendRequest();
    lastAutoRequest = millis();
  }
  */
}

void sendRequest() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Create JSON payload
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["esp_id"] = espID;
    
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    
    // Send POST request
    http.begin(serverURL);
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
