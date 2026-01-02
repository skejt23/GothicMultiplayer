"""
Gothic MCP Server - Main Entry Point

This MCP server provides tools for AI assistants to interact with the Gothic game.
Communication with the game uses Named Pipes with JSON-formatted messages.
"""

# Redirect stderr to file IMMEDIATELY, before ANY imports that might print warnings
# The MCP protocol uses stdio, so ANY non-JSON output to stdout/stderr will corrupt it
import sys
import os
from pathlib import Path

_log_dir = Path(__file__).parent
_stderr_log = open(_log_dir / 'gothic_mcp_stderr.log', 'a')
sys.stderr = _stderr_log

# Now safe to import other modules
import asyncio
import json
import logging
import uuid
from typing import Any, Optional

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent

from gothic_pipe_client import GothicPipeClient
from gothic_game_manager import get_game_manager, GothicGameManager

# Configure logging - force=True to override any existing config
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(_log_dir / 'gothic_mcp_server.log')
    ],
    force=True  # Override any existing logging configuration
)
logger = logging.getLogger(__name__)

# Initialize MCP server
app = Server("gothic-mcp-server")

# Global pipe client instance
pipe_client: Optional[GothicPipeClient] = None


def get_pipe_client() -> GothicPipeClient:
    """Get or create the Gothic pipe client."""
    global pipe_client
    if pipe_client is None:
        pipe_client = GothicPipeClient()
    return pipe_client


# =============================================================================
# MCP Tool Definitions
# =============================================================================

