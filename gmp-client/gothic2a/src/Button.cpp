
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

//Library: Gothic 2 Wrappers
//Module: Button
//Author: Mecio
// __  __           _       
//|  \/  |         (_)      
//| \  / | ___  ___ _  ___  
//| |\/| |/ _ \/ __| |/ _ \ 
//| |  | |  __/ (__| | (_) |
//|_|  |_|\___|\___|_|\___/ 
//
//Coded for Gothic Multiplayer

#include "Button.h"

using namespace G2W;

Button::Button(int x, int y, int width, int height){
	this->x = x;
	this->y = y;
	this->width = width;
	this->height = height;
	this->highlight = false;
	this->surface = new Gothic_II_Addon::zCView(x,y,x+width,y+height);
}

Button::~Button(void){
	
}

void Button::setText(const char * text){
	this->text = text;
}

void Button::setTexture(const char * texture){
	this->texture = texture;
}

void Button::setHighlightTexture(const char * texture){
	this->highlightTexture = texture;
}

void Button::render(){
	surface = new Gothic_II_Addon::zCView(x,y,x+width,y+height);
	if(highlight){
		surface->InsertBack(Gothic_II_Addon::zSTRING(highlightTexture));
		surface->SetFont(Gothic_II_Addon::zSTRING(highlightFont->texture));
		surface->SetFontColor(Gothic_II_Addon::zCOLOR(highlightFont->r,highlightFont->g,highlightFont->b)); 
	}else{
		surface->InsertBack(Gothic_II_Addon::zSTRING(texture));
		surface->SetFont(Gothic_II_Addon::zSTRING(font->texture));
		surface->SetFontColor(Gothic_II_Addon::zCOLOR(font->r,font->g,font->b)); 
	}
	
	surface->PrintCXY(Gothic_II_Addon::zSTRING(text));
	surface->Render();
	surface->ClrPrintwin();
}

void Button::setFont(const Font & font){
	this->font = &font;
}
void Button::setHighlightFont(const Font & font){
	this->highlightFont = &font;
}