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

#include "menu/states/menu_state.hpp"
#include "menu/menu_context.hpp"

namespace menu {
namespace states {

/**
 * @brief Main menu state - the primary menu with options to join server, customize appearance, etc.
 * 
 * This is the main hub menu that players see after initial setup.
 * It presents options like:
 * - Choose Server
 * - Options
 * - Online Options
 * - Leave Game
 */
class MainMenuLoopState : public MenuState {
private:
    MenuContext& context_;
    
    enum MenuItem {
        CHOOSE_SERVER = 0,
        OPTIONS = 1,
        ONLINE_OPTIONS = 2,
        LEAVE_GAME = 3,
        MENU_ITEM_COUNT = 4
    };
    
    int selectedMenuItem_;
    MenuState* nextState_;
    
public:
    explicit MainMenuLoopState(MenuContext& context);
    ~MainMenuLoopState() override = default;
    
    // MenuState interface
    void OnEnter() override;
    void OnExit() override;
    StateResult Update() override;
    MenuState* CheckTransition() override;
    const char* GetStateName() const override { return "MainMenuLoop"; }
    
private:
    void RenderMenu();
    void RenderVersionInfo();
    void HandleInput();
    void ExecuteMenuItem(MenuItem item);
};

}  // namespace states
}  // namespace menu
