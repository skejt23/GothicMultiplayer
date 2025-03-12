
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

/*****************************************************************************
**																			**
**	File name:		Interface/CMainMenu.cpp		   							**
**																			**
**	Created by:		06/06/11	-	skejt23									**
**																			**
**	Description:	Multiplayer main menu functionallity	 				**
**																			**
*****************************************************************************/

#include "CGmpClient.h"
#include "CLanguage.h"
#include "keyboard.h"
#include "CMainMenu.h"
#include "patch.h"
#include "mod.h"
#include "CWatch.h"
#include "interface.h"
#include <urlmon.h>
#include "git.h"
#include "CSyncFuncs.h"
#include "WorldBuilder\CBuilder.h"
#include "ExtendedServerList.h"
#include <spdlog/spdlog.h>

extern CGmpClient *client;
extern zSTRING GlobalFont;
extern CConfig* user_config;
extern std::vector<zSTRING> vec_choose_lang;
extern std::vector<std::string> vec_lang_files;
extern const char* LANG_DIR;
zSTRING string_tmp = "ItMw_1h_Mil_Sword";
extern float fWRatio, fHRatio;
zCOLOR Normal = zCOLOR(255,255,255);
zCOLOR Highlighted = zCOLOR(128,180,128);
zCOLOR Red = zCOLOR(0xFF, 0, 0);
extern zCOLOR Green;
zCOLOR FColor;
zSTRING FDefault = "FONT_DEFAULT.TGA";
zSTRING WalkAnim = "S_WALKL";
CLanguage* Lang;
zCView* Screen;
zCView* PrintTimedScreen;
zCInput* Input;
CBuilder* Builder;
ifstream g2names;
ifstream g2particles;
zSTRING News = "News";
zSTRING HOWTOWB = "F1 - World Builder";
zSTRING WRITE_MAPNAME = "Write ZEN world name ex. newworld.zen:";
zSTRING WRITE_SAVEDMAP = "Write saved map name:";
zSTRING MAPFILE_EMPTY = "Map file doesn't exist!";
#define RSS_FILE ""
#define RSS_URL_ADDON ""

