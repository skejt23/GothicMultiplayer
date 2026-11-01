
/*
MIT License

Copyright (c) 2025 Gothic Multiplayer Team.

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

#include "menu/menu_scene.h"

namespace menu::scenes {
namespace {

const MenuWeaponBaseline kDefaultWeaponBaseline{
    zVEC3(
		13354.502930f,
		2040.0f,
		-1141.678467f
	),  										   // Camera Position
	0.0f,   									   // Camera Pitch
	210.0f,    									   // Camera Yaw

	zVEC3(
		13346.502930f,
		2006.0f,
		-1240.678467f
	),    									   	   // Weapon Position
	0.0f,   									   // Weapon Yaw
};
constexpr const char* kDefaultWeaponVisual = "ItMw_030_1h_PAL_sword_bastard_RAW_01.3DS";

const MenuSceneSettings kDefaultSceneSettings{
    zVEC3(
		13354.502930f,
		2040.0f,
		-1141.678467f
	),  										   // Camera Position
    0.000000f,                                     // Camera Pitch
    210.0f,                                        // Camera Yaw
    kDefaultWeaponVisual,                          // Weapon Visual
    true,                                          // Show Weapon
    true,                                          // Enable Timelapse
    false,                                         // Freeze Time
    &kDefaultWeaponBaseline,                       // Original positions for the weapon offset
};

const MenuSceneSettings kNewWorldScene01{
    zVEC3(
		31619.121094f,
		4508.782227f,
		2984.011475f
	),
	0.0f,
	182.747147f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene02{
    zVEC3(
		41863.109375f,
		4710.402344f,
		15256.212891f
	),
	0.000000f,
	54.797134f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene03{
    zVEC3(
		70179.828125f,
		4295.717773f,
		-6205.051270f
	),
	354.750000f,
	147.347107f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene04{
    zVEC3(
		68337.726562f,
		4592.324219f,
		16624.369141f
	),
	354.750000f,
	139.097107f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene05{
    zVEC3(
		78442.226562f,
		6470.847656f,
		32084.533203f
	),
	354.750000f,
	213.647125f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene06{
    zVEC3(
		62509.265625f,
		6936.740234f,
		34528.269531f
	),
	354.750000f,
	276.197144f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene07{
    zVEC3(
		36494.265625f,
		7498.957520f,
		32712.416016f
	),
	354.750000f,
	122.747177f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene08{
    zVEC3(
		-3901.210205f,
		5221.858887f,
		-11950.410156f
	),
	24.150017f,
	249.647186f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene09{
    zVEC3(
		30694.166016f,
		4902.337402f,
		-20880.150391f
	),
	15.000046f,
	171.197174f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene10{
    zVEC3(
		25211.849609f,
		3525.285889f,
		-20235.957031f
	),
	357.300049f,
	350.597229f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene11{
    zVEC3(
		-817.782715f,
		4470.246582f,
		15524.096680f
	),
	357.300049f,
	286.847198f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene12{
    zVEC3(
		40574.832031f,
		4362.170410f,
		-2136.614990f
	),
	357.300049f,
	265.697235f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

const MenuSceneSettings kNewWorldScene13{
    zVEC3(
		43353.675781f,
		3643.089600f,
		-23798.738281f
	),
	357.300049f,
	223.847229f,
	kDefaultWeaponVisual,
	true,
	true,
	false,
	&kDefaultWeaponBaseline,
};

}  // namespace
}  // namespace menu::scenes
