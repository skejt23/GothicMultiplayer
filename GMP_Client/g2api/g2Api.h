
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

#pragma once

//#include "common/osheader.hpp" // OS header
#include "cgamemanager.hpp"
#include "zcinput.hpp"
#include "ocgame.hpp"
#include "zcworld.hpp"
#include "ocnpc.hpp"
#include "zcview.hpp" 
#include "zcmodel.hpp"
#include "zcparser.hpp"
#include "ocobjectfactory.hpp"
#include "ocspawnmanager.hpp"
#include "ocworld.hpp"
#include "ocworldtimer.hpp"
#include "zcinput.hpp"
#include "zccamera.hpp"
#include "ocmob.hpp"
#include "zcaicamera.hpp"
#include "zcskycontroller.hpp"
#include "zcoption.hpp"
#include "ocviewstatusbar.hpp"
#include "zcfont.hpp"
#include "zcclassdef.hpp"
#include "zcmenu.hpp"
#include "zcmover.hpp"
#include "zctexture.hpp"
#include "zcrenderer.hpp"
#include "zcsoundsystem.hpp"
#include "zfile.hpp"
#include "zcparticlefx.hpp"
#include "zceffect.hpp"
#include "ocmsg.hpp"
#include "zceventmanager.hpp"
#include "oczone.hpp"


//definicje przydatne do ladowania swiata
//ADDONWORLD: Jarkendar, OLDWORLD: Gornicza dolina, NEWWORLD: Khorinis
#define ADDONWORLD "ADDON\\ADDONWORLD.ZEN"
#define OLDWORLD "OLDWORLD\\OLDWORLD.ZEN"
#define NEWWORLD "NEWWORLD\\NEWWORLD.ZEN"
#define IRDORATH "NEWWORLD\\DRAGONISLAND.ZEN"
/*
Inne warianty plikow dla prefiksu "ADDON\\":
ADDON_PART_ADANOSTEMPLE_01.ZEN
ADDON_PART_BADITSCAMP_01.ZEN
ADDON_PART_CANYON_01.ZEN
ADDON_PART_ENTRANCE_01.ZEN
ADDON_PART_GOLDMINE_01.ZEN
ADDON_PART_PIRATESCAMP_01.ZEN
ADDON_PART_VALLEY_01.ZEN
Inne warianty plikow dla prefiksu "OLDWORLD\\":
OLDCAMP.ZEN
Inne warianty plikow dla prefiksu "NEWWOLRD\\":
NEWWORLD_PART_CITY_01.ZEN
NEWWORLD_PART_DRAGON_FINAL_01.ZEN
NEWWORLD_PART_DRAGON_ISLAND_01.ZEN
NEWWORLD_PART_FARM_01.ZEN
NEWWORLD_PART_FOREST_01.ZEN
NEWWORLD_PART_GREATPEASANT_01.ZEN
NEWWORLD_PART_MONASTERY_01.ZEN
NEWWORLD_PART_PASS_TO_OW_01.ZEN
NEWWORLD_PART_SHIP_01.ZEN
NEWWORLD_PART_TROLLAREA_01.ZEN
NEWWORLD_PART_XARDAS_01.ZEN
*/