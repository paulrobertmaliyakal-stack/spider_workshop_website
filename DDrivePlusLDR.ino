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
#define IN1 12
#define IN2 13

#define ENB 4
#define IN3 0
#define IN4 2

#define stdby 14

/* ---------- LDR ---------- */
#define LDR_PIN 1
#define LDR_THRESHOLD 500
const unsigned long lockoutDuration = 10000; // 10s

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

  // safer PWM for WiFi
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

  handleJoystickUDP();
   // ⭐ MOTOR SAFETY STOP
  handleLDR();

  yield(); // keep WiFi alive
}

/* =========================================================
                PACKET TIMEOUT SAFETY
========================================================= */


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
 


  char* comma = strchr(packetBuffer, ',');
  if (!comma) return;

  *comma = '\0';

  xVal = constrain(atof(packetBuffer), -150, 150);
  yVal = constrain(atof(comma + 1), -150, 150);

  driveDifferential(xVal, yVal);
}

/* =========================================================
                DIFFERENTIAL DRIVE
========================================================= */
void driveDifferential(float x, float y) {

  float turn  = (x ) / 150.0;
  float speed = ( y) / 150.0;

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
