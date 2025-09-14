
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

#include "Table.h"

#include <stdio.h>

using namespace G2W;

Table::Table(int x, int y, int width, int height, int interline, int visibleRows) {
  this->x = x;
  this->y = y;
  this->width = width;
  this->height = height;
  this->interline = interline;
  this->visibleRows = visibleRows;
  this->scroll = 0;
  this->surface = new Gothic_II_Addon::zCView(x, y, x + width, y + height);
}

Table::~Table(void) {
}

void Table::addColumn(const char* name, int width) {
  sColumn c = {name, width};
  columns.push_back(c);
}

void Table::addRow(TableRow row) {
  rows.push_back(row);
}

void Table::clear() {
  rows.clear();
}

void Table::scrollUp(unsigned int val) {
  if (scroll > 0)
    scroll -= val;
}
void Table::scrollDown(unsigned int val) {
  if (scroll < rows.size() - 1)
    scroll += val;
}

void Table::render() {
  surface = new Gothic_II_Addon::zCView(x, y, x + width, y + height);

  surface->InsertBack(background);
  int px = 0, py = 0;
  for (unsigned int i = 0; i < columns.size(); i++) {
    surface->Print(px, py, columns[i].name);
    px += columns[i].width;
  }
  for (unsigned int i = scroll; i < rows.size(); i++) {
    py += interline;
    px = 0;
    if (rows[i].highlight) {
      surface->SetFont(Gothic_II_Addon::zSTRING(highlightFont->texture));
      surface->SetFontColor(Gothic_II_Addon::zCOLOR(highlightFont->r, highlightFont->g, highlightFont->b));

    } else {
      surface->SetFont(Gothic_II_Addon::zSTRING(font->texture));
      surface->SetFontColor(Gothic_II_Addon::zCOLOR(font->r, font->g, font->b));
    }

    for (unsigned int j = 0; (j < rows[i].values.size() && j < columns.size()); j++) {
      surface->Print(px, py, rows[i].values[j].c_str());
      px += columns[j].width;
    }
    if (i - scroll >= visibleRows)
      break;
  }

  surface->Render();
  surface->ClrPrintwin();
}

void Table::setBackground(const char* texture) {
  this->background = texture;
}

void Table::setFont(const Font& font) {
  this->font = &font;
}
void Table::setHighlightFont(const Font& font) {
  this->highlightFont = &font;
}

void Table::hihghightAll() {
  for (unsigned int i = 0; i < rows.size(); i++) {
    rows[i].highlight = true;
  }
}

void Table::unHighlightAll() {
  for (unsigned int i = 0; i < rows.size(); i++) {
    rows[i].highlight = false;
  }
}