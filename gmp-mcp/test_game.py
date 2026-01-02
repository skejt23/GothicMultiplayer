"""
Complete test script for Gothic MCP ecosystem.
This script demonstrates the full lifecycle:
1. Starting the game via GameManager
2. Connecting via Named Pipe
3. Sending commands
4. Taking screenshots
5. Stopping the game
"""

import json
import time
import sys
import os
from pathlib import Path

# Add current directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gothic_game_manager import get_game_manager
from gothic_pipe_client import GothicPipeClient

import logging

def main():
    logging.basicConfig(level=logging.INFO, format='%(message)s')
    print("=" * 60)
    print("Gothic MCP - Full System Test")
    print("=" * 60)
    
    manager = get_game_manager()
    client = GothicPipeClient()
    
    # 1. Manage Game Process
    print("\n[1] Checking Game Status...")
    status = manager.get_status()
    print(f"    Running: {status['running']}")
    
    if not status['running']:
        print("\n[2] Starting Game...")
        # Configure paths if needed - modify these if your paths differ!
        # manager.configure_paths(gothic_path=r"C:\Gothic II\System\Gothic2.exe")
        
        result = manager.start_game(wait_for_window=True)
        if not result['success']:
            print(f"    ✗ Failed to start game: {result.get('error')}")
            return
        print(f"    ✓ Game started! PID: {result['pid']}")
        
        # Give it a moment to initialize the pipe server
        print("    Waiting for initialization...")
        time.sleep(5)
    else:
        print("    Game already running, proceeding...")

    # 2. Connect to Pipe
    print("\n[3] Connecting to Game Pipe...")
    connected = False
    for i in range(5):
        if client.connect():
            connected = True
            break
        print(f"    Retrying connection ({i+1}/5)...")
        time.sleep(2)
        
    if not connected:
        print("    ✗ Failed to connect to Named Pipe")
        return
    print("    ✓ Connected to Gothic!")

    # 3. Wait for Ready (Loading Screen correct)
    print("\n[4] Waiting for Game Ready (Player in world)...")
    print("    (This handles loading screens automatically)")
    
    # Retrieve status in loop
    ready = False
    max_wait = 60 # seconds
    start_time = time.time()
    
    while time.time() - start_time < max_wait:
        response = client.send_command("getStatus", {})
        status = response.get("result", {})
        
        state = status.get("gameState", "unknown")
        is_ready = status.get("isReady", False)
        
        print(f"    Status: {state}, Ready: {is_ready}", end="\r")
        
        if is_ready:
            ready = True
            print(f"\n    ✓ Game is READY!")
            break
            
        time.sleep(1)
        
    if not ready:
        print("\n    ⚠️ Timed out waiting for ready state. Proceeding anyway...")

    # 5. Open Inventory
    print("\n[5] Opening Inventory...")
    client.send_command("openInventory", {})
    # Wait for UI animation
    time.sleep(1.0)

    # 6. Take Screenshot
    print("\n[6] Taking Screenshot...")
    screen_result = manager.capture_screenshot(filename="inventory_screenshot")
    if screen_result['success']:
        print(f"    ✓ Screenshot saved: {screen_result.get('filepath')}")
    else:
        print(f"    ✗ Screenshot failed: {screen_result.get('error')}")

    # 7. Rotate Camera
    print("\n[7] Rotating Camera (Yaw=90, Pitch=-20)...")
    client.send_command("setCameraAngle", {"yaw": 90.0, "pitch": -20.0})
    time.sleep(1.0)
    screen_result = manager.capture_screenshot(filename="camera_test")
    if screen_result['success']:
        print(f"    ✓ Screenshot saved: {screen_result.get('filepath')}")

    # 8. Send Other Commands
    print("\n[8] Sending Other Commands...")
    
    commands = [
        ("getPlayerInfo", {}),
    ]
    
    for method, params in commands:
        print(f"    > {method}...")
        resp = client.send_command(method, params)
        if "error" in resp:
            print(f"      Error: {resp['error']}")
        else:
            print(f"      Success")

    # 8. Stop Game?
    print("\n" + "=" * 60)
    choice = input("Test complete. Stop game? (y/n): ").lower()

    if choice == 'y':
        print("\n[9] Stopping Game...")
        manager.stop_game()
        print("    ✓ Game stopped")
    else:
        print("    Game left running.")
        
    client.disconnect()

if __name__ == "__main__":
    main()
