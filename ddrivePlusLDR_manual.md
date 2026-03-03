# ESP8266 Laser Bot - progress this far on the code

> **Important Note (Read First)**
> ⚠️ **The pinout described below matches the *current firmware*, NOT the PCB shown.**
> Before final deployment, **the pin definitions in the code MUST be updated to match the actual PCB routing**.
> Until then, this document explains how the code behaves and how to test it safely on a breadboard.
> Also note that the code does not include any part of the udp handling **for the laser**, so that needs to be added next.

---

## 1. Overview (What This Bot Does)

This bot is an **ESP8266 (NodeMCU 1.0) based differential-drive robot** controlled over **Wi‑Fi (UDP joystick input)**.

Additional features:

* Laser / LDR hit detection
* Lockout system after a hit
* Temporary freeze on hit
* Score reporting via UDP

The firmware is intentionally **simple and non-blocking** to keep Wi‑Fi stable, ensure that code remains non-blocking when adding new functions to the code.

---

## 2. Operating Modes (Code Behavior)

The bot operates in **three logical modes**, selected automatically in software.

### 2.1 Normal Mode (Default)

* Bot listens for **UDP joystick packets**
* Differential drive based on X/Y values
* Motors stop automatically if packets stop arriving (failsafe)
* LDR is monitored in the background

### 2.2 Freeze Mode (After Hit)

Triggered when the LDR crosses the threshold.

* Total lockout duration: **10 seconds**
* **First 5 seconds:**

  * Bot is completely frozen
  * Motors are forced OFF
* **Next 5 seconds:**

  * Bot moves normally again
  * Cannot be hit again during this time

This prevents repeated scoring and gives a recovery window.

### 2.3 LDR Test Mode (Breadboard Only)

Activated by grounding the **MODE_PIN**.

* Motors are disabled
* UDP traffic is ignored
* LED shows LDR state relative to threshold

⚠️ **MODE_PIN and LED are NOT present on the PCB**
They are **temporary breadboard-only test features**.

---

## 3. Pinout (Matches Code, NOT PCB)

> ⚠️ **This pinout MUST be changed later to match the PCB routing**

### 3.1 Motor Driver (TB6612)

```cpp
#define ENA   D1   // GPIO5  → Motor A PWM
#define IN1   D6   // GPIO12 → Motor A direction 1
#define IN2   D7   // GPIO13 → Motor A direction 2

#define ENB   D2   // GPIO4  → Motor B PWM
#define IN3   D3   // GPIO0  → Motor B direction 1 (BOOT pin)
#define IN4   D4   // GPIO2  → Motor B direction 2 (BOOT pin)

#define stdby D5   // GPIO14 → TB6612 STBY (enable)
```

### 3.2 LDR (Hit Detection)

```cpp
#define LDR_PIN A0   // ADC0 → LDR voltage divider input
```

* Threshold set in software
* Averaged over multiple samples

---

### 3.3 Test / Debug Pins (Breadboard Only)

```cpp
#define MODE_PIN D0  // GPIO16 → Test mode select (INPUT_PULLUP)
#define LED      D8  // GPIO15 → Status LED
```

**These pins do NOT exist on the PCB.**
They are used only for:

* LDR threshold tuning
* Freeze-state indication during testing

They should be **removed or disabled** in final PCB firmware.

---

## 4. Lockout & Freeze Logic (Important)

When an LDR hit is detected:

1. Score is sent once via UDP
2. Lockout timer starts (10 s)
3. First 5 s → motors forced OFF
4. Next 5 s → normal movement allowed
5. After 10 s → bot can be hit again

All timing uses `millis()` (non-blocking).

This ensures:

* No Wi‑Fi disconnections
* Predictable behavior

---

## 5. Motor Safety & Wi‑Fi Stability

* No `delay()` in control paths
* No blocking loops during freeze
* `yield()` is called every loop
* PWM frequency reduced to 1 kHz for Wi‑Fi stability

Motor stopping is **instant**, not ramped, by design (low-speed bot).

---

## 6. How to Test Safely (Breadboard)

### LDR Test

1. Ground MODE_PIN
2. Power ESP
3. Shine laser on LDR
4. LED turns ON above threshold

### Freeze Test

1. Run in normal mode
2. Trigger LDR
3. Bot freezes for ~5 s
4. Bot moves again, but cannot score

---

## 7. Known TODOs / Warnings

* ❗ Pinout must be updated to match PCB
* ❗ MODE_PIN and LED should be removed for final build
* ❗ PCB routing must be verified for boot pins

---

## 8. Intended Audience

This document is meant for:

* Debugging during assembly
* Firmware–hardware integration

Not meant as end‑user documentation.
