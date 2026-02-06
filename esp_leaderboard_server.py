from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from typing import List, Dict
import json
import asyncio
from datetime import datetime
import uvicorn

app = FastAPI(title="ESP Leaderboard Server")

# Store active WebSocket connections
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        # Initialize leaderboard for two ESPs
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
        # Send current leaderboard to newly connected client
        await websocket.send_json({
            "type": "initial_data",
            "leaderboard": self.get_sorted_leaderboard()
        })
    
    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
    
    async def broadcast(self, message: dict):
        """Broadcast message to all connected WebSocket clients"""
        disconnected = []
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except:
                disconnected.append(connection)
        
        # Remove disconnected clients
        for connection in disconnected:
            if connection in self.active_connections:
                self.active_connections.remove(connection)
    
    def update_scores(self, requesting_esp_id: str):
        """
        When one ESP sends a request, give 10 points to the OTHER ESP
        """
        if requesting_esp_id not in self.leaderboard:
            raise ValueError(f"Unknown ESP ID: {requesting_esp_id}")
        
        # Find the other ESP
        other_esp_id = "ESP_2" if requesting_esp_id == "ESP_1" else "ESP_1"
        
        # Award points to the OTHER ESP
        self.leaderboard[other_esp_id]["score"] += 10
        
        # Update request stats for the requesting ESP
        self.leaderboard[requesting_esp_id]["last_request"] = datetime.now().isoformat()
        self.leaderboard[requesting_esp_id]["total_requests"] += 1
        
        return other_esp_id
    
    def get_sorted_leaderboard(self):
        """Return leaderboard sorted by score (highest first)"""
        return sorted(
            self.leaderboard.values(),
            key=lambda x: x["score"],
            reverse=True
        )
    
    def reset_leaderboard(self):
        """Reset all scores to zero"""
        for esp_id in self.leaderboard:
            self.leaderboard[esp_id]["score"] = 0
            self.leaderboard[esp_id]["total_requests"] = 0
            self.leaderboard[esp_id]["last_request"] = None

manager = ConnectionManager()

# HTML page endpoint
@app.get("/", response_class=HTMLResponse)
async def get_dashboard():
    with open("leaderboard.html", "r", encoding="utf-8") as f:
        return f.read()

# REST API endpoint for ESP devices to send requests
@app.post("/api/esp/request")
async def esp_request(data: dict):
    """
    ESP devices send requests to this endpoint
    Expected format: {
        "esp_id": "ESP_1"  # or "ESP_2"
    }
    When ESP_1 sends a request, ESP_2 gets 10 points (and vice versa)
    """
    if "esp_id" not in data:
        raise HTTPException(status_code=400, detail="esp_id is required")
    
    esp_id = data["esp_id"]
    
    if esp_id not in manager.leaderboard:
        raise HTTPException(status_code=400, detail=f"Invalid esp_id. Must be ESP_1 or ESP_2")
    
    # Update scores (give points to the OTHER ESP)
    winner_id = manager.update_scores(esp_id)
    
    # Broadcast updated leaderboard to all connected clients
    await manager.broadcast({
        "type": "score_update",
        "requesting_esp": esp_id,
        "points_awarded_to": winner_id,
        "points": 10,
        "leaderboard": manager.get_sorted_leaderboard(),
        "timestamp": datetime.now().isoformat()
    })
    
    return {
        "status": "success",
        "requesting_esp": esp_id,
        "points_awarded_to": winner_id,
        "points": 10,
        "leaderboard": manager.get_sorted_leaderboard()
    }

# WebSocket endpoint for real-time updates to web clients
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            # Keep connection alive and receive any client messages
            data = await websocket.receive_text()
            
            # Handle client commands
            try:
                message = json.loads(data)
                if message.get("command") == "reset":
                    manager.reset_leaderboard()
                    await manager.broadcast({
                        "type": "leaderboard_reset",
                        "leaderboard": manager.get_sorted_leaderboard(),
                        "timestamp": datetime.now().isoformat()
                    })
            except:
                # Just echo back for keep-alive
                await websocket.send_json({"type": "pong", "message": "Connection alive"})
    except WebSocketDisconnect:
        manager.disconnect(websocket)

# Get current leaderboard
@app.get("/api/leaderboard")
async def get_leaderboard():
    """Get current leaderboard"""
    return {
        "leaderboard": manager.get_sorted_leaderboard(),
        "connected_clients": len(manager.active_connections)
    }

# Get specific ESP status
@app.get("/api/esp/{esp_id}")
async def get_esp_status(esp_id: str):
    """Get status of a specific ESP device"""
    if esp_id not in manager.leaderboard:
        raise HTTPException(status_code=404, detail="ESP device not found")
    return manager.leaderboard[esp_id]

# Reset leaderboard
@app.post("/api/leaderboard/reset")
async def reset_leaderboard():
    """Reset all scores to zero"""
    manager.reset_leaderboard()
    
    # Broadcast reset to all connected clients
    await manager.broadcast({
        "type": "leaderboard_reset",
        "leaderboard": manager.get_sorted_leaderboard(),
        "timestamp": datetime.now().isoformat()
    })
    
    return {
        "status": "success",
        "message": "Leaderboard reset",
        "leaderboard": manager.get_sorted_leaderboard()
    }

if __name__ == "__main__":
    # Get local IP address
    import socket
    hostname = socket.gethostname()
    local_ip = socket.gethostbyname(hostname)
    
    print(f"\n{'='*60}")
    print(f"🏆 ESP Leaderboard Server Starting...")
    print(f"{'='*60}")
    print(f"📡 Server running on:")
    print(f"   - Local:   http://localhost:8000")
    print(f"   - Network: http://{local_ip}:8000")
    print(f"\n📊 Leaderboard Dashboard: Open the above URL in your browser")
    print(f"🔌 WebSocket: ws://{local_ip}:8000/ws")
    print(f"📮 ESP request endpoint: http://{local_ip}:8000/api/esp/request")
    print(f"\n⚡ How it works:")
    print(f"   - When ESP_1 sends a request → ESP_2 gets +10 points")
    print(f"   - When ESP_2 sends a request → ESP_1 gets +10 points")
    print(f"{'='*60}\n")
    
    # Run server accessible from network
    uvicorn.run(
        app, 
        host="0.0.0.0",  # Listen on all network interfaces
        port=8000,
        log_level="info"
    )
