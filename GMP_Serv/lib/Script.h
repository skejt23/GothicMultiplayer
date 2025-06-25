#pragma once
#include <vector>

#include "sol/sol.hpp"
class Script {
private:
  sol::state lua;

public:
  Script(std::vector<std::string>);
  ~Script();

private:
  void Init();
  void BindFunctionsAndVariables();
  void LoadScripts(std::vector<std::string>);
  void LoadScript(std::string);
};