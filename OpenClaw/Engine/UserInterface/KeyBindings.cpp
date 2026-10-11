#include "KeyBindings.h"
#include "../GameApp/BaseGameApp.h"

#include <map>

struct KeyBindingActionInfo
{
    const char* configName;
    SDL_Scancode classicKey;
    SDL_Scancode alternateKey;
    // Polish captions are written without diacritics, which the console font does not have
    const char* captionEn;
    const char* captionPl;
};

// Same order as KeyBindingAction enum
static const KeyBindingActionInfo s_ActionInfo[KeyBindingAction_Count] =
{
    { "MoveLeft",   SDL_SCANCODE_LEFT,   SDL_SCANCODE_A,      "Move left",     "W lewo" },
    { "MoveRight",  SDL_SCANCODE_RIGHT,  SDL_SCANCODE_D,      "Move right",    "W prawo" },
    { "Up",         SDL_SCANCODE_UP,     SDL_SCANCODE_W,      "Look up/Climb", "W gore/Wspinaczka" },
    { "Down",       SDL_SCANCODE_DOWN,   SDL_SCANCODE_S,      "Duck/Climb",    "Kucanie/Schodzenie" },
    { "Jump",       SDL_SCANCODE_SPACE,  SDL_SCANCODE_SPACE,  "Jump",          "Skok" },
    { "Attack",     SDL_SCANCODE_LCTRL,  SDL_SCANCODE_J,      "Attack",        "Atak" },
    { "Fire",       SDL_SCANCODE_LALT,   SDL_SCANCODE_K,      "Fire weapon",   "Strzal" },
    { "ChangeAmmo", SDL_SCANCODE_LSHIFT, SDL_SCANCODE_E,      "Switch weapon", "Zmiana broni" },
    { "Lift",       SDL_SCANCODE_Z,      SDL_SCANCODE_Q,      "Lift/Throw",    "Podnies/Rzuc" },
};

void KeyBindings::SetDefaults(bool useAlternateControls)
{
    for (int action = 0; action < KeyBindingAction_Count; action++)
    {
        m_Keys[action] = useAlternateControls ? s_ActionInfo[action].alternateKey : s_ActionInfo[action].classicKey;
    }
}

void KeyBindings::SetKey(KeyBindingAction action, SDL_Scancode key)
{
    if (action >= KeyBindingAction_Count)
    {
        return;
    }

    KeyBindingAction previousOwner = GetAction(key);
    if (previousOwner != KeyBindingAction_None && previousOwner != action)
    {
        m_Keys[previousOwner] = m_Keys[action];
    }

    m_Keys[action] = key;
}

KeyBindingAction KeyBindings::GetAction(SDL_Scancode key) const
{
    if (key == SDL_SCANCODE_UNKNOWN)
    {
        return KeyBindingAction_None;
    }

    for (int action = 0; action < KeyBindingAction_Count; action++)
    {
        if (m_Keys[action] == key)
        {
            return (KeyBindingAction)action;
        }
    }

    return KeyBindingAction_None;
}

const char* KeyBindings::GetActionConfigName(KeyBindingAction action)
{
    return action < KeyBindingAction_Count ? s_ActionInfo[action].configName : "";
}

KeyBindingAction KeyBindings::ConfigNameToAction(const std::string& name)
{
    for (int action = 0; action < KeyBindingAction_Count; action++)
    {
        if (name == s_ActionInfo[action].configName)
        {
            return (KeyBindingAction)action;
        }
    }

    return KeyBindingAction_None;
}

static const std::map<SDL_Scancode, const char*> s_KeyConfigNames =
{
    { SDL_SCANCODE_LALT, "Left Alt" },
    { SDL_SCANCODE_RALT, "Right Alt" },
    { SDL_SCANCODE_LGUI, "Left GUI" },
    { SDL_SCANCODE_RGUI, "Right GUI" },
};

std::string KeyBindings::KeyToConfigName(SDL_Scancode key)
{
    auto findIt = s_KeyConfigNames.find(key);
    if (findIt != s_KeyConfigNames.end())
    {
        return findIt->second;
    }

    return SDL_GetScancodeName(key);
}

SDL_Scancode KeyBindings::ConfigNameToKey(const std::string& name)
{
    for (const auto& keyName : s_KeyConfigNames)
    {
        if (SDL_strcasecmp(name.c_str(), keyName.second) == 0)
        {
            return keyName.first;
        }
    }

    return SDL_GetScancodeFromName(name.c_str());
}

static bool IsPolish()
{
    return g_pApp != NULL && g_pApp->GetGameDataLanguage() == "pl";
}

std::string KeyBindings::GetActionCaption(KeyBindingAction action)
{
    if (action >= KeyBindingAction_Count)
    {
        return "";
    }

    return IsPolish() ? s_ActionInfo[action].captionPl : s_ActionInfo[action].captionEn;
}

std::string KeyBindings::GetKeyCaption(SDL_Scancode key)
{
    static const std::map<SDL_Scancode, std::pair<const char*, const char*>> s_KeyCaptions =
    {
        { SDL_SCANCODE_LEFT,   { "Left Arrow",  "Strzalka w lewo" } },
        { SDL_SCANCODE_RIGHT,  { "Right Arrow", "Strzalka w prawo" } },
        { SDL_SCANCODE_UP,     { "Up Arrow",    "Strzalka w gore" } },
        { SDL_SCANCODE_DOWN,   { "Down Arrow",  "Strzalka w dol" } },
        { SDL_SCANCODE_SPACE,  { "Space",       "Spacja" } },
        { SDL_SCANCODE_LCTRL,  { "Left Ctrl",   "Lewy Ctrl" } },
        { SDL_SCANCODE_RCTRL,  { "Right Ctrl",  "Prawy Ctrl" } },
        { SDL_SCANCODE_LALT,   { "Left Alt",    "Lewy Alt" } },
        { SDL_SCANCODE_RALT,   { "Right Alt",   "Prawy Alt" } },
        { SDL_SCANCODE_LSHIFT, { "Left Shift",  "Lewy Shift" } },
        { SDL_SCANCODE_RSHIFT, { "Right Shift", "Prawy Shift" } },
    };

    if (key == SDL_SCANCODE_UNKNOWN)
    {
        return "-";
    }

    auto findIt = s_KeyCaptions.find(key);
    if (findIt != s_KeyCaptions.end())
    {
        return IsPolish() ? findIt->second.second : findIt->second.first;
    }

    std::string name = SDL_GetScancodeName(key);
    return name.empty() ? "?" : name;
}

bool KeyBindings::IsKeyBindable(SDL_Scancode key)
{
    return key != SDL_SCANCODE_UNKNOWN &&
           key != SDL_SCANCODE_ESCAPE &&    // Ingame menu
           key != SDL_SCANCODE_GRAVE;       // Console
}
