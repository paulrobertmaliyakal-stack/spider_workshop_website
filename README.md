# ESP Leaderboard System

A real-time competitive leaderboard system for two ESP devices. When one ESP sends a request, the **other ESP gets +10 points**!

## 🎯 How It Works

- **ESP_1** sends a request → **ESP_2** gets +10 points
- **ESP_2** sends a request → **ESP_1** gets +10 points
- Real-time updates via WebSocket
- Beautiful web dashboard to view the competition

## 📋 Requirements

### Server (Python)
- Python 3.7+
- FastAPI
- Uvicorn
- Install with: `pip install fastapi uvicorn websockets`

### ESP Devices
- ESP32 or ESP8266
- Arduino IDE or PlatformIO
- Libraries needed:
  - WiFi (built-in for ESP32) or ESP8266WiFi
  - HTTPClient (built-in)
  - ArduinoJson (install via Library Manager)

## 🚀 Quick Start

### 1. Setup the Server

1. Install Python dependencies:
```bash
pip install fastapi uvicorn websockets
```

2. Place both files in the same folder:
   - `esp_leaderboard_server.py`
   - `leaderboard.html`

3. Run the server:
```bash
python esp_leaderboard_server.py
```

4. Note the server IP address shown in the console (e.g., `http://192.168.1.100:8000`)

### 2. Setup ESP Devices

#### For ESP #1:
1. Open `esp_leaderboard_client.ino` in Arduino IDE
2. Configure:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* serverIP = "192.168.1.100";  // Your server IP
   const char* espID = "ESP_1";  // Keep as ESP_1
   ```
3. Upload to first ESP

#### For ESP #2:
1. Open the same file
2. Configure:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* serverIP = "192.168.1.100";  // Your server IP
   const char* espID = "ESP_2";  // CHANGE to ESP_2
   ```
3. Upload to second ESP

### 3. View the Dashboard

Open your browser and go to:
```
http://192.168.1.100:8000
```
(Replace with your server's IP address)

## 🎮 Usage

### Trigger Requests from ESP

**Method 1: Button Press**
- Press the boot button (GPIO 0) on either ESP
- The OTHER ESP will get +10 points
- Watch the leaderboard update in real-time!

**Method 2: Automatic Requests (Optional)**
Uncomment these lines in the Arduino code to send requests every 10 seconds:
```cpp
static unsigned long lastAutoRequest = 0;
if (millis() - lastAutoRequest > 10000) {
  sendRequest();
  lastAutoRequest = millis();
}
```

### Dashboard Features

- **Live Leaderboard**: See current scores with rankings (🥇 and 🥈)
- **Real-time Updates**: Instant notifications when points are awarded
- **Statistics**: View total requests and last request time for each ESP
- **Reset Button**: Clear all scores and start fresh
- **Connection Status**: See WebSocket connection status

## 📡 API Endpoints

### For ESP Devices

**Send Request** (Awards points to the OTHER ESP)
```
POST http://your-server-ip:8000/api/esp/request
Content-Type: application/json

{
  "esp_id": "ESP_1"  // or "ESP_2"
}
```

**Response:**
```json
{
  "status": "success",
  "requesting_esp": "ESP_1",
  "points_awarded_to": "ESP_2",
  "points": 10,
  "leaderboard": [...]
}
```

### For Web Dashboard

**Get Current Leaderboard**
```
GET http://your-server-ip:8000/api/leaderboard
```

**Reset Leaderboard**
```
POST http://your-server-ip:8000/api/leaderboard/reset
```

**Get Specific ESP Status**
```
GET http://your-server-ip:8000/api/esp/ESP_1
GET http://your-server-ip:8000/api/esp/ESP_2
```

## 🔧 Customization

### Change Device Names
Edit in `esp_leaderboard_server.py`:
```python
self.leaderboard: Dict[str, dict] = {
    "ESP_1": {
        "esp_id": "ESP_1",
        "name": "Team Red",  # Change this
        ...
    },
    "ESP_2": {
        "esp_id": "ESP_2",
        "name": "Team Blue",  # Change this
        ...
    }
}
```

### Change Points Per Request
Edit in `esp_leaderboard_server.py`:
```python
self.leaderboard[other_esp_id]["score"] += 10  # Change 10 to any value
```

### Change Button Pin
Edit in `esp_leaderboard_client.ino`:
```cpp
const int buttonPin = 0;  // Change to any GPIO pin
```

## 🐛 Troubleshooting

### ESP can't connect to WiFi
- Double-check SSID and password
- Ensure ESP is in range of WiFi router
- Check Serial Monitor (115200 baud) for error messages

### ESP can't reach server
- Verify server IP address is correct
- Ensure server is running
- Check that ESP and server are on the same network
- Try pinging the server from another device

### Dashboard shows "Disconnected"
- Check if server is running
- Refresh the browser page
- Check browser console for WebSocket errors

### No points being awarded
- Verify ESP is sending correct esp_id ("ESP_1" or "ESP_2")
- Check server logs for errors
- Test the API endpoint with curl or Postman

## 📊 Testing with Curl

Test the server without ESP devices:

```bash
# Award points to ESP_2 (by having ESP_1 send a request)
curl -X POST http://192.168.1.100:8000/api/esp/request \
  -H "Content-Type: application/json" \
  -d '{"esp_id": "ESP_1"}'

# Award points to ESP_1 (by having ESP_2 send a request)
curl -X POST http://192.168.1.100:8000/api/esp/request \
  -H "Content-Type: application/json" \
  -d '{"esp_id": "ESP_2"}'

# Get current leaderboard
curl http://192.168.1.100:8000/api/leaderboard

# Reset leaderboard
curl -X POST http://192.168.1.100:8000/api/leaderboard/reset
```

## 🎨 Features

- ✅ Real-time WebSocket updates
- ✅ Beautiful gradient UI with animations
- ✅ Score change animations
- ✅ Pop-up notifications for point awards
- ✅ Connection status indicator
- ✅ Responsive design for mobile/desktop
- ✅ Automatic reconnection
- ✅ Request statistics tracking
- ✅ One-click leaderboard reset

## 📝 License

Free to use and modify!

## 🤝 Support

If you encounter issues:
1. Check the troubleshooting section
2. Review server logs
3. Check ESP Serial Monitor output
4. Verify network connectivity

Happy competing! 🏆
