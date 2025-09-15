
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

#include "CActiveAniID.h"

#include "ZenGin/zGothicAPI.h"

CActiveAniID *pActiveAniID = NULL;

CActiveAniID *CActiveAniID::GetInstance() {
  return pActiveAniID;
}

CActiveAniID::CActiveAniID() {
  pActiveAniID = this;
}

CActiveAniID::~CActiveAniID() {
  pActiveAniID = NULL;
}

void CActiveAniID::AddAni(int id) {
  if (this->animation_stack.empty())
    this->animation_stack.push(id);
  else if (id != this->animation_stack.top())
    this->animation_stack.push(id);
}

void CActiveAniID::ValidateStack() {
  if (this->animation_stack.size() > 100)
    for (int i = 0; i < 100; i++) this->animation_stack.pop();
func_validate_ani_start:
  if (!this->animation_stack.empty())
    if (!player->GetModel()->IsAnimationActive(player->GetModel()->GetAniFromAniID(this->animation_stack.top())->GetAniName())) {
      this->animation_stack.pop();
      goto func_validate_ani_start;
    }
}

int CActiveAniID::GetAniID() {
  ValidateStack();
  if (this->animation_stack.empty())
    return 265;
  return this->animation_stack.top();
}