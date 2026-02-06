import requests
import time
import random
from datetime import datetime

# Server configuration
SERVER_URL = "http://localhost:8000"
ESP_REQUEST_ENDPOINT = f"{SERVER_URL}/api/esp/request"
LEADERBOARD_ENDPOINT = f"{SERVER_URL}/api/leaderboard"
RESET_ENDPOINT = f"{SERVER_URL}/api/leaderboard/reset"

def print_separator():
    print("\n" + "="*60)

def send_esp_request(esp_id: str):
    """Simulate an ESP device sending a request"""
    try:
        response = requests.post(
            ESP_REQUEST_ENDPOINT,
            json={"esp_id": esp_id},
            timeout=5
        )
        
        if response.status_code == 200:
            data = response.json()
            print(f"✅ {esp_id} sent request")
            print(f"   → {data['points_awarded_to']} received +{data['points']} points")
            return True
        else:
            print(f"❌ Error: {response.status_code} - {response.text}")
            return False
    except requests.exceptions.ConnectionError:
        print(f"❌ Connection Error: Server not running at {SERVER_URL}")
        print(f"   Make sure the FastAPI server is running first!")
        return False
    except Exception as e:
        print(f"❌ Error: {e}")
        return False

def get_leaderboard():
    """Get current leaderboard"""
    try:
        response = requests.get(LEADERBOARD_ENDPOINT, timeout=5)
        if response.status_code == 200:
            return response.json()
        else:
            print(f"❌ Error getting leaderboard: {response.status_code}")
            return None
    except Exception as e:
        print(f"❌ Error: {e}")
        return None

def display_leaderboard():
    """Display the current leaderboard"""
    data = get_leaderboard()
    if data:
        print_separator()
        print("📊 CURRENT LEADERBOARD:")
        print_separator()
        for i, esp in enumerate(data['leaderboard'], 1):
            trophy = "🥇" if i == 1 else "🥈" if i == 2 else "🥉"
            print(f"{trophy} {esp['name']}")
            print(f"   Score: {esp['score']} points")
            print(f"   Total Requests: {esp['total_requests']}")
            print(f"   Last Request: {esp['last_request'] or 'Never'}")
            print()
        print(f"🌐 Connected Clients: {data['connected_clients']}")
        print_separator()

def reset_leaderboard():
    """Reset the leaderboard"""
    try:
        response = requests.post(RESET_ENDPOINT, timeout=5)
        if response.status_code == 200:
            print("✅ Leaderboard reset successfully!")
            return True
        else:
            print(f"❌ Error resetting: {response.status_code}")
            return False
    except Exception as e:
        print(f"❌ Error: {e}")
        return False

def test_single_request():
    """Test 1: Send a single request from ESP_1"""
    print("\n🧪 TEST 1: Single Request from ESP_1")
    print_separator()
    send_esp_request("ESP_1")
    time.sleep(0.5)
    display_leaderboard()

def test_alternating_requests():
    """Test 2: Send alternating requests"""
    print("\n🧪 TEST 2: Alternating Requests (5 from each ESP)")
    print_separator()
    for i in range(5):
        esp_id = "ESP_1" if i % 2 == 0 else "ESP_2"
        send_esp_request(esp_id)
        time.sleep(0.3)
    time.sleep(0.5)
    display_leaderboard()

def test_random_requests():
    """Test 3: Send random requests"""
    print("\n🧪 TEST 3: Random Requests (10 requests)")
    print_separator()
    for i in range(10):
        esp_id = random.choice(["ESP_1", "ESP_2"])
        send_esp_request(esp_id)
        time.sleep(random.uniform(0.2, 0.5))
    time.sleep(0.5)
    display_leaderboard()

def test_rapid_fire():
    """Test 4: Rapid fire from one ESP"""
    print("\n🧪 TEST 4: Rapid Fire from ESP_2 (10 requests)")
    print_separator()
    for i in range(10):
        send_esp_request("ESP_2")
        time.sleep(0.1)
    time.sleep(0.5)
    display_leaderboard()

def interactive_mode():
    """Interactive mode - manual control"""
    print("\n🎮 INTERACTIVE MODE")
    print_separator()
    
    while True:
        print("\nOptions:")
        print("  1 - Send request from ESP_1")
        print("  2 - Send request from ESP_2")
        print("  3 - Send 5 random requests")
        print("  4 - Show leaderboard")
        print("  5 - Reset leaderboard")
        print("  0 - Exit")
        
        choice = input("\nEnter choice: ").strip()
        
        if choice == "1":
            send_esp_request("ESP_1")
        elif choice == "2":
            send_esp_request("ESP_2")
        elif choice == "3":
            for _ in range(5):
                esp_id = random.choice(["ESP_1", "ESP_2"])
                send_esp_request(esp_id)
                time.sleep(0.2)
        elif choice == "4":
            display_leaderboard()
        elif choice == "5":
            reset_leaderboard()
            time.sleep(0.5)
            display_leaderboard()
        elif choice == "0":
            print("\n👋 Goodbye!")
            break
        else:
            print("❌ Invalid choice")

def main():
    print("="*60)
    print("🧪 ESP LEADERBOARD SERVER TEST SUITE")
    print("="*60)
    print(f"Server: {SERVER_URL}")
    print(f"Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    # Check if server is running
    print("\n🔍 Checking server status...")
    try:
        response = requests.get(LEADERBOARD_ENDPOINT, timeout=5)
        if response.status_code == 200:
            print("✅ Server is running!")
        else:
            print(f"⚠️ Server responded with status: {response.status_code}")
    except requests.exceptions.ConnectionError:
        print("❌ Server is not running!")
        print(f"\nPlease start the server first by running:")
        print(f"   python main.py")
        return
    except Exception as e:
        print(f"❌ Error: {e}")
        return
    
    # Show initial state
    display_leaderboard()
    
    # Menu
    while True:
        print("\n" + "="*60)
        print("TEST MENU:")
        print("="*60)
        print("  1 - Run all automated tests")
        print("  2 - Test single request")
        print("  3 - Test alternating requests")
        print("  4 - Test random requests")
        print("  5 - Test rapid fire")
        print("  6 - Interactive mode")
        print("  7 - Show leaderboard")
        print("  8 - Reset leaderboard")
        print("  0 - Exit")
        
        choice = input("\nEnter choice: ").strip()
        
        if choice == "1":
            # Run all tests
            reset_leaderboard()
            time.sleep(1)
            test_single_request()
            time.sleep(2)
            
            reset_leaderboard()
            time.sleep(1)
            test_alternating_requests()
            time.sleep(2)
            
            reset_leaderboard()
            time.sleep(1)
            test_random_requests()
            time.sleep(2)
            
            reset_leaderboard()
            time.sleep(1)
            test_rapid_fire()
            
        elif choice == "2":
            test_single_request()
        elif choice == "3":
            test_alternating_requests()
        elif choice == "4":
            test_random_requests()
        elif choice == "5":
            test_rapid_fire()
        elif choice == "6":
            interactive_mode()
        elif choice == "7":
            display_leaderboard()
        elif choice == "8":
            reset_leaderboard()
            time.sleep(0.5)
            display_leaderboard()
        elif choice == "0":
            print("\n👋 Goodbye!")
            break
        else:
            print("❌ Invalid choice")

if __name__ == "__main__":
    main()
