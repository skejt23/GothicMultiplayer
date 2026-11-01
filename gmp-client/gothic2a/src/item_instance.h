#pragma once

#include <string>

#include "ZenGin/zGothicAPI.h"
#include "ZenGin/Gothic_II_Addon/API/zParser_Const.h"
#include "script_names.h"

namespace gmp::gothic {

inline bool IsItemInstanceIndex(int index) {
  auto* parser = Gothic_II_Addon::parser;
  if (!parser || index <= 0 || index >= parser->symtab.GetNumInList()) return false;
  auto* symbol = parser->GetSymbol(index);
  const int item_class = parser->GetIndex("C_ITEM");
  return symbol && symbol->type == Gothic_II_Addon::zPAR_TYPE_INSTANCE &&
         (symbol->flags & Gothic_II_Addon::zPAR_FLAG_CONST) != 0 &&
         (symbol->flags & Gothic_II_Addon::zPAR_FLAG_EXTERNAL) == 0 &&
         item_class >= 0 && parser->GetBaseClass(symbol) == item_class;
}

inline std::string NetworkItemName(int index) {
  if (!IsItemInstanceIndex(index)) return {};
  const auto* name = Gothic_II_Addon::parser->GetSymbol(index)->name.ToChar();
  return name && IsValidScriptName(name) ? NormalizeScriptName(name) : std::string{};
}

inline int LocalItemIndex(const std::string& instance) {
  auto* parser = Gothic_II_Addon::parser;
  if (!parser || !IsValidScriptName(instance)) return 0;
  Gothic_II_Addon::zSTRING name(NormalizeScriptName(instance).c_str());
  const int index = parser->GetIndex(name);
  return IsItemInstanceIndex(index) ? index : 0;
}

}  // namespace gmp::gothic
