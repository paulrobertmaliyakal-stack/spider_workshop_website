#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/* ---------- WIFI CONFIG ---------- */
const char* ssid = "SAC-08(2.4GHz)";
const char* password = "sac@1964";

/* ---------- UDP CONFIG ---------- */
WiFiUDP udp;        // joystick UDP
WiFiUDP scoreUdp;   // score UDP

const unsigned int joystickPort = 1234;
const char* scoreServerIP = "192.168.1.169";
const unsigned int scoreServerPort = 9000;

/* ---------- ESP ID ---------- */
const char* espID = "ESP_1";

/* ---------- MOTOR PINS (NodeMCU 1.0) ---------- */
// TB6612 differential drive motor driver

#define ENA       D1    // D1  → GPIO5  → Motor A PWM (speed control)
#define IN1       D6    // D6  → GPIO12 → Motor A direction pin 1
#define IN2       D7    // D7  → GPIO13 → Motor A direction pin 2

#define ENB       D2    // D2  → GPIO4  → Motor B PWM (speed control)
#define IN3       D3    // D3  → GPIO0  → Motor B direction pin 1 (BOOT pin, must be HIGH at boot)
#define IN4       D4    // D4  → GPIO2  → Motor B direction pin 2 (BOOT pin, must be HIGH at boot)

#define stdby     D5    // D5  → GPIO14 → TB6612 STBY (enable pin)

/* ---------- LDR ---------- */
#define LDR_PIN   A0    // A0  → ADC0   → LDR analog input (0–1023)

/* ---------- MODE + STATUS LED ---------- */
#define MODE_PIN  D0    // D0  → GPIO16 → Mode select (INPUT_PULLUP, GND = LDR test mode)
#define LED       D8    // D8  → GPIO15 → Status LED (LOW at boot, safe output)

/* ---------- LDR SETTINGS ---------- */
#define LDR_THRESHOLD 1000
const unsigned long lockoutDuration = 10000; // 10s
const unsigned long freezeDuration = 5000;   // 5s

/* ---------- CONTROL STATE ---------- */
float xVal = 128.0;
float yVal = 128.0;

unsigned long lastPacketTime = 0;
const unsigned long packetTimeout = 500; // STOP after 0.5s silence

bool motorsStopped = true;

/* ---------- LDR STATE ---------- */
bool lockoutActive = false;
bool wasAboveThreshold = false;
unsigned long lockoutStartTime = 0;

char packetBuffer[64];

/* =========================================================
                        SETUP
========================================================= */
void setup() {

  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== ESP START ===");

  /* Motor Pins */
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(stdby, OUTPUT);
  digitalWrite(stdby, HIGH);
  stopMotors();

  /* LDR Pins (+ extra testing pins)*/
  pinMode(LDR_PIN, INPUT);

  pinMode(MODE_PIN, INPUT_PULLUP);   
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  analogWriteFreq(1000);

  /* WiFi */
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) {
      Serial.println("\nWiFi timeout!");
      break;
    }
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWiFi CONNECTED");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  udp.begin(joystickPort);
  Serial.print("Joystick UDP Port: ");
  Serial.println(joystickPort);

  scoreUdp.begin(0);
}

/* =========================================================
                        LOOP
========================================================= */

void loop() {

  bool freezeBot = false;

  // Detect freeze window (first 5s of lockout)
  if (lockoutActive) {
    if (millis() - lockoutStartTime < freezeDuration) {
      freezeBot = true;
    }
  }

  // ===== MODE PRIORITY =====
  if (digitalRead(MODE_PIN) == LOW) {
    // LDR TEST MODE (highest priority)
    handleLDRTestMode();
  }
  else if (freezeBot) {
    // FREEZE MODE
    stopMotors();
    motorsStopped = true;

    digitalWrite(LED, HIGH); // LED ON during freeze
  }
  else {
    // NORMAL MODE
    digitalWrite(LED, LOW);  // LED OFF

    handleJoystickUDP();
    handlePacketTimeout();
  }

  // LDR must always run to manage lockout timing
  handleLDR();

  yield(); // keep WiFi alive
}

