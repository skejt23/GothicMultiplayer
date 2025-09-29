
/*
MIT License

Copyright (c) 2022 Gothic Multiplayer Team (pampi, skejt23, mecio)

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

// Library: Gothic 2 Wrappers
// Module: Table
// Author: Mecio
//  __  __           _
//|  \/  |         (_)
//| \  / | ___  ___ _  ___
//| |\/| |/ _ \/ __| |/ _ \ 
//| |  | |  __/ (__| | (_) |
//|_|  |_|\___|\___|_|\___/
//
// Coded for Gothic Multiplayer

#pragma once

#include <string>
#include <vector>

#include "Font.h"
#include "ZenGin/zGothicAPI.h"

namespace G2W {

struct TableRow {
  std::vector<std::string> values;
  bool highlight;
};

class Table {
public:
  Table(int x, int y, int width, int height, int interline = 200, int visibleRows = 30);
  void setBackground(const char* texture);
  void addColumn(const char* name, int width);
  void addRow(TableRow row);
  void clear();
  void render();
  void scrollUp(unsigned int val);
  void scrollDown(unsigned int val);
  ~Table(void);
  void setFont(const G2W::Font& font);
  void setHighlightFont(const G2W::Font& font);
  void hihghightAll();
  void unHighlightAll();
  std::vector<TableRow> rows;
  unsigned int visibleRows;

private:
  Gothic_II_Addon::zCView* surface;
  const Font *font, *highlightFont;
  struct sColumn {
    const char* name;
    int width;
  };
  const char* background;
  int x, y, width, height, interline;
  unsigned int scroll;
  std::vector<sColumn> columns;
};

}  // namespace G2W
