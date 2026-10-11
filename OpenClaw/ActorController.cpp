#include "ActorController.h"
#include "Engine/Scene/SceneNodes.h"
#include "Engine/Events/EventMgr.h"
#include "Engine/Events/Events.h"
#include "ClawEvents.h"
#include "Engine/GameApp/BaseGameApp.h"
#include "Engine/UserInterface/Touch/TouchRecognizers/TapRecognizer.h"
#include "Engine/UserInterface/Touch/TouchRecognizers/JoystickRecognizer.h"
#include "Engine/UserInterface/Touch/TouchRecognizers/SwipeRecognizer.h"
#include "Engine/UserInterface/Touch/TouchRecognizers/PressRecognizer.h"

#define PAUSE_TAP_RECOGNIZER 1
#define JUMP_PRESS_RECOGNIZER 2
#define ATTACK_TAP_RECOGNIZER 3
#define WEAPON_TAP_RECOGNIZER 4
#define MOVEMENT_JOYSTICK_RECOGNIZER 5
#define PROJECTILE_SWIPE_RECOGNIZER 6

ActorController::ActorController(shared_ptr<SceneNode> controlledObject, float speed)
{
    m_pControlledObject = controlledObject;
    m_Speed = speed;
    m_pKeyStates = NULL;
    m_MouseLeftButtonDown = m_MouseRightButtonDown = false;
    ReleaseAllActions();
}

void ActorController::HandleAction(ActionType actionType)
{
    switch(actionType)
    {
        case ActionType_Fire:
        {
            shared_ptr<EventData_Actor_Fire> pClimbEvent(new EventData_Actor_Fire(m_pControlledObject->VGetProperties()->GetActorId()));
            IEventMgr::Get()->VTriggerEvent(pClimbEvent);
            break;
        }
        case ActionType_Attack:
        {
            shared_ptr<EventData_Actor_Attack> pClimbEvent(new EventData_Actor_Attack(m_pControlledObject->VGetProperties()->GetActorId()));
            IEventMgr::Get()->VTriggerEvent(pClimbEvent);
            break;
        }
        case ActionType_Lift:
        {
            shared_ptr<EventData_Actor_Lift> pEvent(new EventData_Actor_Lift(m_pControlledObject->VGetProperties()->GetActorId()));
            IEventMgr::Get()->VTriggerEvent(pEvent);
            break;
        }
        case ActionType_Change_Ammo_Type:
        {
            shared_ptr<EventData_Request_Change_Ammo_Type> pEvent(new EventData_Request_Change_Ammo_Type(m_pControlledObject->VGetProperties()->GetActorId()));
            IEventMgr::Get()->VTriggerEvent(pEvent);
            break;
        }
    }
}

void ActorController::OnUpdate(uint32 msDiff)
{
    int count;
    SDL_PumpEvents();
    m_pKeyStates = SDL_GetKeyboardState(&count);

    float moveX = 0.0f;
    float moveY = 0.0f;

    float climbY = 0.0f;

    // We need two conditions because I want behaviour such as when both right and left
    // buttons are pressed, I dont want actor to move, e.g. to have the move effect nullyfied

    if (m_ActionPressed[KeyBindingAction_MoveRight])
    {
        moveX += m_Speed * (float)msDiff;
    }
    if (m_ActionPressed[KeyBindingAction_MoveLeft])
    {
        moveX -= m_Speed * (float)msDiff;
    }

    // CLimbing
    if (m_ActionPressed[KeyBindingAction_Down])
    {
        climbY += 5.0;
    }
    if (m_ActionPressed[KeyBindingAction_Up])
    {
        climbY -= 5.0;
    }

    // Jumping, with alternate (WASD) controls up jumps aswell
    if (m_ActionPressed[KeyBindingAction_Jump] ||
        (g_pApp->GetControlOptions()->useAlternateControls && m_ActionPressed[KeyBindingAction_Up]))
    {
        moveY -= m_Speed * (float)msDiff;
    }

    if (fabs(climbY) > FLT_EPSILON)
    {
        shared_ptr<EventData_Start_Climb> pClimbEvent(new EventData_Start_Climb(m_pControlledObject->VGetProperties()->GetActorId(), Point(0, climbY)));
        IEventMgr::Get()->VTriggerEvent(pClimbEvent);
    }
    if (fabs(moveX) > FLT_EPSILON || fabs(moveY) > FLT_EPSILON)
    {
        shared_ptr<EventData_Actor_Start_Move> pMoveEvent(new EventData_Actor_Start_Move(m_pControlledObject->VGetProperties()->GetActorId(), Point(moveX, moveY)));
        IEventMgr::Get()->VTriggerEvent(pMoveEvent);
    }
}

