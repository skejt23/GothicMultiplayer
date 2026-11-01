/*
MIT License

Copyright (c) 2026 Gothic Multiplayer Team.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <array>

#include "ZenGin/zGothicAPI.h"

inline bool IsNpcInstance(zCParser* parser, int index) {
  auto* symbol = parser && index >= 0 ? parser->GetSymbol(index) : nullptr;
  const int npc_class = parser ? parser->GetIndex("C_NPC") : -1;
  return symbol && symbol->type == zPAR_TYPE_INSTANCE && npc_class >= 0 && parser->GetBaseClass(symbol) == npc_class;
}

// Native constructors and script calls mutate the shared parser context. A
// network proxy must never steal the hero's instance or leak SELF/OTHER/ITEM.
class ScriptInstanceScope {
public:
  explicit ScriptInstanceScope(zCParser& parser, int instance = -1)
      : bindings_{Save(parser.GetSymbol("SELF")), Save(parser.GetSymbol("OTHER")), Save(parser.GetSymbol("ITEM")),
                  Save(instance >= 0 ? parser.GetSymbol(instance) : nullptr)},
        instance_symbol_(zCPar_Symbol::instance_sym), instance_address_(zCPar_Symbol::instance_adr),
        ai_disabled_(oCNpc::ai_disabled) {}

  ~ScriptInstanceScope() {
    for (const auto& binding : bindings_) {
      if (binding.symbol) {
        binding.symbol->SetOffset(binding.offset);
      }
    }
    zCPar_Symbol::instance_sym = instance_symbol_;
    zCPar_Symbol::instance_adr = instance_address_;
    oCNpc::ai_disabled = ai_disabled_;
  }

  ScriptInstanceScope(const ScriptInstanceScope&) = delete;
  ScriptInstanceScope& operator=(const ScriptInstanceScope&) = delete;

private:
  struct Binding { zCPar_Symbol* symbol; int offset; };
  static Binding Save(zCPar_Symbol* symbol) {
    return symbol && symbol->type == zPAR_TYPE_INSTANCE ? Binding{symbol, symbol->GetOffset()} : Binding{nullptr, 0};
  }
  std::array<Binding, 4> bindings_;
  zCPar_Symbol* instance_symbol_;
  void* instance_address_;
  int ai_disabled_;
};
