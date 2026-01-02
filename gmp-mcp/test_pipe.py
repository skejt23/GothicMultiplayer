"""
Test script for Gothic MCP Pipe connection.

Run this script to test the Named Pipe connection to the Gothic game.
"""

import json
import sys

from gothic_pipe_client import GothicPipeClient, PIPE_NAME


def test_connection():
    """Test basic pipe connection."""
    print("=" * 60)
    print("Gothic MCP Pipe Connection Test")
    print("=" * 60)
    print(f"\nPipe: {PIPE_NAME}")
    print("-" * 60)
    
    client = GothicPipeClient()
    
    print("\n[1] Attempting to connect...")
    if not client.connect():
        print("    ✗ Connection FAILED")
        print("\n    Make sure:")
        print("    - Gothic 2 is running with GMP client")
        print("    - The MCP pipe handler is enabled in the game")
        return False
    
    print("    ✓ Connected!")
    
    # Test commands
    test_commands = [
        ("getStatus", {}, "Game status"),
        ("waitForReady", {}, "Wait for ready"),
        ("getPlayerInfo", {}, "Player info"),
    ]
    
    print("\n[2] Testing commands...")
    print("-" * 60)
    
    all_passed = True
    for method, params, description in test_commands:
        print(f"\n    Testing: {description} ({method})")
        response = client.send_command(method, params)
        
        if "error" in response and not response.get("result"):
            print(f"    ✗ Error: {response.get('error')}")
            all_passed = False
        else:
            result = response.get("result", response)
            print(f"    ✓ Success:")
            print(f"      {json.dumps(result, indent=6)}")
    
    print("\n" + "-" * 60)
    client.disconnect()
    print("[3] Disconnected")
    
    print("\n" + "=" * 60)
    if all_passed:
        print("All tests PASSED!")
    else:
        print("Some tests FAILED")
    print("=" * 60)
    
    return all_passed


def interactive_mode():
    """Interactive mode for sending commands."""
    print("=" * 60)
    print("Gothic MCP Interactive Mode")
    print("=" * 60)
    print("\nType commands in format: method_name {json_params}")
    print("Examples:")
    print("  getStatus")
    print("  waitForReady {\"timeoutMs\": 5000}")
    print("  setPlayerPosition {\"x\": 0, \"y\": 0, \"z\": 0}")
    print("  setCameraAngle {\"yaw\": 90, \"pitch\": 10}")
    print("\nType 'quit' or 'exit' to leave, 'help' for command list")
    print("-" * 60)
    
    client = GothicPipeClient()
    
    if not client.connect():
        print("\nCould not connect to Gothic. Make sure the game is running.")
        return
    
    print("\nConnected to Gothic!\n")
    
    commands = [
        "getStatus", "waitForReady", "getPlayerInfo",
        "setPlayerPosition", "openInventory", "setCameraAngle",
        "getPixelColor", "getD3D9State"
    ]
    
    while True:
        try:
            user_input = input("\n> ").strip()
            
            if not user_input:
                continue
            
            if user_input.lower() in ("quit", "exit"):
                break
            
            if user_input.lower() == "help":
                print("\nAvailable commands:")
                for cmd in commands:
                    print(f"  - {cmd}")
                continue
            
            # Parse input
            parts = user_input.split(" ", 1)
            method = parts[0]
            params = {}
            
            if len(parts) > 1:
                try:
                    params = json.loads(parts[1])
                except json.JSONDecodeError as e:
                    print(f"Invalid JSON parameters: {e}")
                    continue
            
            # Send command
            print(f"Sending: {method} {json.dumps(params)}")
            response = client.send_command(method, params)
            print(f"Response: {json.dumps(response, indent=2)}")
            
        except KeyboardInterrupt:
            print("\n\nInterrupted")
            break
        except EOFError:
            break
    
    client.disconnect()
    print("\nDisconnected. Goodbye!")


def main():
    """Main entry point."""
    args = sys.argv[1:]
    
    if not args or args[0] == "test":
        test_connection()
    elif args[0] == "interactive" or args[0] == "-i":
        interactive_mode()
    else:
        print(f"Unknown command: {args[0]}")
        print("\nUsage:")
        print("  python test_pipe.py          - Run connection test")
        print("  python test_pipe.py test     - Run connection test")
        print("  python test_pipe.py -i       - Interactive mode")
        print("  python test_pipe.py interactive - Interactive mode")


if __name__ == "__main__":
    main()
