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

/* ---------- MOTOR PINS ---------- */
#define ENA 5
#define IN1 4
#define IN2 16
#define ENB 14
#define IN3 12
#define IN4 13

/* ---------- LDR ---------- */
#define LDR_PIN A0
#define LDR_THRESHOLD 500
const unsigned long lockoutDuration = 10000; // 10s

/* ---------- STATE ---------- */
float xVal = 128.0;
float yVal = 128.0;

bool lockoutActive = false;
bool wasAboveThreshold = false;
unsigned long lockoutStartTime = 0;

char packetBuffer[64];

/* ---------- SETUP ---------- */
void setup() {
  Serial.begin(115200);
  delay(500);

  // Motor pins
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  // Set PWM frequency safe for Wi-Fi
  analogWriteFreq(1000); // 1kHz

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) { // 10s timeout
      Serial.println("\nWiFi connection timeout!");
      break;
    }
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nWiFi CONNECTED");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  udp.begin(joystickPort);
  Serial.print("UDP joystick port: "); Serial.println(joystickPort);

  scoreUdp.begin(0);
}

/* ---------- LOOP ---------- */
void loop() {
  handleJoystickUDP();
  handleLDR();
  yield(); // gives Wi-Fi background tasks time to run
}

/* ---------- UDP JOYSTICK ---------- */
void handleJoystickUDP() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 20) return; // 50Hz max
  lastCheck = millis();

  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
  if (len <= 0) return;
  packetBuffer[len] = '\0';

  char* comma = strchr(packetBuffer, ',');
  if (!comma) return;

  *comma = '\0';
  xVal = constrain(atof(packetBuffer), 0, 255);
  yVal = constrain(atof(comma + 1), 0, 255);

  driveDifferential(xVal, yVal);
}

/* ---------- DIFFERENTIAL DRIVE ---------- */
void driveDifferential(float x, float y) {
  // Normalize -1..1
  float turn  = (x - 128.0) / 128.0;
  float speed = (128.0 - y) / 128.0;

  // Deadzone to reduce jitter
  if (abs(turn) < 0.05) turn = 0;
  if (abs(speed) < 0.05) speed = 0;

  int left  = constrain((speed - turn) * 255, -255, 255);
  int right = constrain((speed + turn) * 255, -255, 255);

  setMotor(left, right);
}

/* ---------- MOTOR CONTROL ---------- */
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

/* ---------- LDR HANDLER ---------- */
void handleLDR() {
  unsigned long now = millis();

  // Lockout check
  if (lockoutActive) {
    if (now - lockoutStartTime >= lockoutDuration) {
      lockoutActive = false;
      wasAboveThreshold = false;
    } else {
      return; // skip if lockout active
    }
  }

  // Average 5 samples to reduce noise
  int ldrValue = 0;
  for (int i = 0; i < 5; i++) ldrValue += analogRead(LDR_PIN);
  ldrValue /= 5;

  if (ldrValue >= LDR_THRESHOLD && !wasAboveThreshold) {
    wasAboveThreshold = true;
    lockoutActive = true;
    lockoutStartTime = now;
    sendScoreUDP();
  }

  if (ldrValue < LDR_THRESHOLD) wasAboveThreshold = false;
}

/* ---------- SCORE UDP ---------- */
void sendScoreUDP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ WiFi not connected, retrying...");
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
