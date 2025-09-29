
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

#include "CSpawnPoint.h"

CSpawnPoint::CSpawnPoint(const char *source){
	int a,b,c;
	const char *ptr=(char*)source;
	while(sscanf(ptr, "%d%d%d", &a, &b, &c)==3){
		ptr=strchr(ptr, ' ');
		if(ptr) ptr++;
		if(ptr) ptr=strchr(ptr, ' ')+1;
		if(ptr){
			const char *ptr2=strchr(ptr, 0xA);
			ptr=strchr(ptr, ' ')+1;
			if(((ptr) || (ptr-1)) && (ptr2)){
				if(ptr2<ptr) ptr=ptr2;
			}
		}
		zVEC3 *tmp=new zVEC3(static_cast<float>(a), static_cast<float>(b), static_cast<float>(c));
		data.push_back(tmp);
		if((!ptr) || (!(ptr-1))) return;
	}
}

CSpawnPoint::~CSpawnPoint(){
	for(size_t i=0; i<data.size(); i++) delete data[i];
	data.clear();
}

DWORD CSpawnPoint::GetSize(){ return data.size(); }
zVEC3* CSpawnPoint::operator [](size_t i){ return (i<data.size())?data[i]:NULL; }