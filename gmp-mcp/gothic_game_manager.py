"""
Gothic Game Manager - Start, stop, and capture screenshots of the Gothic game.

This module handles game process management and screenshot capture entirely in Python,
without requiring any C++ changes.
"""

import base64
import io
import logging
import os
import subprocess
import time
from datetime import datetime
from pathlib import Path
from typing import Optional, Tuple

import psutil

logger = logging.getLogger(__name__)

# Default paths - can be configured
DEFAULT_GOTHIC_PATH = r"D:\SteamLibrary\steamapps\common\Gothic II\system\Gothic.exe"
DEFAULT_GMP_LAUNCHER_PATH = r"D:\SteamLibrary\steamapps\common\Gothic II\Multiplayer\GMPLauncher.exe"
SCREENSHOT_DIR = Path(__file__).parent / "screenshots"


class GothicGameManager:
    """Manages the Gothic game process - starting, stopping, and capturing screenshots."""
    
    # Process names to look for
    GOTHIC_PROCESS_NAMES = ["gothic2.exe", "gothic.exe", "gothicii.exe"]
    
    def __init__(
        self,
        gothic_path: Optional[str] = None,
        launcher_path: Optional[str] = None,
        screenshot_dir: Optional[Path] = None
    ):
        """Initialize the game manager.
        
        Args:
            gothic_path: Path to Gothic2.exe (optional)
            launcher_path: Path to GMPLauncher.exe (optional)
            screenshot_dir: Directory to save screenshots (optional)
        """
        self.gothic_path = gothic_path or DEFAULT_GOTHIC_PATH
        self.launcher_path = launcher_path or DEFAULT_GMP_LAUNCHER_PATH
        self.screenshot_dir = screenshot_dir or SCREENSHOT_DIR
        
        # Ensure screenshot directory exists
        self.screenshot_dir.mkdir(parents=True, exist_ok=True)
        
        self._game_process: Optional[subprocess.Popen] = None
    
    def find_gothic_process(self) -> Optional[psutil.Process]:
        """Find a running Gothic process.
        
        Returns:
            psutil.Process if found, None otherwise.
        """
        for proc in psutil.process_iter(['name', 'pid']):
            try:
                name = proc.info['name'].lower()
                if name in self.GOTHIC_PROCESS_NAMES:
                    return proc
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        return None
    
    def is_running(self) -> bool:
        """Check if Gothic is currently running.
        
        Returns:
            True if Gothic is running, False otherwise.
        """
        return self.find_gothic_process() is not None
    
    def get_status(self) -> dict:
        """Get detailed status of the Gothic game.
        
        Returns:
            Dictionary with status information.
        """
        proc = self.find_gothic_process()
        
        if proc is None:
            return {
                "running": False,
                "pid": None,
                "memory_mb": None,
                "cpu_percent": None,
                "runtime_seconds": None
            }
        
        try:
            with proc.oneshot():
                memory_info = proc.memory_info()
                create_time = proc.create_time()
                runtime = time.time() - create_time
                
                return {
                    "running": True,
                    "pid": proc.pid,
                    "memory_mb": round(memory_info.rss / (1024 * 1024), 2),
                    "cpu_percent": proc.cpu_percent(interval=0.1),
                    "runtime_seconds": round(runtime, 1)
                }
        except (psutil.NoSuchProcess, psutil.AccessDenied) as e:
            logger.warning(f"Error getting process info: {e}")
            return {
                "running": False,
                "pid": None,
                "error": str(e)
            }
    
    def start_game(
        self,
        # use_launcher: bool = False,  # Commented out - always use Gothic.exe directly
        wait_for_window: bool = True,
        timeout: float = 30.0
    ) -> dict:
        """Start the Gothic game.
        
        Args:
            wait_for_window: If True, wait for the game window to appear.
            timeout: Maximum time to wait for the game to start (seconds).
        
        Returns:
            Dictionary with result information.
        """
        # Check if already running
        if self.is_running():
            proc = self.find_gothic_process()
            return {
                "success": False,
                "error": "Gothic is already running",
                "pid": proc.pid if proc else None
            }
        
        # Always use Gothic.exe directly (launcher option commented out)
        exe_path = self.gothic_path
        # if use_launcher:
        #     exe_path = self.launcher_path
        #     if not os.path.exists(exe_path):
        #         # Fallback to the Multiplayer directory beside System
        #         gothic_dir = os.path.dirname(self.gothic_path)
        #         exe_path = os.path.join(os.path.dirname(gothic_dir), "Multiplayer", "GMPLauncher.exe")
        # else:
        #     exe_path = self.gothic_path
        
        if not os.path.exists(exe_path):
            return {
                "success": False,
                "error": f"Executable not found: {exe_path}"
            }
        
        try:
            # Get the working directory (should be the System folder)
            work_dir = os.path.dirname(exe_path)
            
            logger.info(f"Starting Gothic: {exe_path}")
            
            # Start the process
            # Redirect stdout/stderr to DEVNULL to prevent output from corrupting MCP stdio protocol
            self._game_process = subprocess.Popen(
                [exe_path],
                cwd=work_dir,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                stdin=subprocess.DEVNULL,
                creationflags=subprocess.CREATE_NEW_PROCESS_GROUP
            )
            
            # Wait for the game to actually start
            if wait_for_window:
                start_time = time.time()
                while time.time() - start_time < timeout:
                    if self.is_running():
                        # Give it a moment to initialize
                        time.sleep(1.0)
                        proc = self.find_gothic_process()
                        return {
                            "success": True,
                            "pid": proc.pid if proc else self._game_process.pid,
                            "message": "Gothic started successfully"
                        }
                    time.sleep(0.5)
                
                return {
                    "success": False,
                    "error": f"Game did not start within {timeout} seconds"
                }
            
            return {
                "success": True,
                "pid": self._game_process.pid,
                "message": "Gothic start initiated"
            }
            
        except Exception as e:
            logger.error(f"Failed to start Gothic: {e}")
            return {
                "success": False,
                "error": str(e)
            }
    
    def stop_game(self, force: bool = False, timeout: float = 10.0) -> dict:
        """Stop the Gothic game.
        
        Args:
            force: If True, forcefully terminate. If False, try graceful shutdown first.
            timeout: Time to wait for graceful shutdown before forcing.
        
        Returns:
            Dictionary with result information.
        """
        proc = self.find_gothic_process()
        
        if proc is None:
            return {
                "success": True,
                "message": "Gothic was not running"
            }
        
        pid = proc.pid
        
        try:
            if force:
                # Force kill immediately
                proc.kill()
                proc.wait(timeout=5.0)
                return {
                    "success": True,
                    "message": f"Gothic (PID {pid}) forcefully terminated"
                }
            
            # Try graceful termination first
            proc.terminate()
            
            try:
                proc.wait(timeout=timeout)
                return {
                    "success": True,
                    "message": f"Gothic (PID {pid}) terminated gracefully"
                }
            except psutil.TimeoutExpired:
                # Force kill if graceful shutdown didn't work
                logger.warning(f"Graceful shutdown timed out, forcing kill")
                proc.kill()
                proc.wait(timeout=5.0)
                return {
                    "success": True,
                    "message": f"Gothic (PID {pid}) forcefully terminated after timeout"
                }
                
        except Exception as e:
            logger.error(f"Failed to stop Gothic: {e}")
            return {
                "success": False,
                "error": str(e),
                "pid": pid
            }
    
    def get_gothic_window(self) -> Optional[Tuple[int, int, int, int]]:
        """Get the Gothic window position and size.
        
        Returns:
            Tuple of (left, top, width, height) or None if not found.
        """
        try:
            import win32gui
            import ctypes
            
            # Enable DPI awareness to get correct coordinates
            try:
                ctypes.windll.shcore.SetProcessDpiAwareness(2) # PROCESS_PER_MONITOR_DPI_AWARE
            except Exception:
                pass # access denied or older OS
            
            def callback(hwnd, windows):
                if win32gui.IsWindowVisible(hwnd):
                    title = win32gui.GetWindowText(hwnd).lower()
                    
                    if "gothic" in title:
                        class_name = win32gui.GetClassName(hwnd)
                        
                        # Exclude file explorer windows
                        if class_name == "CabinetWClass":
                            return True
                            
                        # Accept known classes OR generic if title is strong match
                        # "DIEmWin" is standard ZenGin, "SDL_app" is SDL2
                        known_classes = ["DIEmWin", "GothicWindow", "SDL_app"]
                        
                        if class_name in known_classes or "gothic" in title:
                            # Verify it's not a console window or zero size
                            rect = win32gui.GetWindowRect(hwnd)
                            if rect[2] - rect[0] > 0 and rect[3] - rect[1] > 0:
                                windows.append((hwnd, rect))
                                
                return True
            
            windows = []
            win32gui.EnumWindows(callback, windows)
            
            if windows:
                # Return the first found window
                hwnd, rect = windows[0]
                left, top, right, bottom = rect
                
                # If minimized, restore it?? No, just report what we have.
                # Adjust for borders if needed? GetClientRect might be better but GetWindowRect is standard for screenshot.
                
                return (left, top, right - left, bottom - top)
            
            logger.warning("No Gothic window found via EnumWindows")
            return None
            
        except ImportError:
            logger.error("pywin32 not installed. Cannot detect window position. Install with: pip install pywin32")
            return None
        except Exception as e:
            logger.warning(f"Could not get Gothic window: {e}")
            return None
    
    def capture_screenshot(
        self,
        save_to_file: bool = True,
        filename: Optional[str] = None,
        return_base64: bool = False
    ) -> dict:
        """Capture a screenshot of the Gothic game window.
        
        Args:
            save_to_file: If True, save the screenshot to a file.
            filename: Custom filename (without extension). Auto-generated if None.
            return_base64: If True, include base64-encoded image in response.
        
        Returns:
            Dictionary with screenshot information.
        """
        try:
            import mss
            from PIL import Image
            
            # Check if Gothic is running
            if not self.is_running():
                return {
                    "success": False,
                    "error": "Gothic is not running"
                }
            
            # Try to get the Gothic window position
            window_rect = self.get_gothic_window()
            
            with mss.mss() as sct:
                if window_rect:
                    # Capture specific window
                    left, top, width, height = window_rect
                    monitor = {
                        "left": left,
                        "top": top,
                        "width": width,
                        "height": height
                    }
                    logger.info(f"Capturing Gothic window at {window_rect}")
                else:
                    # Fallback to primary monitor
                    monitor = sct.monitors[1]  # Primary monitor
                    logger.info("Gothic window not found, capturing primary monitor")
                
                # Capture the screen
                screenshot = sct.grab(monitor)
                
                # Convert to PIL Image
                img = Image.frombytes("RGB", screenshot.size, screenshot.bgra, "raw", "BGRX")
            
            result = {
                "success": True,
                "width": img.width,
                "height": img.height,
                "captured_window": window_rect is not None
            }
            
            # Save to file if requested
            if save_to_file:
                if filename is None:
                    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                    filename = f"gothic_screenshot_{timestamp}"
                
                filepath = self.screenshot_dir / f"{filename}.png"
                img.save(filepath, "PNG")
                result["filepath"] = str(filepath)
                logger.info(f"Screenshot saved to {filepath}")
            
            # Include base64 if requested
            if return_base64:
                buffer = io.BytesIO()
                img.save(buffer, format="PNG")
                buffer.seek(0)
                result["base64"] = base64.b64encode(buffer.read()).decode("utf-8")
            
            return result
            
        except ImportError as e:
            return {
                "success": False,
                "error": f"Missing dependency: {e}. Install with: pip install mss Pillow"
            }
        except Exception as e:
            logger.error(f"Failed to capture screenshot: {e}")
            return {
                "success": False,
                "error": str(e)
            }
    
    def configure_paths(
        self,
        gothic_path: Optional[str] = None,
        launcher_path: Optional[str] = None
    ) -> dict:
        """Configure the paths to Gothic executables.
        
        Args:
            gothic_path: Path to Gothic2.exe
            launcher_path: Path to GMPLauncher.exe
        
        Returns:
            Dictionary with current configuration.
        """
        if gothic_path:
            self.gothic_path = gothic_path
        if launcher_path:
            self.launcher_path = launcher_path
        
        return {
            "gothic_path": self.gothic_path,
            "gothic_exists": os.path.exists(self.gothic_path),
            "launcher_path": self.launcher_path,
            "launcher_exists": os.path.exists(self.launcher_path),
            "screenshot_dir": str(self.screenshot_dir)
        }


# Global instance
_game_manager: Optional[GothicGameManager] = None


def get_game_manager() -> GothicGameManager:
    """Get the global game manager instance."""
    global _game_manager
    if _game_manager is None:
        _game_manager = GothicGameManager()
    return _game_manager
