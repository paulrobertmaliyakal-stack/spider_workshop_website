from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.responses import HTMLResponse
from typing import List, Dict
import json
import asyncio
from datetime import datetime
import socket
import uvicorn

app = FastAPI(title="ESP Leaderboard Server")

# ---------------- CONNECTION MANAGER ---------------- #

class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self.leaderboard: Dict[str, dict] = {
            "ESP_1": {
                "esp_id": "ESP_1",
                "name": "ESP Device 1",
                "score": 0,
                "last_request": None,
                "total_requests": 0
            },
            "ESP_2": {
                "esp_id": "ESP_2",
                "name": "ESP Device 2",
                "score": 0,
                "last_request": None,
                "total_requests": 0
            }
        }

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

        await websocket.send_json({
            "type": "initial_data",
            "leaderboard": self.get_sorted_leaderboard(),
            "connected_clients": len(self.active_connections)
        })

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        dead = []
        for ws in self.active_connections:
            try:
                await ws.send_json(message)
            except:
                dead.append(ws)
        for ws in dead:
            self.disconnect(ws)

    def update_scores(self, sender_id: str):
        if sender_id not in self.leaderboard:
            return None

        winner = "ESP_2" if sender_id == "ESP_1" else "ESP_1"
        self.leaderboard[winner]["score"] += 10
        self.leaderboard[sender_id]["total_requests"] += 1
        self.leaderboard[sender_id]["last_request"] = datetime.now().isoformat()
        return winner

    def reset(self):
        for esp in self.leaderboard.values():
            esp["score"] = 0
            esp["total_requests"] = 0
            esp["last_request"] = None

    def get_sorted_leaderboard(self):
        return sorted(
            self.leaderboard.values(),
            key=lambda x: x["score"],
            reverse=True
        )

manager = ConnectionManager()

# ---------------- HTTP ROUTES ---------------- #

@app.get("/", response_class=HTMLResponse)
async def dashboard():
    with open("leaderboard.html", "r", encoding="utf-8") as f:
        return f.read()

@app.post("/api/leaderboard/reset")
async def reset_leaderboard():
    manager.reset()
    await manager.broadcast({
        "type": "leaderboard_reset",
        "leaderboard": manager.get_sorted_leaderboard(),
        "timestamp": datetime.now().isoformat()
    })
    return {"status": "ok"}

# ---------------- WEBSOCKET (NON-BLOCKING) ---------------- #

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            await asyncio.sleep(30)   # keep-alive only
    except WebSocketDisconnect:
        manager.disconnect(websocket)

# ---------------- UDP LISTENER (ESP SCORING) ---------------- #

UDP_PORT = 9000

async def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", UDP_PORT))
    sock.setblocking(False)

    print(f"📡 UDP listening on port {UDP_PORT}")

    loop = asyncio.get_event_loop()

    while True:
        try:
            data, addr = await loop.sock_recvfrom(sock, 1024)
            msg = data.decode().strip()
            print("UDP:", msg)

            if msg.startswith("SCORE,"):
                esp_id = msg.split(",")[1]
                winner = manager.update_scores(esp_id)

                if winner:
                    await manager.broadcast({
                        "type": "score_update",
                        "requesting_esp": esp_id,
                        "points_awarded_to": winner,
                        "points": 10,
                        "leaderboard": manager.get_sorted_leaderboard(),
                        "timestamp": datetime.now().isoformat()
                    })
        except Exception:
            await asyncio.sleep(0.01)

@app.on_event("startup")
async def startup():
    asyncio.create_task(udp_listener())

# ---------------- RUN SERVER ---------------- #

if __name__ == "__main__":
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)

    print(f"""
🏆 ESP LEADERBOARD SERVER
--------------------------------
🌐 Web     : http://{local_ip}:8000
📡 UDP     : port 9000
🔌 WS      : ws://{local_ip}:8000/ws
--------------------------------
""")

    uvicorn.run(app, host="0.0.0.0", port=8000)