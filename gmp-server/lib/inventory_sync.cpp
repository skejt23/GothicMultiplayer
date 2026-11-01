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

#include "inventory_sync.h"

#include <unordered_set>

#include "script_names.h"
#include "packets.h"

bool ValidateNativeInventoryDeclaration(const std::vector<InventoryEntry>& inventory) {
  if (inventory.size() > kMaxInventoryEntries) return false;
  std::unordered_set<std::string> instances;
  std::size_t name_bytes = 0;
  for (const auto& entry : inventory) {
    if (!gmp::IsValidScriptName(entry.item_instance) || entry.item_instance != gmp::NormalizeScriptName(entry.item_instance) || entry.amount <= 0 || !instances.insert(entry.item_instance).second) return false;
    name_bytes += entry.item_instance.size();
    if (name_bytes > kMaxInventoryNameBytes) return false;
  }
  return true;
}
