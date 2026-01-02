# Gothic Multiplayer (GMP) - Agent Instructions

This document provides essential instructions for AI agents working on the Gothic Multiplayer project. It covers building, installing, and testing the client using the automated workflows and MCP integration.

## 1. Project Overview
- **gmp-client**: The C++ client code (modifies Gothic II via DLL injection).
- **gmp-mcp**: The MCP server that interacts with the running game.
- **xmake**: The build system used for the project.

## 2. Build and Install Workflow
To build the client and install it to the game directory, use the following `xmake` commands.
**Note**: We have an automated workflow for this at `.agent/workflows/build_and_install_client.md`.

### Manual Commands
1. **Build**:
   ```powershell
   xmake b ClientMain
   ```
2. **Install**:
   ```powershell
   xmake install -o "D:\SteamLibrary\steamapps\common\Gothic II" ClientMain
   ```

## 3. Testing and Verification (Gothic MCP)
We use the **Gothic MCP Server** to run the game and verify changes without manual interaction.

### Connection Reliability
The MCP uses Named Pipes for communication. The connection is **persistent** and includes **automatic retry logic** with exponential backoff to handle transient pipe errors (232, 233, 109). If you see occasional connection errors, the system will automatically retry up to 3 times.

### The Development Cycle
1. **Modify Code**: Make necessary changes to the C++ client.
2. **Build & Install**: Run the build/install commands or workflow.
3. **Run Game**: Use the MCP tool `mcp_gothic_start_game` to launch Gothic.
   - **Wait**: Use `mcp_gothic_wait_for_ready` to ensure the game is loaded and the player is spawned.
4. **Verify**: Use various MCP tools to check the state of the game:
   - `mcp_gothic_get_player_info`: Check position, health, mana.
   - `mcp_gothic_open_inventory`: Check if inventory works.
   - `mcp_gothic_take_screenshot`: Visually verify rendering changes.
   - **Browser Agent**: Can be used to open and inspect screenshots (e.g., `file:///path/to/screenshot.png`) for visual verification since the base agent lacks vision.

### Example Test Sequence (Agent Thought Process)
1. "I have updated the health regeneration logic."
2. "I will run `/build_and_install_client` to deploy the changes."
3. "I will call `mcp_gothic_start_game(use_launcher=true)`."
4. "I will call `mcp_gothic_wait_for_ready()`."
5. "I will set health to 1 using `mcp_gothic_set_player_health(health=1)`."
6. "I will wait a few seconds and check `mcp_gothic_get_player_info()` to see if health increased."

## 4. Workflows
These workflows are available in `.agent/workflows/`:
- **build_and_install_client.md**: Automates the build and install process.
