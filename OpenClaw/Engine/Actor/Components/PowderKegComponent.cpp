#include "PowderKegComponent.h"
#include "AnimationComponent.h"
#include "PositionComponent.h"
#include "../../GameApp/BaseGameApp.h"
#include "../../GameApp/BaseGameLogic.h"

const char* PowderKegComponent::g_Name = "PowderKegComponent";

std::vector<PowderKegComponent*> PowderKegComponent::s_PowderKegs;

PowderKegComponent::PowderKegComponent()
    :
    m_ThrowSpeed(Point(7.0, -1.5)),
    m_ThrowGravityScale(1.0f),
    m_MaxFlyTime(5000),
    m_State(PowderKegState_Idle),
    m_CarrierActorId(INVALID_ACTOR_ID),
    m_ThrowDirection(Direction_Right),
    m_FlyTime(0),
    m_bPendingLift(false),
    m_bPendingThrow(false),
    m_bPendingDrop(false),
    m_bPendingExplode(false),
    m_pAnimationComponent(NULL),
    m_pHealthComponent(NULL)
{
    s_PowderKegs.push_back(this);
}

PowderKegComponent::~PowderKegComponent()
{
    s_PowderKegs.erase(std::remove(s_PowderKegs.begin(), s_PowderKegs.end(), this), s_PowderKegs.end());
}

bool PowderKegComponent::VInit(TiXmlElement* pData)
{
    assert(pData != NULL);

    m_pPhysics = g_pApp->GetGameLogic()->VGetGamePhysics();
    if (!m_pPhysics)
    {
        LOG_WARNING("Attemtping to create powder keg component without valid physics");
        return false;
    }

    ParseValueFromXmlElem(&m_ThrowSpeed, pData->FirstChildElement("ThrowSpeed"), "x", "y");
    ParseValueFromXmlElem(&m_ThrowGravityScale, pData->FirstChildElement("ThrowGravityScale"));
    ParseValueFromXmlElem(&m_MaxFlyTime, pData->FirstChildElement("MaxFlyTime"));

    return true;
}

void PowderKegComponent::VPostInit()
{
    m_pAnimationComponent = MakeStrongPtr(m_pOwner->GetComponent<AnimationComponent>()).get();
    m_pHealthComponent = MakeStrongPtr(m_pOwner->GetComponent<HealthComponent>()).get();
    assert(m_pAnimationComponent != NULL);
    assert(m_pHealthComponent != NULL);

    m_pHealthComponent->AddObserver(this);

    // Idle keg is the first frame of its explode animation
    m_pAnimationComponent->SetAnimation("explode");
    m_pAnimationComponent->PauseAnimation();
}

