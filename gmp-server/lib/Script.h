#pragma once
#include <vector>

#include "sol/sol.hpp"

#include "Lua/timer_manager.h"

class Script {
private:
  sol::state lua;
  TimerManager timer_manager_;

public:
  Script(std::vector<std::string>);
  ~Script();

  void ProcessTimers();
  TimerManager& GetTimerManager();

private:
  void Init();
  void BindFunctionsAndVariables();
  void LoadScripts(std::vector<std::string>);
  void LoadScript(std::string);
};