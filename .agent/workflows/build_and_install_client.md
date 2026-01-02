---
description: Build the GMP Client and install it to the Gothic II directory
---
This workflow builds the ClientMain target using xmake and installs the resulting DLL (and dependencies) to the configured Gothic II game directory.

1. Build the ClientMain target
// turbo
```powershell
xmake b ClientMain
```

2. Install to the game directory
// turbo
```powershell
xmake install -o "D:\SteamLibrary\steamapps\common\Gothic II" ClientMain
```
