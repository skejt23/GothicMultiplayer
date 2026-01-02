"""
Gothic Pipe Client - Named Pipe communication with the Gothic game.

This module handles the Named Pipe connection between the MCP server
and the Gothic game client DLL.
"""

import json
import logging
import time
import uuid
from typing import Any, Optional

# Windows-specific imports for Named Pipes
import os
if os.name == 'nt':
    import win32file
    import win32pipe
    import pywintypes

logger = logging.getLogger(__name__)

# Named Pipe configuration
PIPE_NAME = r'\\.\pipe\GothicMCP'
PIPE_BUFFER_SIZE = 65536
PIPE_TIMEOUT_MS = 5000


class GothicPipeClient:
    """Client for communicating with Gothic game via Named Pipe."""
    
    def __init__(self, pipe_name: str = PIPE_NAME):
        """Initialize the pipe client.
        
        Args:
            pipe_name: The Named Pipe path (default: \\\\.\\pipe\\GothicMCP)
        """
        self.pipe_name = pipe_name
        self._handle: Optional[Any] = None
        self._connected = False
    
    def connect(self) -> bool:
        """Attempt to connect to the Gothic pipe server.
        
        Returns:
            True if connection successful, False otherwise.
        """
        if os.name != 'nt':
            logger.error("Named Pipes are only supported on Windows")
            return False
        
        try:
            # Try to open the named pipe
            self._handle = win32file.CreateFile(
                self.pipe_name,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,  # No sharing
                None,  # Default security
                win32file.OPEN_EXISTING,
                0,  # Default attributes
                None  # No template
            )
            
            # Set pipe to message mode
            win32pipe.SetNamedPipeHandleState(
                self._handle,
                win32pipe.PIPE_READMODE_MESSAGE,
                None,
                None
            )
            
            self._connected = True
            logger.info(f"Connected to Gothic pipe: {self.pipe_name}")
            return True
            
        except pywintypes.error as e:
            logger.warning(f"Failed to connect to Gothic pipe: {e}")
            self._connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Gothic pipe.
        
        Note: The connection is normally kept alive between commands for better
        performance. Only disconnect when done with a session or on errors.
        """
        if self._handle:
            try:
                win32file.CloseHandle(self._handle)
            except:
                pass
            self._handle = None
        self._connected = False
        logger.info("Disconnected from Gothic pipe")
    
    def is_connected(self) -> bool:
        """Check if connected to the pipe."""
        return self._connected and self._handle is not None
    
    def send_command(self, method: str, params: dict[str, Any], max_retries: int = 3) -> dict[str, Any]:
        """Send a command to Gothic and wait for response.
        
        Args:
            method: The command method name
            params: Parameters for the command
            max_retries: Maximum number of retry attempts (default: 3)
            
        Returns:
            Response dictionary from Gothic
        """
        retry_count = 0
        last_error = None
        
        while retry_count <= max_retries:
            # Try to connect if not connected
            if not self.is_connected():
                if not self.connect():
                    if retry_count < max_retries:
                        retry_count += 1
                        backoff = min(0.1 * (2 ** retry_count), 1.0)  # Exponential backoff, max 1s
                        logger.debug(f"Connection failed, retrying in {backoff:.2f}s (attempt {retry_count}/{max_retries})")
                        time.sleep(backoff)
                        continue
                    else:
                        return {
                            "error": "Not connected to Gothic game. Make sure the game is running with MCP support enabled.",
                            "connected": False
                        }
            
            # Create request message
            request_id = str(uuid.uuid4())
            request = {
                "id": request_id,
                "method": method,
                "params": params
            }
            
            try:
                # Send request
                request_json = json.dumps(request)
                logger.debug(f"Sending: {request_json}")
                
                win32file.WriteFile(self._handle, request_json.encode('utf-8'))
                
                # Read response
                result, data = win32file.ReadFile(self._handle, PIPE_BUFFER_SIZE)
                response_json = data.decode('utf-8')
                logger.debug(f"Received: {response_json}")
                
                response = json.loads(response_json)
                
                # Verify response ID matches
                if response.get("id") != request_id:
                    logger.warning(f"Response ID mismatch: expected {request_id}, got {response.get('id')}")
                
                return response
                
            except pywintypes.error as e:
                error_code = e.args[0] if e.args else 0
                
                # Error 232: Pipe is being closed
                # Error 233: No process on the other end
                # Error 109: Broken pipe
                # These are transient errors during pipe reconnection
                if error_code in (232, 233, 109) and retry_count < max_retries:
                    logger.warning(f"Transient pipe error {error_code}: {e}, retrying...")
                    self._connected = False
                    self._handle = None
                    retry_count += 1
                    backoff = min(0.1 * (2 ** retry_count), 1.0)
                    time.sleep(backoff)
                    last_error = e
                    continue
                else:
                    logger.error(f"Pipe communication error: {e}")
                    self._connected = False
                    return {
                        "error": f"Communication error: {e}",
                        "connected": False
                    }
            except json.JSONDecodeError as e:
                logger.error(f"Invalid JSON response: {e}")
                return {
                    "error": f"Invalid response from Gothic: {e}",
                    "connected": True
                }
            except Exception as e:
                logger.error(f"Unexpected error: {e}")
                return {
                    "error": str(e),
                    "connected": self._connected
                }
        
        # If we exhausted all retries
        return {
            "error": f"Failed after {max_retries} retries. Last error: {last_error}",
            "connected": False
        }
    
    def __enter__(self):
        """Context manager entry."""
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.disconnect()
        return False


class MockGothicPipeClient(GothicPipeClient):
    """Mock client for testing without Gothic running."""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._mock_player = {
            "id": 1,
            "name": "Protagonist",
            "position": {"x": 0.0, "y": 0.0, "z": 0.0},
            "health": 100,
            "maxHealth": 100,
            "mana": 50,
            "maxMana": 50,
            "strength": 10,
            "dexterity": 10,
            "level": 1,
            "world": "NEWWORLD.ZEN"
        }
        self._mock_time = {"hour": 8, "minute": 0}
    
    def connect(self) -> bool:
        self._connected = True
        logger.info("Mock pipe client connected")
        return True
    
    def send_command(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        """Handle commands with mock responses."""
        logger.debug(f"Mock command: {method} with {params}")
        
        handlers = {
            "getStatus": lambda: {
                "result": {
                    "connected": True,
                    "gameRunning": True,
                    "inWorld": True,
                    "version": "1.0.0-mock"
                }
            },
            "getPlayerInfo": lambda: {"result": self._mock_player},

        }
        
        handler = handlers.get(method)
        if handler:
            return handler() if callable(handler) else handler
        
        return {"result": {"success": True, "method": method, "params": params}}
    
    def _set_position(self):
        # This would be called from handle_tool_call which passes params
        return {"result": {"success": True}}

def test_connection():
    """Test the pipe connection."""
    print("Testing Gothic MCP Pipe Connection...")
    print(f"Pipe name: {PIPE_NAME}")
    
    client = GothicPipeClient()
    
    if client.connect():
        print("✓ Connected to Gothic!")
        
        # Test getting status
        response = client.send_command("getStatus", {})
        print(f"Status response: {json.dumps(response, indent=2)}")
        
        client.disconnect()
    else:
        print("✗ Could not connect to Gothic.")
        print("  Make sure the game is running with MCP support enabled.")


if __name__ == "__main__":
    test_connection()