bool ActorController::OnActionDown(KeyBindingAction action)
{
    switch (action)
    {
        case KeyBindingAction_Fire:
            HandleAction(ActionType_Fire);
            return true;
        case KeyBindingAction_Attack:
            HandleAction(ActionType_Attack);
            return true;
        case KeyBindingAction_Lift:
            HandleAction(ActionType_Lift);
            return true;
        case KeyBindingAction_ChangeAmmo:
            HandleAction(ActionType_Change_Ammo_Type);
            return true;
        case KeyBindingAction_None:
            return false;
        default:
            m_ActionPressed[action] = true;
            return false;
    }
}

bool ActorController::OnActionUp(KeyBindingAction action)
{
    if (action == KeyBindingAction_Fire)
    {
        shared_ptr<EventData_Actor_Fire_Ended> pFireEndedEvent(new EventData_Actor_Fire_Ended(m_pControlledObject->VGetProperties()->GetActorId()));
        IEventMgr::Get()->VTriggerEvent(pFireEndedEvent);
        return true;
    }

    if (action != KeyBindingAction_None)
    {
        m_ActionPressed[action] = false;
    }

    return false;
}

bool ActorController::VOnKeyDown(SDL_Keycode key)
{
    return OnActionDown(g_pApp->GetKeyBindings()->GetAction(SDL_GetScancodeFromKey(key)));
}

bool ActorController::VOnKeyUp(SDL_Keycode key)
{
    return OnActionUp(g_pApp->GetKeyBindings()->GetAction(SDL_GetScancodeFromKey(key)));
}

void ActorController::ReleaseAllActions()
{
    for (bool& isPressed : m_ActionPressed)
    {
        isPressed = false;
    }
}

bool ActorController::VOnPointerMove(SDL_MouseMotionEvent& mouseEvent)
{
    return false;
}

bool ActorController::VOnPointerButtonDown(SDL_MouseButtonEvent& mouseEvent)
{
    if (mouseEvent.button == SDL_BUTTON_LEFT)
    {
        m_MouseLeftButtonDown = true;

        if (g_pApp->GetControlOptions()->useAlternateControls)
        {
            HandleAction(ActionType_Fire);
        }
        
        return true;
    }
    else if (mouseEvent.button == SDL_BUTTON_RIGHT)
    {
        m_MouseRightButtonDown = true;

        if (g_pApp->GetControlOptions()->useAlternateControls)
        {
            HandleAction(ActionType_Attack);
        }

        return true;
    }

    return false;
}

bool ActorController::VOnPointerButtonUp(SDL_MouseButtonEvent& mouseEvent)
{
    if (mouseEvent.button == SDL_BUTTON_LEFT)
    {
        shared_ptr<EventData_Actor_Fire_Ended> pFireEndedEvent(new EventData_Actor_Fire_Ended(m_pControlledObject->VGetProperties()->GetActorId()));
        IEventMgr::Get()->VTriggerEvent(pFireEndedEvent);

        m_MouseLeftButtonDown = false;
        return true;
    }
    else if (mouseEvent.button == SDL_BUTTON_RIGHT)
    {
        m_MouseRightButtonDown = false;
        return true;
    }

    return false;
}

std::vector<std::shared_ptr<AbstractRecognizer>> ActorController::VRegisterRecognizers() {
    const float thresholdDistance = g_pApp->GetControlOptions()->touchScreen.distanceThreshold;
    const unsigned int thresholdMs = g_pApp->GetControlOptions()->touchScreen.timeThreshold;

    // TODO: Try to describe it in config?
    std::vector<std::shared_ptr<AbstractRecognizer>> recognizers{};
    recognizers.reserve(6);

    // Pause button. Left up screen corner. Maximum priority
    auto pauseRecognizer = std::make_shared<TapRecognizer>(PAUSE_TAP_RECOGNIZER, 100);
    pauseRecognizer->SetFrame(Rect{0.0f, 0.0f, 0.1f, 0.1f});
    recognizers.push_back(pauseRecognizer);

    // Change weapon button. Right up screen corner. Maximum priority
    auto weaponRecognizer = std::make_shared<TapRecognizer>(WEAPON_TAP_RECOGNIZER, 99);
    weaponRecognizer->SetFrame(Rect{0.9f, 0.0f, 0.1f, 0.2f});
    recognizers.push_back(weaponRecognizer);

    // Attack button. All screen area. Minimum priority
    auto attackRecognizer = std::make_shared<TapRecognizer>(ATTACK_TAP_RECOGNIZER, 1);
    recognizers.push_back(attackRecognizer);

    // Movement controller. Left part of screen. Medium priority
    auto joystick = std::make_shared<JoystickRecognizer>(MOVEMENT_JOYSTICK_RECOGNIZER, 50, thresholdDistance, thresholdMs + 100);
    joystick->SetFrame(Rect{0.0f, 0.0f, 0.5f, 1.0f});
    recognizers.push_back(joystick);

    // Jump button. Right part of screen. Minimum priority but greater that attack button
    auto jumpButton = std::make_shared<PressRecognizer>(JUMP_PRESS_RECOGNIZER, 20, thresholdDistance, thresholdMs);
    jumpButton->SetFrame(Rect{0.5f, 0.0f, 0.5f, 1.0f});
    recognizers.push_back(jumpButton);

    // Swipe to attack. Right part of screen. Medium priority (greater than jump button)
    auto swipe = std::make_shared<SwipeRecognizer>(PROJECTILE_SWIPE_RECOGNIZER, 51, thresholdDistance, thresholdMs);
    swipe->SetFrame(Rect{0.5f, 0.0f, 0.5f, 1.0f});
    recognizers.push_back(swipe);

    return recognizers;
}