@app.list_tools()
async def list_tools() -> list[Tool]:
    """Return the list of available tools."""
    return [
        Tool(
            name="gothic_get_status",
            description="Check if Gothic game is running and connected. Returns connection status and basic game info.",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        Tool(
            name="gothic_wait_for_ready",
            description="Block execution until the game is fully loaded and the player is controlable. Includes automatic 1.5s settle delay for loading screen and world initialization.",
            inputSchema={
                "type": "object",
                "properties": {
                    "timeout_ms": {"type": "integer", "description": "Maximum time to wait in milliseconds. Default: 30000"},
                    "poll_interval_ms": {"type": "integer", "description": "Time between checks in milliseconds. Default: 100"}
                },
                "required": []
            }
        ),
        Tool(
            name="gothic_get_player_info",
            description="Get detailed information about the local player including position, health, mana, attributes, and current world.",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        Tool(
            name="gothic_set_player_position",
            description="Teleport the player to the specified world coordinates (X, Y, Z).",
            inputSchema={
                "type": "object",
                "properties": {
                    "x": {"type": "number", "description": "X coordinate in world units"},
                    "y": {"type": "number", "description": "Y coordinate in world units (height)"},
                    "z": {"type": "number", "description": "Z coordinate in world units"}
                },
                "required": ["x", "y", "z"]
            }
        ),
        Tool(
            name="gothic_open_inventory",
            description="Open the player's inventory screen.",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        Tool(
            name="gothic_set_camera_angle",
            description="Orbit the camera around the player (Yaw/Pitch) in degrees.",
            inputSchema={
                "type": "object",
                "properties": {
                    "yaw": {"type": "number", "description": "Horizontal angle (deg)"},
                    "pitch": {"type": "number", "description": "Vertical angle (deg)"}
                },
                "required": []
            }
        ),
        Tool(
            name="gothic_get_pixel_color",
            description="Sample the color of a pixel at the specified screen coordinates. Useful for automated visual verification.",
            inputSchema={
                "type": "object",
                "properties": {
                    "x": {"type": "integer", "description": "X screen coordinate (pixels from left)"},
                    "y": {"type": "integer", "description": "Y screen coordinate (pixels from top)"}
                },
                "required": ["x", "y"]
            }
        ),
        Tool(
            name="gothic_get_d3d9_state",
            description="Get detailed D3D9 render state information for debugging rendering issues. Returns viewport, FVF, textures, stage states, and render states.",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        # =========================================================================
        # Game Management Tools (Python-side, no pipe required)
        # =========================================================================
        Tool(
            name="gothic_start_game",
            description="Start the Gothic game. Launches Gothic.exe directly.",
            inputSchema={
                "type": "object",
                "properties": {
                    # "use_launcher": {  # Commented out - always use Gothic.exe directly
                    #     "type": "boolean", 
                    #     "description": "If true, use GMPLauncher.exe to inject GMP.dll. If false, start Gothic.exe directly. Default: false"
                    # },
                    "wait_for_window": {
                        "type": "boolean",
                        "description": "If true, wait for the game window to appear before returning. Default: true"
                    },
                    "timeout": {
                        "type": "number",
                        "description": "Maximum seconds to wait for game to start. Default: 30"
                    }
                },
                "required": []
            }
        ),
        Tool(
            name="gothic_stop_game",
            description="Stop the Gothic game. Can gracefully terminate or force kill the process.",
            inputSchema={
                "type": "object",
                "properties": {
                    "force": {
                        "type": "boolean",
                        "description": "If true, forcefully kill the process immediately. If false, try graceful shutdown first. Default: false"
                    },
                    "timeout": {
                        "type": "number",
                        "description": "Seconds to wait for graceful shutdown before forcing. Default: 10"
                    }
                },
                "required": []
            }
        ),
        Tool(
            name="gothic_take_screenshot",
            description="Capture a screenshot of the Gothic game window. Returns the file path and optionally the image data.",
            inputSchema={
                "type": "object",
                "properties": {
                    "filename": {
                        "type": "string",
                        "description": "Custom filename for the screenshot (without extension). Auto-generated if not provided."
                    },
                    "return_base64": {
                        "type": "boolean",
                        "description": "If true, include base64-encoded image data in the response. Default: false"
                    }
                },
                "required": []
            }
        ),
        Tool(
            name="gothic_game_status",
            description="Get detailed status of the Gothic game process including PID, memory usage, CPU usage, and runtime.",
            inputSchema={
                "type": "object",
                "properties": {},
                "required": []
            }
        ),
        Tool(
            name="gothic_configure_paths",
            description="Configure the paths to Gothic executables. Use this if Gothic is not in the default location.",
            inputSchema={
                "type": "object",
                "properties": {
                    "gothic_path": {
                        "type": "string",
                        "description": "Full path to Gothic.exe"
                    },
                    "launcher_path": {
                        "type": "string",
                        "description": "Full path to GMPLauncher.exe"
                    }
                },
                "required": []
            }
        )
    ]


@app.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[TextContent]:
    """Handle tool calls from the MCP client."""
    logger.info(f"Tool called: {name} with arguments: {arguments}")
    
    try:
        result = await handle_tool_call(name, arguments)
        return [TextContent(type="text", text=json.dumps(result, indent=2))]
    except Exception as e:
        logger.error(f"Error handling tool {name}: {e}")
        return [TextContent(type="text", text=json.dumps({
            "error": str(e),
            "tool": name
        }, indent=2))]


async def handle_tool_call(name: str, arguments: dict[str, Any]) -> dict[str, Any]:
    """Route tool calls to appropriate handlers."""
    
    # Game management tools (Python-side, no pipe required)
    game_manager = get_game_manager()
    
    game_management_handlers = {
        "gothic_start_game": lambda: game_manager.start_game(
            # use_launcher=arguments.get("use_launcher", False),  # Commented out
            wait_for_window=arguments.get("wait_for_window", True),
            timeout=arguments.get("timeout", 30.0)
        ),
        
        "gothic_stop_game": lambda: game_manager.stop_game(
            force=arguments.get("force", False),
            timeout=arguments.get("timeout", 10.0)
        ),
        
        "gothic_take_screenshot": lambda: game_manager.capture_screenshot(
            save_to_file=True,
            filename=arguments.get("filename"),
            return_base64=arguments.get("return_base64", False)
        ),
        
        "gothic_game_status": lambda: game_manager.get_status(),
        
        "gothic_configure_paths": lambda: game_manager.configure_paths(
            gothic_path=arguments.get("gothic_path"),
            launcher_path=arguments.get("launcher_path")
        ),
    }
    
    # Check if this is a game management tool
    if name in game_management_handlers:
        return await asyncio.get_event_loop().run_in_executor(
            None, game_management_handlers[name]
        )
    
    # Pipe-based tools (require game to be running)
    client = get_pipe_client()
    
    pipe_handlers = {
        "gothic_get_status": lambda: client.send_command("getStatus", {}),
        
        "gothic_wait_for_ready": lambda: client.send_command("waitForReady", {
            "timeoutMs": arguments.get("timeout_ms", 30000),
            "pollIntervalMs": arguments.get("poll_interval_ms", 100)
        }, max_retries=10),  # More retries during game startup (up to ~20s connection attempts)
        
        "gothic_get_player_info": lambda: client.send_command("getPlayerInfo", {}),
        
        "gothic_set_player_position": lambda: client.send_command("setPlayerPosition", {
            "x": arguments["x"],
            "y": arguments["y"],
            "z": arguments["z"]
        }),
        
        "gothic_open_inventory": lambda: client.send_command("openInventory", {}),
        
        "gothic_set_camera_angle": lambda: client.send_command("setCameraAngle", {
            "yaw": arguments.get("yaw"),
            "pitch": arguments.get("pitch")
        }),
        
        "gothic_get_pixel_color": lambda: client.send_command("getPixelColor", {
            "x": arguments["x"],
            "y": arguments["y"]
        }),
        
        "gothic_get_d3d9_state": lambda: client.send_command("getD3D9State", {}),
    }
    
    if name not in pipe_handlers:
        return {"error": f"Unknown tool: {name}"}
    
    return await asyncio.get_event_loop().run_in_executor(None, pipe_handlers[name])


async def main():
    """Main entry point for the MCP server."""
    logger.info("Starting Gothic MCP Server...")
    
    # Run the MCP server using stdio transport
    async with stdio_server() as (read_stream, write_stream):
        logger.info("MCP Server running on stdio")
        await app.run(read_stream, write_stream, app.create_initialization_options())


if __name__ == "__main__":
    asyncio.run(main())

