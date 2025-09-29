
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

#include "HooksManager.h"
#include <cstdio>

HooksManager::HooksManager(void)
{
	this->InitAllPatches();
	InitializeCriticalSection(&this->DoneCs);
	InitializeCriticalSection(&this->RenderCs);
	InitializeCriticalSection(&this->CloseLoadScreenCs);
	InitializeCriticalSection(&this->AiMovingCs);
}

HooksManager::~HooksManager(void)
{
}

void HooksManager::AddHook(HOOK_TYPE type, DWORD callback, bool regsAccess)
{
	
	switch(type)
	{
	case HT_RENDER:
		EnterCriticalSection(&this->RenderCs);
		this->OnRenderHooks.insert(std::make_pair(callback, regsAccess));
		LeaveCriticalSection(&this->RenderCs);
		break;
	case HT_CLOSELOADSCREEN:
		EnterCriticalSection(&this->CloseLoadScreenCs);
		this->OnCloseLoadScreenHooks.insert(std::make_pair(callback, regsAccess));
		LeaveCriticalSection(&this->CloseLoadScreenCs);
		break;
	case HT_DONE:
		EnterCriticalSection(&this->DoneCs);
		this->OnDoneHooks.insert(std::make_pair(callback, regsAccess));
		LeaveCriticalSection(&this->DoneCs);
		break;
	case HT_AIMOVING:
		EnterCriticalSection(&this->AiMovingCs);
		this->OnAiMovingHooks.insert(std::make_pair(callback, regsAccess));
		LeaveCriticalSection(&this->AiMovingCs);
		break;
	}
	

}

void HooksManager::RemoveHook(HOOK_TYPE type, DWORD callback)
{

	switch(type)
	{
	case HT_RENDER:
		EnterCriticalSection(&this->RenderCs);
		this->RenderCallbacksToDelete.push_back(callback);
		LeaveCriticalSection(&this->RenderCs);
		break;
	case HT_CLOSELOADSCREEN:
		EnterCriticalSection(&this->CloseLoadScreenCs);
		this->OnCloseLoadScreenHooks.erase(this->OnRenderHooks.find(callback));
		LeaveCriticalSection(&this->CloseLoadScreenCs);
		break;
	case HT_DONE:
		EnterCriticalSection(&this->DoneCs);
		this->OnDoneHooks.erase(this->OnRenderHooks.find(callback));
		LeaveCriticalSection(&this->DoneCs);
		break;
	case HT_AIMOVING:
		EnterCriticalSection(&this->AiMovingCs);
		this->AiMovingCallbacksToDelete.push_back(callback);
		LeaveCriticalSection(&this->AiMovingCs);
		break;
	}
}
void HooksManager::InitAllPatches()
{
	CreateHook(0x6C8AC2, (DWORD)&HooksManager::OnRender, 0, true);
	//CreateHook(0x6C2C26, (DWORD)&HooksManager::OnCloseLoadScreen, 0, true); Nie potrzebne for now
	//CreateHook(0x4254E0, (DWORD)&HooksManager::OnDone, 0, true); Nie potrzebne for now
	CreateHook(0x50E8E1, (DWORD)&HooksManager::OnAiMoving, 0, true);

}
void __stdcall HooksManager::OnRender(sRegs & regs)
{
	HooksManager * hm = HooksManager::GetInstance();
	EnterCriticalSection(&hm->RenderCs);
	HooksMap::iterator end = hm->OnRenderHooks.end(); 
	for (HooksMap::iterator it = hm->OnRenderHooks.begin(); it != end; ++it)
	{
		if(it->second)
		{
			typedef void (*fptr)(int);
			fptr p = (fptr)(it->first);
			p(regs.ECX);
		}else{
			typedef void (*fptr)(void);
			fptr p = (fptr)(it->first);
			p();
		}
	}
	if(hm->RenderCallbacksToDelete.size() > 0) 
	{
		for(int i = 0; i < (int)hm->RenderCallbacksToDelete.size(); i++){
			hm->OnRenderHooks.erase(hm->OnRenderHooks.find(hm->RenderCallbacksToDelete[i]));
			hm->RenderCallbacksToDelete.erase(hm->RenderCallbacksToDelete.begin()+i);
		}
	}
	LeaveCriticalSection(&hm->RenderCs);
}
void __stdcall HooksManager::OnCloseLoadScreen(sRegs & regs)
{
	HooksManager * hm = HooksManager::GetInstance();
	EnterCriticalSection(&hm->CloseLoadScreenCs);
	HooksMap::iterator end = hm->OnCloseLoadScreenHooks.end(); 
	for (HooksMap::iterator it = hm->OnCloseLoadScreenHooks.begin(); it != end; ++it)
	{

		if(it->second)
		{
			typedef void (*fptr)(int);
			fptr p = (fptr)(it->first);
			p(regs.ECX);
		}else{
			typedef void (*fptr)(void);
			fptr p = (fptr)(it->first);
			p();
		}

	}
	LeaveCriticalSection(&hm->CloseLoadScreenCs);

}
void __stdcall HooksManager::OnDone(sRegs & regs)
{
	HooksManager * hm = HooksManager::GetInstance();
	EnterCriticalSection(&hm->DoneCs);
	HooksMap::iterator end = hm->OnDoneHooks.end(); 
	for (HooksMap::iterator it = hm->OnDoneHooks.begin(); it != end; ++it)
	{

		if(it->second)
		{
			typedef void (*fptr)(int);
			fptr p = (fptr)(it->first);
			p(regs.ECX);
		}else{
			typedef void (*fptr)(void);
			fptr p = (fptr)(it->first);
			p();
		}
	}
	LeaveCriticalSection(&hm->DoneCs);
}
void __stdcall HooksManager::OnAiMoving(sRegs & regs)
{
	HooksManager * hm = HooksManager::GetInstance();
	EnterCriticalSection(&hm->AiMovingCs);
	HooksMap::iterator end = hm->OnAiMovingHooks.end(); 
	for (HooksMap::iterator it = hm->OnAiMovingHooks.begin(); it != end; ++it)
	{

		if(it->second)
		{
			typedef void (*fptr)(int);
			fptr p = (fptr)(it->first);
			p(regs.ECX);
		}else{
			typedef void (*fptr)(void);
			fptr p = (fptr)(it->first);
			p();
		}

	}
	if(hm->AiMovingCallbacksToDelete.size() > 0) 
	{
		for(int i = 0; i < (int)hm->AiMovingCallbacksToDelete.size(); i++){
			hm->OnAiMovingHooks.erase(hm->OnAiMovingHooks.find(hm->AiMovingCallbacksToDelete[i]));
			hm->AiMovingCallbacksToDelete.erase(hm->AiMovingCallbacksToDelete.begin()+i);
		}
	}
	LeaveCriticalSection(&hm->AiMovingCs);

}