bool ActorController::VOnTouch(const Touch_Event &evt) {
    auto code = evt.sdlEvent.user.code;
    switch (code) {
        case TAP:
            return OnTap(evt.recognizerId, evt.customData.tap);
        case JOYSTICK_MOVE:
        case JOYSTICK_RESET:
            return OnJoystick(evt.recognizerId, evt.customData.joystick);
        case SWIPE_START:
        case SWIPE_END:
            return OnSwipe(evt.recognizerId, evt.customData.swipe, code == SWIPE_START);
        case PRESS_START:
        case PRESS_END:
            return OnPress(evt.recognizerId, evt.customData.press, code == PRESS_START);
    }
    return false;
}

bool ActorController::OnTap(int id, const Touch_TapEvent &evt) {
    KeyBindingAction action;
    switch (id) {
        case PAUSE_TAP_RECOGNIZER:
        {
            // Little hack to open/close ingame menu
            SDL_Keycode key = SDLK_ESCAPE;

            SDL_Event event{0};
            event.type = SDL_KEYDOWN;
            event.key.keysym.sym = key;
            event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
            SDL_PushEvent(&event);
            event.type = SDL_KEYUP;
            SDL_PushEvent(&event);
            return true;
        }
        case WEAPON_TAP_RECOGNIZER:
            action = KeyBindingAction_ChangeAmmo;
            break;
        case ATTACK_TAP_RECOGNIZER:
            action = KeyBindingAction_Attack;
            break;
        default:
            action = KeyBindingAction_None;
            break;
    }

    if (action != KeyBindingAction_None) {
        OnActionDown(action);
        OnActionUp(action);
        return true;
    }
    return false;
}

bool ActorController::OnJoystick(int id, const Touch_JoystickEvent &evt) {
    if (id == MOVEMENT_JOYSTICK_RECOGNIZER) {
        const auto dx = evt.dx;
        const auto dy = evt.dy;

        const float deadzone = 0.05 * 0.05;

        KeyBindingAction horizontalAction = KeyBindingAction_None;
        KeyBindingAction verticalAction = KeyBindingAction_None;
        if (dx * dx + dy * dy > deadzone) {

            if (dx * 2 < dy && dx * 2 > -dy) {
                // 247 - 292 deg
                verticalAction = KeyBindingAction_Down;
            } else if (dx * 2 < -dy && dx * 2 > dy) {
                // 67 deg - 112 deg
                verticalAction = KeyBindingAction_Up;
            }

            if (dy < dx * 2 && dy > -dx * 2) {
                // 292 - 67 deg
                horizontalAction = KeyBindingAction_MoveRight;
            } else if (dy < -dx * 2 && dy > dx * 2) {
                // 112 - 247 deg
                horizontalAction = KeyBindingAction_MoveLeft;
            }
        }

        KeyBindingAction actions[] = {KeyBindingAction_MoveLeft, KeyBindingAction_Up, KeyBindingAction_MoveRight, KeyBindingAction_Down};
        for (KeyBindingAction action : actions) {
            bool isPressed = m_ActionPressed[action];
            bool press = action == horizontalAction || action == verticalAction;
            if (!isPressed && press) {
                OnActionDown(action);
            } else if (isPressed && !press) {
                OnActionUp(action);
            }
        }

        return true;
    }
    return false;
}

bool ActorController::OnSwipe(int id, const Touch_SwipeEvent &evt, bool start) {
    if (id == PROJECTILE_SWIPE_RECOGNIZER) {
        if (start) {
            return OnActionDown(KeyBindingAction_Fire);
        } else {
            return OnActionUp(KeyBindingAction_Fire);
        }
    }
    return false;
}

bool ActorController::OnPress(int id, const Touch_PressEvent &evt, bool start) {
    if (id == JUMP_PRESS_RECOGNIZER) {
        if (start) {
            return OnActionDown(KeyBindingAction_Jump);
        } else {
            return OnActionUp(KeyBindingAction_Jump);
        }
    }
    return false;
}