int Language;
char x[2]={0, 0};

	CMainMenu::CMainMenu()
	{
		PrintTimedScreen = zCView::GetScreen()->GetPrintScreen();
		oCNpc::GetHero()->SetMovLock(1);
		Patch::PlayerInterfaceEnabled(false);
		Screen = zCView::GetScreen();
		Input = zCInput::GetInput();
		LoadConfig();
		ScreenResolution.x = zCOption::GetOption()->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", 320);
		ScreenResolution.y = zCOption::GetOption()->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenY", 258); 
		MenuItems = 4;
		hbX, hbY, ps = 0, MenuPos = 0, OptionPos = 0, WBMenuPos = 0;
		RECT wymiary;
		GetWindowRect(Patch::GetHWND(), &wymiary);
		fWRatio=1280.0f/(float)wymiary.right;	//zalozenie jest takie ze szerokosc jest dopasowywana weddug szerokosci 1280
		fHRatio=1024.0f/(float)wymiary.top;
		DisableHealthBar();
		GMPLogo = new zCView(0,0,8192,8192,VIEW_ITEM);
		GMPLogo->SetPos(4000,200);
		GMPLogo->InsertBack(zSTRING("GMP_LOGO_MENU.TGA"));
		Screen->InsertItem(GMPLogo);
		GMPLogo->SetSize(5500,2000);
		TitleWeaponEnabled = false;
		AppCamCreated = false;
		WritingNickname = false;
		AppWeapon = NULL;
		LangSetting = NULL;
		Options = NULL;
		MState = CHOOSE_LANGUAGE;
		
		oCSpawnManager::SetRemoveRange(2097152.0f);
		ServerList=new CServerList();
		LaunchMenuScene();
		oCGame::GetGame()->GetWorldTimer()->GetTime(Hour, Minute);
		HooksManager * hm = HooksManager::GetInstance();
		hm->AddHook(HT_RENDER, (DWORD)MainMenuLoop, false);
		hm->AddHook(HT_RENDER, (DWORD)CSelectClass::Loop, false);
		hm->RemoveHook(HT_AIMOVING, (DWORD)Initialize);
		ClassSelect=NULL;
		HeroPos = oCNpc::GetHero()->GetPosition();
		Angle = oCNpc::GetHero()->GetAngle();
		NAngle = oCNpc::GetHero()->GetAngleN();
		ClearNpcTalents(oCNpc::GetHero());
		zCSoundFX* FXMusic;
		if(Christmas) FXMusic = zCSoundSystem::GetSoundSystem()->LoadSoundFX("XMAS.WAV");
		else FXMusic = zCSoundSystem::GetSoundSystem()->LoadSoundFX("K_KURKOWSKI_A_CERTAIN_PLACE.WAV");
		FXMusic->SetLooping(1);
		MusicId = zCSoundSystem::GetSoundSystem()->PlaySound(FXMusic, 1);
	};

	CMainMenu::~CMainMenu()
	{
		delete esl;
		CleanUpMainMenu();
	};
	
	static void ReLaunchPart2()
	{
		HooksManager::GetInstance()->RemoveHook(HT_AIMOVING, (DWORD)ReLaunchPart2);
		oCNpc* Hero = oCNpc::GetHero();
		CMainMenu* ReMenu = CMainMenu::GetInstance();
		ReMenu->ClearNpcTalents(Hero);
		ReMenu->ps = MAIN_MENU;
		ReMenu->MState = MENU_LOOP;
		Hero->SetMovLock(1);
		Patch::PlayerInterfaceEnabled(false);
		if(Hero->GetEquippedArmor()) Hero->UnequipItem(Hero->GetEquippedArmor());
		if(Hero->GetEquippedRangedWeapon()) Hero->UnequipItem(Hero->GetEquippedRangedWeapon());
		if(Hero->GetEquippedMeleeWeapon()) Hero->UnequipItem(Hero->GetEquippedMeleeWeapon());
		if(Hero->GetRightHand()){
			zCVob* Ptr = Hero->GetRightHand();
			zCVob* PtrLeft = Hero->GetLeftHand();
			Hero->DropAllInHand();
			if(PtrLeft) PtrLeft->RemoveVobFromWorld();
			Ptr->RemoveVobFromWorld();
		}
		Hero->GetModel()->StartAnimation("S_RUN");
		Hero->SetWeaponMode(NPC_WEAPON_NONE);
		Hero->GetInventory()->ClearInventory();
		ReMenu->DisableHealthBar();
		ReMenu->GMPLogo = new zCView(0,0,8192,8192,VIEW_ITEM);
		ReMenu->GMPLogo->SetPos(4000,200);
		ReMenu->GMPLogo->InsertBack(zSTRING("GMP_LOGO_MENU.TGA"));
		Screen->InsertItem(ReMenu->GMPLogo);
		ReMenu->GMPLogo->SetSize(5500,2000);
		HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CMainMenu::MainMenuLoop, false);
		ReMenu->TitleWeaponEnabled = false;
		ReMenu->AppCamCreated = false;
		ReMenu->WritingNickname = false;
		ReMenu->LaunchMenuScene();
		HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)CSelectClass::Loop, false);
	};

	void CMainMenu::ReLaunchMainMenu()
	{
		zCInput::GetInput()->ClearKeyBuffer();
		oCGame* Game = oCGame::GetGame();
		if(!memcmp("NEWWORLD\\NEWWORLD.ZEN", Game->GetWorld()->GetWorldName().ToChar(), 21)){} // jest mapa
		else {
			Patch::ChangeLevelEnabled(true);
			oCGame::GetGame()->ChangeLevel("NEWWORLD\\NEWWORLD.ZEN", zSTRING("????"));
			Patch::ChangeLevelEnabled(false);
		}
		HooksManager::GetInstance()->AddHook(HT_AIMOVING, (DWORD)ReLaunchPart2, false);
	};

	void CMainMenu::EnableHealthBar()
	{
		oCGame::GetGame()->GetHealthBar()->SetSize(hbX,hbY);
	};
	void CMainMenu::DisableHealthBar()
	{
		oCGame::GetGame()->GetHealthBar()->GetSize(hbX,hbY);
		oCGame::GetGame()->GetHealthBar()->SetSize(0,0);
	};

	void CMainMenu::PrintNews()
	{
		if(RssNews.size() > 0){
			zCView* Screen = zCView::GetScreen();
			Screen->SetFont(FDefault);
			Screen->SetFontColor(Green);
			Screen->Print(200,0, News);
			Screen->SetFontColor(Normal);
			int Pos = 200;
			for (int i = 0; i < (int)RssNews.size(); i++){
				Screen->Print(200, Pos, RssNews[i]);
				Pos+=200;
			}
		}
	};

	char* Convert(const wchar_t* wString){
		size_t len=wcslen(wString);
		char *szString=new char[len+1];
		memset(szString, 0, len+1);
		for(size_t i=0; i<len; i++) wctomb(szString+i, wString[i]);
		return szString;
	}

	void CMainMenu::LoadLangNames(void)
	{
		std::string indexPath = std::string(LANG_DIR) + "index";
		std::ifstream ifs(indexPath, std::ifstream::in);
		if (!ifs.is_open())
		{
			SPDLOG_ERROR("Couldn't find language index file {}!", indexPath);
			std::abort();
		}

		std::string line;
		while (std::getline(ifs, line))
		{
			vec_lang_files.push_back(std::move(line));
		}
		if (!vec_lang_files.back().compare(vec_lang_files[vec_lang_files.size() - 2].c_str()))
		{
			vec_lang_files.pop_back();
		}

		for (const auto& lang : vec_lang_files)
		{
			std::string langName;
			std::ifstream langFile(LANG_DIR + lang, std::ifstream::in);
			langFile >> langName;
			vec_choose_lang.push_back(zSTRING(langName.c_str()));
		}
	};

	void CMainMenu::LaunchMenuScene()
	{
		CamWeapon = oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex(string_tmp));
		CamWeapon->ClearItemName();
		CamWeapon->SetPositionWorld(zVEC3((float)13354.502930, 2040.0, (float)-1141.678467));
		CamWeapon->RotateWorldY(-150);
		oCGame::GetGame()->CamInit(CamWeapon, zCCamera::GetCamera());
		string_tmp = "ItMw_1H_Blessed_03";
		TitleWeapon = oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex(string_tmp));
		TitleWeapon->SetPositionWorld(zVEC3((float)13346.502930, 2006.0, (float)-1240.678467));

	};

	void CMainMenu::LoadConfig()
	{
		if(!user_config){
			user_config = CConfig::GetInstance();
			LoadLangNames();
			if(!user_config->IsDefault()) {
				headmodel_tmp = CPlayer::GetHeadModelNameFromByte(user_config->headmodel);
				Walkstyle_tmp = CPlayer::GetWalkStyleFromByte(user_config->walkstyle);
				string_tmp = "HUM_BODY_NAKED0";
				oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
				oCNpc::GetHero()->ApplyOverlay(Walkstyle_tmp);
			}
		}
	};

	void CMainMenu::CleanUpMainMenu()
	{
		zCSoundSystem::GetSoundSystem()->StopAllSounds();
		MState = MENU_CLEAN;
		oCGame::GetGame()->CamInitOrginal();
		TitleWeapon->RemoveVobFromWorld();
		CamWeapon->RemoveVobFromWorld();
		TitleWeapon = NULL;
		CamWeapon = NULL;
		TitleWeaponEnabled = false;
		MenuPos = 0;
		OptionPos = 0;
		oCNpc::GetHero()->SetMovLock(0);
		EnableHealthBar();
		Screen->RemoveItem(GMPLogo);
		delete GMPLogo;
		if(AppWeapon) AppWeapon->RemoveVobFromWorld();
		oCGame::GetGame()->GetWorldTimer()->SetDay(1);
		oCGame::GetGame()->GetWorldTimer()->SetTime(12, 00);
		Screen->SetFont(FDefault);
	};
	

	void CMainMenu::PrintMenu()
	{
			if(!LangSetting) {
				zSTRING path=LANG_DIR;
				path+=vec_lang_files[user_config->lang].c_str();
				LangSetting = new CLanguage(path.ToChar());
				Lang = LangSetting;
				esl = new ExtendedServerList();
				path.Clear();
				vec_lang_files.clear();
			}
			switch(ps){
				default:
				case MAIN_MENU:
					Screen->SetFont(GlobalFont);
					FColor = (MenuPos == 0) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 3200, (*LangSetting)[CLanguage::MMENU_CHSERVER]);
					FColor = (MenuPos == 1) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 3600, (*LangSetting)[CLanguage::MMENU_APPEARANCE]);
					FColor = (MenuPos == 2) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 4000, (*LangSetting)[CLanguage::MMENU_OPTIONS]);
					FColor = (MenuPos == 3) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 4400, (*LangSetting)[CLanguage::MMENU_ONLINEOPTIONS]);
					FColor = (MenuPos == 4) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 4800, (*LangSetting)[CLanguage::MMENU_LEAVEGAME]);
				break;
				case SERVER_LIST: {

					esl->setLanguage(LangSetting);
					esl->HandleInput();
					esl->Draw();
				break;
								  }
				case SETTINGS_MENU:
				{
					Screen->SetFont(GlobalFont);
					FColor = (OptionPos == 0) ? Highlighted : Normal;
					if(WritingNickname) FColor = Red;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 3200, (*LangSetting)[CLanguage::MMENU_NICKNAME]);
					Screen->SetFontColor(Normal);
					if(ScreenResolution.x < 1024) Screen->Print(1800, 3200, user_config->Nickname);
					else Screen->Print(1500, 3200, user_config->Nickname);
					FColor = (OptionPos == 1) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 3600, (user_config->logchat) ? (*LangSetting)[CLanguage::MMENU_LOGCHATYES] : (*LangSetting)[CLanguage::MMENU_LOGCHATNO]);
					FColor = (OptionPos == 2) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 4000, (user_config->watch) ? (*LangSetting)[CLanguage::MMENU_WATCHON] : (*LangSetting)[CLanguage::MMENU_WATCHOFF]);
					FColor = (OptionPos == 3) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 4400, (*LangSetting)[CLanguage::MMENU_SETWATCHPOS]);
					FColor = (OptionPos == 4) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 4800, (user_config->antialiasing) ? (*LangSetting)[CLanguage::MMENU_ANTIALIASINGYES] : (*LangSetting)[CLanguage::MMENU_ANTIAlIASINGNO]);
					FColor = (OptionPos == 5) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 5200, (user_config->joystick) ? (*LangSetting)[CLanguage::MMENU_JOYSTICKYES] : (*LangSetting)[CLanguage::MMENU_JOYSTICKNO]);
					FColor = (OptionPos == 6) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					sprintf(tmpbuff, "%s %d", (*LangSetting)[CLanguage::MMENU_CHATLINES].ToChar(),  user_config->ChatLines);
					ChatLinesTMP = tmpbuff;
					Screen->Print(200, 5600, ChatLinesTMP);
					FColor = (OptionPos == 7) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					int printx = 200;
					int printy = 6000;
					switch (user_config->keyboardlayout)
					{
						case CConfig::KEYBOARD_POLISH:
							Screen->Print(printx, printy, (*LangSetting)[CLanguage::KEYBOARD_POLISH]);
						break;
						case CConfig::KEYBOARD_GERMAN:
							Screen->Print(printx, printy, (*LangSetting)[CLanguage::KEYBOARD_GERMAN]);
						break;
						case CConfig::KEYBOARD_CYRYLLIC:
							Screen->Print(printx, printy, (*LangSetting)[CLanguage::KEYBOARD_RUSSIAN]);
						break;
					};
					FColor = (OptionPos == 8) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 6400, (user_config->logovideos) ? (*LangSetting)[CLanguage::INTRO_YES] : (*LangSetting)[CLanguage::INTRO_NO]);
					FColor = (OptionPos == 9) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 6800, (*LangSetting)[CLanguage::MMENU_BACK]);
				}
				break;
				case WORLDBUILDER_MENU:
					Screen->SetFont(GlobalFont);
					FColor = (WBMenuPos == 0) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 3200,(*LangSetting)[CLanguage::WB_NEWMAP]);
					FColor = (WBMenuPos == 1) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 3600, (*LangSetting)[CLanguage::WB_LOADMAP]);
					FColor = (WBMenuPos == 2) ? Highlighted : Normal;
					Screen->SetFontColor(FColor);
					Screen->Print(200, 4000, (*LangSetting)[CLanguage::MMENU_BACK]);
				break;
			}
	};

	void CMainMenu::SpeedUpTime()
	{
		oCGame::GetGame()->GetWorldTimer()->GetTime(Hour, Minute);
		if(Minute >= 59) Hour++;
		Minute++;
		oCGame::GetGame()->GetWorldTimer()->SetTime(Hour, Minute);
	};
	void CMainMenu::ClearNpcTalents(oCNpc* Npc)
	{
		Npc->GetInventory()->ClearInventory();
		Npc->DestroySpellBook();
		Npc->MakeSpellBook();
		for(int i=0; i<21; i++) {
			Npc->SetTalentSkill(i, 0);
			Npc->SetTalentValue(i, 0);
		}
	};
	void CMainMenu::EraseSpacesInNickname()
	{
		std::string erasenick = user_config->Nickname.ToChar();
		erasenick.resize(24);
		user_config->Nickname = erasenick.c_str();
		user_config->SaveConfigToFile();
	};

	void CMainMenu::LeaveOptionsMenu()
	{
		Input->ClearKeyBuffer();
		CMainMenu::GetInstance()->Options->Leave();
		CGameManager::GetGameManager()->ApplySomeSettings();
		oCNpc::GetHero()->SetMovLock(1);
		CMainMenu::GetInstance()->MState = MENU_LOOP;
		ScreenResolution.x = zCOption::GetOption()->ReadInt(zOPT_SEC_VIDEO, "zVidResFullscreenX", 320);
	};

	void CMainMenu::RunMenuItem()
	{
		switch(MenuPos)
		{
		case 0:
			// WYBIERZ SERWER
			Screen->RemoveItem(GMPLogo);
			ps=SERVER_LIST;
			MState=CHOOSE_SRV_LOOP;
			SelectedServer=0;
			esl->RefreshList();
			if(!ServerIP.IsEmpty()) ServerIP.Clear();
			EraseSpacesInNickname();
			oCNpc::GetHero()->SetName(user_config->Nickname);
		break;
		case 1:
			// WYBIERZ WYGLAD
			if(AppCamCreated){
				ChoosingApperance = ApperancePart::FACE;
				LastApperance = ApperancePart::FACE;
				AppWeapon->ResetRotationsWorld();
				AppWeapon->SetPositionWorld(zVEC3(oCNpc::GetHero()->GetPosition().x-78, oCNpc::GetHero()->GetPosition().y+50, oCNpc::GetHero()->GetPosition().z-119));
				AppWeapon->RotateWorldY(30);
				oCGame::GetGame()->CamInit(CMainMenu::GetInstance()->AppWeapon, zCCamera::GetCamera());
			}
			MState=MENU_APPEARANCE;
		break;
		case 2:
			// OPCJE
			MState=MENU_OPTIONS;
			if(!Options) Options = zCMenu::Create(zSTRING("MENU_OPTIONS"));
			Options->Run();
		break;
		case 3:
			// OPCJE DODATKOWE
			MState=MENU_OPTONLINE;
			ps=SETTINGS_MENU;
		break;
		case 4:
			// WYJDZ Z GRY
			CGameManager::GetGameManager()->Done();
		break;
		};
	};

	void CMainMenu::RunOptionsItem()
	{
		switch(OptionPos)
		{
		case 0:
			// NICKNAME
			WritingNickname = true;
		break;
		case 1:
			// LOGOWANIE CZATU
			user_config->logchat = !user_config->logchat;
			user_config->SaveConfigToFile();
		break;
		case 2:
			// WLACZENIE ZEGARA
			user_config->watch = !user_config->watch;
			user_config->SaveConfigToFile();
		break;
		case 3:
			// POZYCJA ZEGARA
			user_config->watch = true;
			Screen->RemoveItem(GMPLogo);
			MState=MENU_SETWATCHPOS;
		break;
		case 4:
			// ANTYALIASING
			user_config->antialiasing = !user_config->antialiasing;
			user_config->SaveConfigToFile();
		break;
		case 5:
			// JOYSTICK
			user_config->joystick = !user_config->joystick;
			user_config->SaveConfigToFile();
		break;
		case 6:
			// Chat Lines
		break;
		case 7:
			// KEYBOARD LAYOUT
		break;
		case 8:
			// INTROS
			user_config->logovideos = !user_config->logovideos;
			user_config->SaveConfigToFile();
		break;
		case 9:
			// POWROT
			ps=MAIN_MENU;
			MState=MENU_LOOP;
		break;
		};
	};
	void CMainMenu::RunWbMenuItem()
	{
		switch(WBMenuPos)
		{
			case 0:
				// NOWA MAPA
				MState=WRITE_WORLDNAME;
				WBMapName.Clear();
			break;
			case 1:
				// WCZYTYWANIE MAPY
				MState=LOAD_WBMAP;
				WBMapName.Clear();
			break;
			case 2:
				// POWROT DO MENU
				ps=MAIN_MENU;
				MState=MENU_LOOP;
			break;
		};
	};

	void __stdcall CMainMenu::MainMenuLoop()
	{
		CMainMenu::GetInstance()->RenderMenu();
	};

	void CMainMenu::RenderMenu()
	{
		if(Christmas)oCGame::GetGame()->GetWorld()->StartSnow();
		if(oCNpc::GetHero()->GetPosition().x != HeroPos.x){
			oCNpc::GetHero()->SetPosition(HeroPos.x, HeroPos.y, HeroPos.z);
			oCNpc::GetHero()->SetAngle(Angle.x, Angle.z, Angle.y, NAngle.x, NAngle.y, NAngle.z);
		}
		switch(MState)
		{
		case CHOOSE_LANGUAGE:
			if(user_config->IsDefault()){
			string_tmp = "Choose your language:";
			Screen->Print(200, 200, string_tmp);
			Screen->Print(200, 350, vec_choose_lang[Language]);
			if(zCInput::GetInput()->KeyToggled(KEY_LEFT)) Language=(Language==0)?(vec_choose_lang.size()-1):Language-1;
			if(zCInput::GetInput()->KeyToggled(KEY_RIGHT)) Language=(++Language==vec_choose_lang.size())?0:Language;
			if(zCInput::GetInput()->KeyPressed(KEY_RETURN)) {
				user_config->lang=Language;
				zSTRING path=LANG_DIR;
				path+=vec_lang_files[user_config->lang].c_str();
				LangSetting = new CLanguage(path.ToChar());
				Lang = LangSetting;
				esl = new ExtendedServerList();
				path.Clear();
				vec_lang_files.clear();
				zCInput::GetInput()->ClearKeyBuffer();
				MState = CHOOSE_NICKNAME;
			}
			}
			else{
				MState = CHOOSE_NICKNAME;
			}
		break;
		case CHOOSE_NICKNAME:
			if(user_config->IsDefault() || user_config->Nickname.IsEmpty()){
			if(!vec_choose_lang.empty()) vec_choose_lang.clear();
			Screen->Print(200,200,(*LangSetting)[CLanguage::WRITE_NICKNAME]);
			zCView::GetScreen()->Print(200+static_cast<zINT>(static_cast<float>((*LangSetting)[CLanguage::WRITE_NICKNAME].Length()*70)*fWRatio),200,user_config->Nickname);
			x[0]=GInput::GetCharacterFormKeyboard();
			if((x[0]==8) && (user_config->Nickname.Length()>0)) user_config->Nickname.DeleteRight(1);
			if((x[0]>=0x20) && (user_config->Nickname.Length()<24)) user_config->Nickname+=x;
			if((x[0]==0x0D) && (!user_config->Nickname.IsEmpty())){
				user_config->SaveConfigToFile();
				MState = MENU_LOOP;
			}
			}
			else{
				MState = MENU_LOOP;
			}
		break;
		case MENU_LOOP:
		{
			if(oCNpc::GetHero()->GetModel()->IsAnimationActive(WalkAnim)) oCNpc::GetHero()->GetModel()->StopAnimation(WalkAnim);
			if(!TitleWeaponEnabled && TitleWeapon) {
				TitleWeaponEnabled = true;
				oCGame::GetGame()->GetWorld()->AddVob(TitleWeapon);
			}
			PrintNews();
			if(!Christmas) SpeedUpTime();
			Screen->SetFont(FDefault);
			Screen->SetFontColor(Normal);
			VersionString = (git_IsPopulated()) ? git_Describe() : "Unknown build";
			Screen->Print(8192 - Screen->FontSize(VersionString), 8192 - Screen->FontY(), VersionString);
			Screen->Print(100, 8192 - Screen->FontY(), HOWTOWB);
			if(TitleWeapon) TitleWeapon->RotateWorldX(0.6f);
			if(Input->KeyToggled(KEY_F1)){
				g2names.open(".\\Multiplayer\\WorldBuilder\\g2mobs.wb");
				g2particles.open(".\\Multiplayer\\WorldBuilder\\g2particles.wb");
				if(g2names.good() && g2particles.good()){
					ps=WORLDBUILDER_MENU;
					MState=MENU_WORLDBUILDER;
				}
				else PrintTimedScreen->PrintTimedCXY("Important World Builder files missing. Couldn't launch. Download full installer from GMP site.", 5000.0f, 0);
				g2names.close();
				g2particles.close();
				g2names.clear();
				g2particles.clear();
			}
			if(Input->KeyToggled(KEY_UP)) {
				MenuPos == 0 ? MenuPos = MenuItems : MenuPos--;
			}
			if(Input->KeyToggled(KEY_DOWN)) {
				MenuPos == MenuItems ? MenuPos = 0 : MenuPos++;
			}
			if(Input->KeyPressed(KEY_RETURN)){
				Input->ClearKeyBuffer();
				RunMenuItem();
			}
			PrintMenu();
		}
		break;
		case CHOOSE_SRV_LOOP:
			if(!TitleWeaponEnabled && TitleWeapon) {
				TitleWeaponEnabled = true;
				oCGame::GetGame()->GetWorld()->AddVob(TitleWeapon);
			}
			if(Input->KeyPressed(KEY_RETURN)){
				if(SelectedServer != -1){
					ServerIP.Clear();
					char buforek[256];
					esl->getSelectedServer(buforek, sizeof(buforek));
					ServerIP+= buforek;
				}
				Input->ClearKeyBuffer();
				if(client){delete client; client=NULL;}
				client=new CGmpClient(ServerIP.ToChar(), LangSetting);
				if(!client->Connect()) Screen->GetPrintScreen()->PrintTimedCXY(client->GetLastError() , 5000.0f, &Red);
				if(client->IsConnected()){
					client->HandleNetwork();
					client->SyncGameTime();
					ClassSelect=new CSelectClass(LangSetting, client);
					zVEC3 SpawnpointPos = oCNpc::GetHero()->GetPosition();
					CleanUpMainMenu();
					Patch::PlayerInterfaceEnabled(true);
					HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)MainMenuLoop);
					if(!client->map.IsEmpty()){
						Patch::ChangeLevelEnabled(true);
						oCGame::GetGame()->ChangeLevel(client->map, zSTRING("????"));
						Patch::ChangeLevelEnabled(false);
					}
					zCWorld::DeleteAllNpcsAlt();
					oCNpc::GetHero()->SetPosition(SpawnpointPos.x, SpawnpointPos.y, SpawnpointPos.z);
					string WordBuilderMapFileName=".\\Multiplayer\\Data\\";
					WordBuilderMapFileName+=client->network->GetServerIp() + "_" + std::to_string(client->network->GetServerPort());
					ifstream WbMap(WordBuilderMapFileName.c_str());
					if(WbMap.good()){
						WbMap.close();
						LoadWorld::LoadWorld(WordBuilderMapFileName.c_str(), client->VobsWorldBuilderMap);
					}
					if(WbMap.is_open()) WbMap.close();
					WordBuilderMapFileName.clear();
					if(client->VobsWorldBuilderMap.size() > 0){
						for(int i = 0; i < (int)client->VobsWorldBuilderMap.size(); i++)
						{
							if(client->VobsWorldBuilderMap[i].Type == TYPE_MOB) client->VobsWorldBuilderMap[i].Vob->SetCollDet(1);
						}
					}
				} else{
					SPDLOG_ERROR("ERROR: {}", client->GetLastError().ToChar());
				}
			}
			if(Input->KeyPressed(KEY_W)) {SelectedServer= -1; Input->ClearKeyBuffer();}
			if(SelectedServer == -1){
				if(Input->KeyToggled(KEY_SLASH)) {
					SelectedServer=0;
				}
				if(Input->KeyToggled(KEY_F1)) {
					ServerIP = "127.0.0.1";
				}
				char x[2]={0,0};
				x[0]=GInput::GetCharacterFormKeyboard(true);
				if(x[0]>0x20){
					ServerIP+=x;
				}
				if((x[0]==0x08) && (ServerIP.Length())) ServerIP.DeleteRight(1);
				Screen->PrintCXY(ServerIP);
			}
			else PrintMenu();
			if(TitleWeapon) TitleWeapon->RotateWorldX(0.6f);
			/*if(SelectedServer>=0){
				if(Input->KeyToggled(KEY_DOWN) && (ServerList->GetListSize())) {
					if((ServerList->GetListSize()-1)>(size_t)SelectedServer){
						SelectedServer++;
						SetServerIP(SelectedServer);
					}
				}
				if(Input->KeyToggled(KEY_UP)) {
					if(SelectedServer>0){
						SelectedServer--;
						SetServerIP(SelectedServer);
					}
				}
			
				if(Input->KeyPressed(KEY_W)) {SelectedServer= -1; Input->ClearKeyBuffer();}
				if(Input->KeyPressed(KEY_R)) {ServerList->ReceiveListHttp(); SelectedServer=0; Input->ClearKeyBuffer();}
			} else{
				if(Input->KeyToggled(KEY_SLASH)) {
					SelectedServer=0;
				}
				if(Input->KeyToggled(KEY_F1)) {
					ServerIP = "127.0.0.1";
				}
				char x[2]={0,0};
				x[0]=GInput::GetCharacterFormKeyboard(true);
				if(x[0]>0x20){
					ServerIP+=x;
				}
				if((x[0]==0x08) && (ServerIP.Length())) ServerIP.DeleteRight(1);
			}*/
			if(Input->KeyPressed(KEY_ESCAPE)){
				Input->ClearKeyBuffer();
				Screen->InsertItem(GMPLogo);
				ps=MAIN_MENU;
				MState=MENU_LOOP;
			}
			break;
		case MENU_APPEARANCE:
			Screen->SetFont(FDefault);
			Screen->Print(100,200, (*LangSetting)[CLanguage::APP_INFO1]);
			if(!AppCamCreated){
				string_tmp = "ItMw_1h_Mil_Sword";
				AppWeapon = oCObjectFactory::GetFactory()->CreateItem(zCParser::GetParser()->GetIndex(string_tmp));
				AppWeapon->SetPositionWorld(zVEC3(oCNpc::GetHero()->GetPosition().x-78, oCNpc::GetHero()->GetPosition().y+50, oCNpc::GetHero()->GetPosition().z-119));
				AppWeapon->RotateWorldY(30);
				AppWeapon->ClearItemName();
				string_tmp = "HUM_BODY_NAKED0";
				if(!user_config) user_config = CConfig::GetInstance();
				headmodel_tmp = CPlayer::GetHeadModelNameFromByte(user_config->headmodel);
				oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
				oCNpc::GetHero()->GetModel()->GetDontKnow()->GetModelTexAniState()->SetSkinTexture(user_config->skintexture);
				ChoosingApperance = ApperancePart::FACE;
				LastApperance = ApperancePart::FACE;
				oCGame::GetGame()->CamInit(AppWeapon, zCCamera::GetCamera());
				AppCamCreated = true;
			}
			if((Input->KeyToggled(KEY_UP)) && (ChoosingApperance>ApperancePart::HEAD)) --ChoosingApperance;
			if((Input->KeyToggled(KEY_DOWN)) && (ChoosingApperance<ApperancePart::WALKSTYLE)) ++ChoosingApperance;
			if((Input->KeyPressed(KEY_ESCAPE))){
				string_tmp.Clear();
				Input->ClearKeyBuffer();
				MState = MENU_LOOP;
				user_config->SaveConfigToFile();
				oCGame::GetGame()->CamInit(CamWeapon, zCCamera::GetCamera());
			}
			switch(ChoosingApperance){
				default:
				case ApperancePart::HEAD:
					Screen->Print(500,2000, (*LangSetting)[CLanguage::HEAD_MODEL]);
				if((Input->KeyToggled(KEY_LEFT))){
					if(user_config->headmodel > 0)
					{
						user_config->headmodel--;
						headmodel_tmp = CPlayer::GetHeadModelNameFromByte(user_config->headmodel);
						oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
					}
				}
				if((Input->KeyToggled(KEY_RIGHT))){
					if(user_config->headmodel < 5)
					{
						user_config->headmodel++;
						headmodel_tmp = CPlayer::GetHeadModelNameFromByte(user_config->headmodel);
						oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
					}
				}
					if(LastApperance != ChoosingApperance){
						AppWeapon->SetPositionWorld(zVEC3(oCNpc::GetHero()->GetPosition().x+55, oCNpc::GetHero()->GetPosition().y+70, oCNpc::GetHero()->GetPosition().z-37));
						AppWeapon->RotateWorldY(-90);
						LastApperance = ChoosingApperance;
					}
					break;
				case ApperancePart::FACE:
					Screen->Print(500,2000, (*LangSetting)[CLanguage::FACE_APPERANCE]);
				if((Input->KeyToggled(KEY_LEFT))){
					if(user_config->facetexture > 0)
					{
						user_config->facetexture--;
						oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
					}
				}
				if((Input->KeyToggled(KEY_RIGHT))){
					if(user_config->facetexture < 162)
					{
						user_config->facetexture++;
						oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
					}
				}
					if(LastApperance != ChoosingApperance){
					AppWeapon->SetPositionWorld(zVEC3(oCNpc::GetHero()->GetPosition().x-78, oCNpc::GetHero()->GetPosition().y+50, oCNpc::GetHero()->GetPosition().z-119));
					AppWeapon->RotateWorldY(90);
					LastApperance = ChoosingApperance;
					}
					break;
				case ApperancePart::SKIN:
					Screen->Print(500,2000, (*LangSetting)[CLanguage::SKIN_TEXTURE]);
					if(oCNpc::GetHero()->GetModel()->IsAnimationActive(WalkAnim)) oCNpc::GetHero()->GetModel()->StopAnimation(WalkAnim);
				if((Input->KeyToggled(KEY_LEFT))){
					if(user_config->skintexture > 0)
					{
						user_config->skintexture--;
						oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
						oCNpc::GetHero()->GetModel()->GetDontKnow()->GetModelTexAniState()->SetSkinTexture(user_config->skintexture);
					}
				}
				if((Input->KeyToggled(KEY_RIGHT))){
					if(user_config->skintexture < 12)
					{
						user_config->skintexture++;
						oCNpc::GetHero()->SetAdditionalVisuals(string_tmp, user_config->skintexture, 0, headmodel_tmp, user_config->facetexture, 0, -1);
						oCNpc::GetHero()->GetModel()->GetDontKnow()->GetModelTexAniState()->SetSkinTexture(user_config->skintexture);
					}
				}
				break;
				case ApperancePart::WALKSTYLE:
					Screen->Print(500,2000, (*LangSetting)[CLanguage::WALK_STYLE]);
				if((Input->KeyPressed(KEY_LEFT))){
					Input->ClearKeyBuffer();
					if(user_config->walkstyle > 0)
					{
						if(oCNpc::GetHero()->GetModel()->IsAnimationActive(WalkAnim)) oCNpc::GetHero()->GetModel()->StopAnimation(WalkAnim);
						oCNpc::GetHero()->RemoveOverlay(Walkstyle_tmp);
						user_config->walkstyle--;
						Walkstyle_tmp = CPlayer::GetWalkStyleFromByte(user_config->walkstyle);
						oCNpc::GetHero()->ApplyOverlay(Walkstyle_tmp);
					}
				}
				if((Input->KeyPressed(KEY_RIGHT))){
					Input->ClearKeyBuffer();
					if(user_config->walkstyle < 6)
					{
						if(oCNpc::GetHero()->GetModel()->IsAnimationActive(WalkAnim)) oCNpc::GetHero()->GetModel()->StopAnimation(WalkAnim);
						oCNpc::GetHero()->RemoveOverlay(Walkstyle_tmp);
						user_config->walkstyle++;
						Walkstyle_tmp = CPlayer::GetWalkStyleFromByte(user_config->walkstyle);
						oCNpc::GetHero()->ApplyOverlay(Walkstyle_tmp);
					}
				}
				if(!oCNpc::GetHero()->GetModel()->IsAnimationActive(WalkAnim)) oCNpc::GetHero()->GetModel()->StartAnimation(WalkAnim);
				oCNpc::GetHero()->SetPosition(HeroPos.x, HeroPos.y, HeroPos.z);
				break;
			}
		break;
		case MENU_OPTIONS:
			if(!oCNpc::GetHero()->IsMovLock()) oCNpc::GetHero()->SetMovLock(1);
			if(TitleWeapon) TitleWeapon->RotateWorldX(0.6f);
			if(memcmp("MENU_OPTIONS", zCMenu::GetActive()->GetName().ToChar(), 12)==0 && Input->KeyPressed(KEY_ESCAPE)) LeaveOptionsMenu();
			if(memcmp("MENUITEM_OPT_BACK", Options->GetActiveItem()->GetName().ToChar(), 17)==0 && Input->KeyPressed(KEY_RETURN)) LeaveOptionsMenu();
		break;
		case MENU_OPTONLINE:
			if(TitleWeapon) TitleWeapon->RotateWorldX(0.6f);
			if(user_config->watch) CWatch::GetInstance()->PrintWatch();
			if(WritingNickname){
				x[0]=GInput::GetCharacterFormKeyboard();
				if((x[0]==8) && (user_config->Nickname.Length()>0)) user_config->Nickname.DeleteRight(1);
				if((x[0]>=0x20) && (user_config->Nickname.Length()<24)) user_config->Nickname+=x;
				if((x[0]==0x0D) && (!user_config->Nickname.IsEmpty())){
					user_config->SaveConfigToFile();
					WritingNickname = false;
				}
			}
			// Wybor opcji przez enter
			if(!WritingNickname){
			if(Input->KeyToggled(KEY_UP)) {
				OptionPos == 0 ? OptionPos = 9 : OptionPos--;
            }
			if(Input->KeyToggled(KEY_DOWN)) {
				OptionPos == 9 ? OptionPos = 0 : OptionPos++;
			}
			if(Input->KeyPressed(KEY_RETURN)){
				Input->ClearKeyBuffer();
				RunOptionsItem();
			}
			}
			// Opcja od ilosci lini czatu
			if(OptionPos == 6){
				if(Input->KeyToggled(KEY_LEFT)){
					if(user_config->ChatLines > 0){
						if(user_config->ChatLines <= 5) user_config->ChatLines = 0;
						else user_config->ChatLines--;
						user_config->SaveConfigToFile();
					}
				}
				if(Input->KeyToggled(KEY_RIGHT)){
					if(user_config->ChatLines < 30){
						if(user_config->ChatLines < 5) user_config->ChatLines = 5;
						else user_config->ChatLines++;
						user_config->SaveConfigToFile();
					}
				}
			}
			if(OptionPos == 7){
				if(Input->KeyToggled(KEY_LEFT)){
					if(user_config->keyboardlayout > CConfig::KEYBOARD_POLISH){
						user_config->keyboardlayout--;
						user_config->SaveConfigToFile();
					}
				}
				if(Input->KeyToggled(KEY_RIGHT)){
					if(user_config->keyboardlayout < CConfig::KEYBOARD_CYRYLLIC){
						user_config->keyboardlayout++;
						user_config->SaveConfigToFile();
					}
				}
			}
			PrintMenu();
		break;
		case MENU_WORLDBUILDER:
			if(TitleWeapon) TitleWeapon->RotateWorldX(0.6f);
			// Wybor opcji przez enter
			if(Input->KeyToggled(KEY_UP)) { 
				WBMenuPos == 0 ? WBMenuPos = 2 : WBMenuPos--;
			}
			if(Input->KeyToggled(KEY_DOWN)) {
				WBMenuPos == 2 ? WBMenuPos = 0 : WBMenuPos++;
			}
			if(Input->KeyPressed(KEY_RETURN)){
				Input->ClearKeyBuffer();
				RunWbMenuItem();
			}
			PrintMenu();
		break;
		case WRITE_WORLDNAME:
			if(TitleWeapon) TitleWeapon->RotateWorldX(0.6f);
			if(zCInput::GetInput()->KeyToggled(KEY_ESCAPE)){
				ps=WORLDBUILDER_MENU;
				MState=MENU_WORLDBUILDER;
			}
			x[0]=GInput::GetCharacterFormKeyboard(true);
			if((x[0]==8) && (WBMapName.Length()>0)) WBMapName.DeleteRight(1);
			if((x[0]>=0x20) && (WBMapName.Length()<24)) WBMapName+=x;
			if((x[0]==0x0D) && (!WBMapName.IsEmpty())){
				CleanUpMainMenu();
				HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)MainMenuLoop);
				WBMapName.Upper();
				if(!strstr(WBMapName.ToChar(), ".ZEN")) WBMapName+=".ZEN";
				if(!memcmp("NEWWORLD.ZEN", WBMapName.ToChar(), 12)){
					WBMapName = "NEWWORLD\\NEWWORLD.ZEN";
					goto ALLDONE;
				};
				if(!memcmp("OLDWORLD.ZEN", WBMapName.ToChar(), 12)){WBMapName = "OLDWORLD\\OLDWORLD.ZEN";};
				if(!memcmp("ADDONWORLD.ZEN", WBMapName.ToChar(), 13)){WBMapName = "ADDON\\ADDONWORLD.ZEN";};
				Patch::ChangeLevelEnabled(true);
				oCGame::GetGame()->ChangeLevel(WBMapName, zSTRING("????"));
				Patch::ChangeLevelEnabled(false);