TiXmlElement* PowderKegComponent::VGenerateXml()
{
    TiXmlElement* pBaseElement = new TiXmlElement(VGetName());

    XML_ADD_2_PARAM_ELEMENT("ThrowSpeed", "x", ToStr(m_ThrowSpeed.x).c_str(), "y", ToStr(m_ThrowSpeed.y).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("ThrowGravityScale", ToStr(m_ThrowGravityScale).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("MaxFlyTime", ToStr(m_MaxFlyTime).c_str(), pBaseElement);

    return pBaseElement;
}

void PowderKegComponent::VUpdate(uint32 msDiff)
{
    if (m_State == PowderKegState_Exploded)
    {
        return;
    }

    const uint32 kegId = m_pOwner->GetGUID();

    if (m_bPendingLift)
    {
        m_bPendingLift = false;

        // While carried the keg does not take part in physics simulation, it just follows Claw
        m_pPhysics->VDeactivate(kegId);
    }

    if (m_State == PowderKegState_Carried)
    {
        ApplyCarryPosition();
    }

    if (m_bPendingThrow)
    {
        m_bPendingThrow = false;

        Point speed = m_ThrowSpeed;
        if (m_ThrowDirection == Direction_Left)
        {
            speed.x *= -1.0;
        }

        ApplyCarryPosition();
        m_pPhysics->VActivate(kegId);
        // Thrown keg behaves like a projectile - enemies' hitboxes react to it
        m_pPhysics->VChangeCollisionFlag(kegId, CollisionFlag_PowderKeg, CollisionFlag_Bullet);
        m_pPhysics->VSetCollisionMask(kegId,
            CollisionFlag_Solid |
            CollisionFlag_Ground |
            CollisionFlag_Death |
            CollisionFlag_DynamicActor |
            CollisionFlag_Crate |
            CollisionFlag_PowderKeg |
            CollisionFlag_Bullet |
            CollisionFlag_Explosion);
        m_pPhysics->VSetGravityScale(kegId, m_ThrowGravityScale);
        m_pPhysics->VSetLinearSpeed(kegId, speed);

        m_pAnimationComponent->SetAnimation("thrown");
        m_pAnimationComponent->ResumeAnimation();

        m_FlyTime = 0;
        m_State = PowderKegState_Thrown;
    }

    if (m_bPendingDrop)
    {
        m_bPendingDrop = false;

        ApplyCarryPosition();
        m_pPhysics->VActivate(kegId);
        m_pPhysics->VSetLinearSpeed(kegId, Point(0, 0));

        m_pAnimationComponent->SetAnimation("explode");
        m_pAnimationComponent->PauseAnimation();

        m_State = PowderKegState_Idle;
    }

    if (m_State == PowderKegState_Thrown)
    {
        m_FlyTime += msDiff;
        if (m_FlyTime > m_MaxFlyTime)
        {
            m_bPendingExplode = true;
        }
    }

    if (m_bPendingExplode)
    {
        m_bPendingExplode = false;

        // Reuses standard keg destruction: DestroyableComponent plays explode animation
        // and sound, ExplodeableComponent spawns the area damage
        m_pHealthComponent->AddHealth(
            -1 * (m_pHealthComponent->GetHealth() + 1),
            DamageType_Explosion,
            m_pOwner->GetPositionComponent()->GetPosition(),
            m_CarrierActorId);
    }
}

void PowderKegComponent::VOnHealthBelowZero(DamageType damageType, int sourceActorId)
{
    m_State = PowderKegState_Exploded;
    m_bPendingLift = m_bPendingThrow = m_bPendingDrop = m_bPendingExplode = false;
}

PowderKegComponent* PowderKegComponent::FindLiftableKeg(const Point& position, Direction facingDirection, const Point& maxDistance)
{
    PowderKegComponent* pClosestKeg = NULL;
    double closestDistance = DBL_MAX;
    for (PowderKegComponent* pKeg : s_PowderKegs)
    {
        if (!pKeg->CanBeLifted())
        {
            continue;
        }

        Point kegPosition = pKeg->m_pOwner->GetPositionComponent()->GetPosition();
        double distanceX = fabs(kegPosition.x - position.x);
        double distanceY = fabs(kegPosition.y - position.y);
        if (distanceX > maxDistance.x || distanceY > maxDistance.y)
        {
            continue;
        }

        // Claw has to face the keg, only a small overlap behind him is tolerated
        double distanceInFront = kegPosition.x - position.x;
        if (facingDirection == Direction_Left)
        {
            distanceInFront *= -1.0;
        }
        if (distanceInFront < -20.0)
        {
            continue;
        }

        if (distanceX < closestDistance)
        {
            closestDistance = distanceX;
            pClosestKeg = pKeg;
        }
    }

    return pClosestKeg;
}

bool PowderKegComponent::CanBeLifted()
{
    return m_State == PowderKegState_Idle &&
        !m_bPendingDrop &&
        m_pHealthComponent != NULL &&
        m_pHealthComponent->GetHealth() > 0;
}

void PowderKegComponent::OnLifted(int carrierActorId)
{
    assert(CanBeLifted());

    m_CarrierActorId = carrierActorId;
    m_CarryPosition = m_pOwner->GetPositionComponent()->GetPosition();
    m_State = PowderKegState_Carried;
    m_bPendingLift = true;

    SetLiftedAnimation(false);
}

void PowderKegComponent::SetLiftedAnimation(bool bAboveHead)
{
    m_pAnimationComponent->SetAnimation(bAboveHead ? "lifted2" : "lifted");
    m_pAnimationComponent->PauseAnimation();
}

void PowderKegComponent::OnThrown(Direction direction)
{
    if (m_State != PowderKegState_Carried)
    {
        return;
    }

    m_ThrowDirection = direction;
    m_bPendingThrow = true;
}

void PowderKegComponent::OnDropped()
{
    if (m_State != PowderKegState_Carried)
    {
        return;
    }

    m_bPendingDrop = true;
}

void PowderKegComponent::OnThrownKegCollided()
{
    if (m_State == PowderKegState_Thrown)
    {
        m_bPendingExplode = true;
    }
}

void PowderKegComponent::ApplyCarryPosition()
{
    // Physics sync then moves the actor and its scene node to the new body position
    m_pPhysics->VSetPosition(m_pOwner->GetGUID(), m_CarryPosition);
}

uint32 PowderKegComponent::GetKegActorId() const
{
    return m_pOwner->GetGUID();
}