/* =========================================================
                PACKET TIMEOUT SAFETY
========================================================= */
void handlePacketTimeout() {

  if (millis() - lastPacketTime > packetTimeout) {

    if (!motorsStopped) {
      Serial.println("⚠ No packets -> STOP motors");
      stopMotors();
      motorsStopped = true;
    }
  }
}

/* =========================================================
                    UDP JOYSTICK
========================================================= */
void handleJoystickUDP() {

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 20) return; // 50Hz
  lastCheck = millis();

  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
  if (len <= 0) return;

  packetBuffer[len] = '\0';
  lastPacketTime = millis();
  motorsStopped = false;

  char* comma = strchr(packetBuffer, ',');
  if (!comma) return;

  *comma = '\0';

  xVal = constrain(atof(packetBuffer), 0, 255);
  yVal = constrain(atof(comma + 1), 0, 255);

  driveDifferential(xVal, yVal);
}

/* =========================================================
                DIFFERENTIAL DRIVE
========================================================= */
void driveDifferential(float x, float y) {

  float turn  = (x - 128.0) / 128.0;
  float speed = (128.0 - y) / 128.0;

  // deadzone
  if (abs(turn) < 0.05) turn = 0;
  if (abs(speed) < 0.05) speed = 0;

  int left  = constrain((speed - turn) * 100, -100, 100);
  int right = constrain((speed + turn) * 100, -100, 100);

  Serial.print("L:");
  Serial.print(left);
  Serial.print("  R:");
  Serial.println(right);

  setMotor(left, right);
}

/* =========================================================
                    MOTOR CONTROL
========================================================= */
void setMotor(int left, int right) {

  digitalWrite(IN1, left >= 0);
  digitalWrite(IN2, left < 0);
  analogWrite(ENA, abs(left));

  digitalWrite(IN3, right >= 0);
  digitalWrite(IN4, right < 0);
  analogWrite(ENB, abs(right));
}

void stopMotors() {

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

/* =========================================================
                    LDR HANDLER
========================================================= */
void handleLDR() {

  unsigned long now = millis();

  if (lockoutActive) {
    if (now - lockoutStartTime >= lockoutDuration) {
      lockoutActive = false;
      wasAboveThreshold = false;
    } else {
      return;
    }
  }

  int ldrValue = 0;
  for (int i = 0; i < 5; i++)
    ldrValue += analogRead(LDR_PIN);

  ldrValue /= 5;
  Serial.println(ldrValue);

  if (ldrValue >= LDR_THRESHOLD && !wasAboveThreshold) {
    wasAboveThreshold = true;
    lockoutActive = true;
    lockoutStartTime = now;
    sendScoreUDP();
  }
  
  if (ldrValue < LDR_THRESHOLD)
    wasAboveThreshold = false;
}

/* =========================================================
                    SCORE UDP
========================================================= */
void sendScoreUDP() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ WiFi lost — reconnecting");
    WiFi.reconnect();
    return;
  }

  char msg[32];
  snprintf(msg, sizeof(msg), "SCORE,%s", espID);

  scoreUdp.beginPacket(scoreServerIP, scoreServerPort);
  scoreUdp.write((uint8_t*)msg, strlen(msg));
  scoreUdp.endPacket();

  Serial.println("✓ SCORE SENT");
}

/* =========================================================
                LDR TEST MODE
========================================================= */
void handleLDRTestMode() {

  // Ensure bot is fully inactive
  stopMotors();
  motorsStopped = true;

  // Block UDP traffic silently
  udp.flush();
  scoreUdp.flush();

  // Read LDR (averaged)
  int ldrValue = 0;
  for (int i = 0; i < 5; i++)
    ldrValue += analogRead(LDR_PIN);

  ldrValue /= 5;

  Serial.print("[TEST MODE] LDR = ");
  Serial.println(ldrValue);

  // LED indication
  if (ldrValue >= LDR_THRESHOLD)
    digitalWrite(LED, HIGH);
  else
    digitalWrite(LED, LOW);

  delay(50); // debounce
}