ALLDONE:
				zCWorld::DeleteAllNpcsAlt();
				Builder = new CBuilder();
				HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)WorldBuilderInterface, false);
			}
			Screen->PrintCX(3600, WRITE_MAPNAME);
			Screen->PrintCX(4000, WBMapName);
		break;
		case LOAD_WBMAP:
			if(TitleWeapon) TitleWeapon->RotateWorldX(0.6f);
			if(zCInput::GetInput()->KeyToggled(KEY_ESCAPE)){
				ps=WORLDBUILDER_MENU;
				MState=MENU_WORLDBUILDER;
			}
			x[0]=GInput::GetCharacterFormKeyboard();
			if((x[0]==8) && (WBMapName.Length()>0)) WBMapName.DeleteRight(1);
			if((x[0]>=0x20) && (WBMapName.Length()<24)) WBMapName+=x;
			if((x[0]==0x0D) && (!WBMapName.IsEmpty())){
				WBMapName.Upper();
				if(!strstr(WBMapName.ToChar(), ".WBM")) WBMapName+=".WBM";
				char buffer[64];
				sprintf(buffer, ".\\Multiplayer\\WorldBuilder\\Maps\\%s", WBMapName.ToChar());
				string Map = LoadWorld::GetZenName(buffer);
				if(Map.size() > 0){
					CleanUpMainMenu();
					HooksManager::GetInstance()->RemoveHook(HT_RENDER, (DWORD)MainMenuLoop);
					if(!memcmp("NEWWORLD\\NEWWORLD.ZEN", Map.c_str(), 22)){
						goto ALLDONE2;
					};
					WBMapName = Map.c_str();
					Patch::ChangeLevelEnabled(true);
					oCGame::GetGame()->ChangeLevel(WBMapName, zSTRING("????"));
					Patch::ChangeLevelEnabled(false);
ALLDONE2:
					zCWorld::DeleteAllNpcsAlt();
					Builder = new CBuilder();
					LoadWorld::LoadWorld(buffer, Builder->SpawnedVobs);
					HooksManager::GetInstance()->AddHook(HT_RENDER, (DWORD)WorldBuilderInterface, false);
				}
				else PrintTimedScreen->PrintTimedCX(2000, MAPFILE_EMPTY,5000.0f,0);
			}
			Screen->PrintCX(3600, WRITE_SAVEDMAP);
			Screen->PrintCX(4000, WBMapName);
		break;
		case MENU_SETWATCHPOS:
			if(user_config->watch) CWatch::GetInstance()->PrintWatch();
			if(Input->KeyPressed(KEY_ESCAPE)){
				Input->ClearKeyBuffer();
				MState = MENU_OPTONLINE;
				Screen->InsertItem(GMPLogo, false);
				user_config->SaveConfigToFile();
			}
			if(Input->KeyPressed(KEY_UP)) user_config->WatchPosY -= 16;
			if(Input->KeyPressed(KEY_DOWN)) user_config->WatchPosY += 16;
			if(Input->KeyPressed(KEY_LEFT)) user_config->WatchPosX -= 16;
			if(Input->KeyPressed(KEY_RIGHT)) user_config->WatchPosX += 16;
		break;
		};
	};
	void CMainMenu::SetServerIP(int selected){
		if(selected>=0){
			if(!ServerIP.IsEmpty()) ServerIP.Clear();
			char buffer[128];
			sprintf(buffer, "%s:%hu\0", ServerList->At((size_t)selected)->ip.ToChar(), ServerList->At((size_t)selected)->port);
			ServerIP=buffer;
		}
	}