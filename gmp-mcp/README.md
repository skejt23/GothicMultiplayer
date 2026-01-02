# Gothic MCP Server

An MCP (Model Context Protocol) server that allows AI assistants (like Claude/Antigravity) to interact with the Gothic game through a Named Pipe + JSON communication channel.

## Architecture

```
┌─────────────────┐    MCP Protocol    ┌──────────────────┐    Named Pipe    ┌─────────────────┐
│   AI Assistant  │ ◄────────────────► │  MCP Server      │ ◄──────────────► │   Gothic Game   │
│   (Antigravity) │    JSON-RPC 2.0    │  (Python)        │    JSON         │   (C++ DLL)     │
└─────────────────┘                    └──────────────────┘                  └─────────────────┘
                                              │
                                              │ (Also manages)
                                              ▼
                                       ┌──────────────────┐
                                       │ Process Control  │
                                       │ - Start/Stop     │
                                       │ - Screenshots    │
                                       └──────────────────┘
```

## Components

### 1. MCP Server (`gothic_mcp_server.py`)
A Python MCP server that exposes Gothic game control as tools:
- `start_game` - Launch Gothic 2 with the GMP client
- `stop_game` - Terminate the Gothic process
- `take_screenshot` - Capture the game window
- `get_player_info` - Get current player information (position, health, etc.)
- `set_player_position` - Teleport the player
- `set_camera_angle` - Orbit the camera
- `open_inventory` - Open the player's inventory


### 2. Game Manager (`gothic_game_manager.py`)
Handles game process management entirely in Python:
- Start/stop Gothic using subprocess
- Capture screenshots using `mss` (no C++ required)
- Monitor process status (memory, CPU, runtime)

### 3. Named Pipe Protocol
Communication between MCP Server and Gothic uses Named Pipes with JSON messages:

**Request Format:**
```json
{
  "id": "uuid",
  "method": "methodName",
  "params": { ... }
}
```

**Response Format:**
```json
{
  "id": "uuid",
  "result": { ... },
  "error": null
}
```

### 4. C++ Integration (`mcp_pipe_handler.cpp/h`)
A C++ module integrated into the Gothic client DLL that:
- Creates and manages the Named Pipe server
- Receives commands from the MCP server
- Executes commands via the existing Lua bindings
- Sends responses back

## Installation

### Prerequisites
- Python 3.10+
- Gothic 2 with GMP client

### Setup
```bash
cd gmp-mcp
pip install -r requirements.txt
```

### Running the MCP Server
```bash
python gothic_mcp_server.py
```

## MCP Tools Reference

### Game Management (Python-side, no game connection required)

| Tool | Description |
|------|-------------|
| `gothic_start_game` | Start Gothic 2 with GMP launcher |
| `gothic_stop_game` | Stop the Gothic game process |
| `gothic_take_screenshot` | Capture a screenshot of the game window |
| `gothic_game_status` | Get process status (PID, memory, CPU, runtime) |
| `gothic_configure_paths` | Configure paths to Gothic executables |

### In-Game Control (requires game to be running)

| Tool | Description |
|------|-------------|
| `gothic_get_status` | Check game connection status |
| `gothic_get_player_info` | Get player position, health, stats |
| `gothic_wait_for_ready` | Block until game is fully loaded and ready |
| `gothic_set_player_position` | Teleport player to coordinates |
| `gothic_open_inventory` | Open player inventory screen |
| `gothic_set_camera_angle` | Rotate camera (yaw/pitch) |
| `gothic_get_pixel_color` | Get pixel color from screen coordinates |
| `gothic_get_d3d9_state` | Get D3D9 debugging info |

## Development

### Testing the Pipe Connection
```bash
python test_pipe.py
```

### Testing Game Management

You can use the included `test_game.py` script to test the full lifecycle (Start -> Connect -> Screenshot -> Stop):

```bash
python test_game.py
```

Or use the library directly:

```python
from gothic_game_manager import get_game_manager

manager = get_game_manager()

# Check if game is running
print(manager.get_status())

# Start the game
result = manager.start_game()
print(result)

# Take a screenshot
screenshot = manager.capture_screenshot()
print(f"Screenshot saved to: {screenshot['filepath']}")

# Stop the game
manager.stop_game()
```

## Configuration for AI Agents (Antigravity/Cline/Cursor)

To use this MCP server with your AI assistant, add the following configuration to your MCP settings file (e.g., `cline_mcp_settings.json` or `claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "gothic-mcp": {
      "command": "python",
      "args": [
        "c:/git/GothicMultiplayer/gmp-mcp/gothic_mcp_server.py"
      ],
      "disabled": false,
      "autoApprove": []
    }
  }
}
```

Make sure `python` is in your system PATH and dependencies are installed (`pip install -r requirements.txt`).

## License
MIT License - See LICENSE file in the repository root.
