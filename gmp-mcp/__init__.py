"""
Gothic MCP Server Package

This package provides an MCP (Model Context Protocol) server for controlling
the Gothic game through AI assistants like Claude/Antigravity.
"""

from .gothic_pipe_client import GothicPipeClient, MockGothicPipeClient, PIPE_NAME
from .gothic_game_manager import GothicGameManager, get_game_manager
from .gothic_mcp_server import app as mcp_app

__version__ = "1.0.0"
__all__ = [
    "GothicPipeClient",
    "MockGothicPipeClient", 
    "PIPE_NAME",
    "GothicGameManager",
    "get_game_manager",
    "mcp_app",
]
