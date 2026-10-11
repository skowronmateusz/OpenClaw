#ifndef __KEYBINDINGS_H__
#define __KEYBINDINGS_H__

#include <SDL2/SDL.h>
#include <string>

// Player actions which can be bound to a keyboard key
enum KeyBindingAction
{
    KeyBindingAction_MoveLeft,
    KeyBindingAction_MoveRight,
    KeyBindingAction_Up,
    KeyBindingAction_Down,
    KeyBindingAction_Jump,
    KeyBindingAction_Attack,
    KeyBindingAction_Fire,
    KeyBindingAction_ChangeAmmo,
    KeyBindingAction_Lift,

    KeyBindingAction_Count,
    KeyBindingAction_None = KeyBindingAction_Count
};

class KeyBindings
{
public:
    KeyBindings() { SetDefaults(false); }

    // Classic layout is the one of the original game (arrows, Ctrl, Alt, Space, Shift),
    // alternate layout is WASD + mouse
    void SetDefaults(bool useAlternateControls);

    SDL_Scancode GetKey(KeyBindingAction action) const { return m_Keys[action]; }

    // Binds the key to the action. Action which had this key before gets the previous key
    // of the rebound action, so that every action stays reachable.
    void SetKey(KeyBindingAction action, SDL_Scancode key);

    // Returns KeyBindingAction_None when the key is not bound
    KeyBindingAction GetAction(SDL_Scancode key) const;

    // Name of the action in config.xml, e.g. "MoveLeft"
    static const char* GetActionConfigName(KeyBindingAction action);
    static KeyBindingAction ConfigNameToAction(const std::string& name);

    // Name of the key in config.xml. SDL key names differ between platforms (e.g. "Left Alt"
    // is "Left Option" on macOS) and SDL cannot parse some of them back, so these are fixed.
    static std::string KeyToConfigName(SDL_Scancode key);
    static SDL_Scancode ConfigNameToKey(const std::string& name);

    // Caption shown in the controls menu in the language of the game data
    static std::string GetActionCaption(KeyBindingAction action);
    static std::string GetKeyCaption(SDL_Scancode key);

    // Keys which are reserved and cannot be bound (menu, console)
    static bool IsKeyBindable(SDL_Scancode key);

private:
    SDL_Scancode m_Keys[KeyBindingAction_Count];
};

#endif
