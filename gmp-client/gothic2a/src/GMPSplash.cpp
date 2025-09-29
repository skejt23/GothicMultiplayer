
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

#include "common.h"
#include "GMPSplash.h"


#define PICTURE_WIDTH 514
#define PICTURE_HEIGHT 228
HINSTANCE hInst = NULL;
const static char * GMPModuleName = "GMP.dll";
const static char * WndClassName = "GmpSplash";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_CREATE:
		
		HRSRC RsrcHand; 
		RsrcHand = FindResourceA(GetModuleHandleA(GMPModuleName), "RANGE", "RGN"); 
		HGLOBAL RsrcPoint;
		RsrcPoint = LoadResource(GetModuleHandleA(GMPModuleName), RsrcHand);
		DWORD RsrcSize;
		RsrcSize = SizeofResource(GetModuleHandleA(GMPModuleName), RsrcHand);
		RsrcPoint = LockResource(RsrcPoint);
		SetWindowRgn(hWnd, ExtCreateRegion(NULL, RsrcSize, (LPRGNDATA)RsrcPoint), true);
			  break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
			
			SendMessage(hWnd, WM_DESTROY, wParam, lParam);
			break;
	default: return DefWindowProcA(hWnd, message, wParam, lParam);
	}
	return NULL;
}

DWORD __stdcall SplashFunc(void * param)
{
	hInst = GetModuleHandleA(NULL);
	WNDCLASSEXA wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW |CS_VREDRAW |CS_BYTEALIGNWINDOW;
	wc.hInstance = hInst;
	wc.cbClsExtra = NULL;
	wc.cbWndExtra = NULL;
	HBITMAP bitmap = LoadBitmapA(GetModuleHandleA(GMPModuleName), MAKEINTRESOURCEA(1000));
	wc.hbrBackground = CreatePatternBrush(bitmap);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WndClassName;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIconSm = NULL;
	wc.lpfnWndProc = WndProc;

	RegisterClassExA(&wc);
	
	
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);
	int wndposx = (screenWidth/2) - (PICTURE_WIDTH/2);
	int wndposy = (screenHeight/2) - (PICTURE_HEIGHT/2);

	HWND hwindow = CreateWindowExA(WS_EX_LEFT|WS_EX_TOOLWINDOW, WndClassName, "", WS_POPUP, wndposx, wndposy, PICTURE_WIDTH, PICTURE_HEIGHT, 0, 0, hInst, 0);
	ShowWindow(hwindow, SW_SHOWNORMAL);
	UpdateWindow(hwindow);

	MSG msg;
	while(GetMessage( &msg, 0, 0, 0 ))
	{ 
		DispatchMessage(&msg); 
	}
	return NULL;
};