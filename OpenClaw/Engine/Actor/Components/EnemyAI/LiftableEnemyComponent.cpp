#include "LiftableEnemyComponent.h"
#include "EnemyAIComponent.h"
#include "EnemyAIStateComponent.h"
#include "../AnimationComponent.h"
#include "../PositionComponent.h"
#include "../PhysicsComponent.h"
#include "../RenderComponent.h"
#include "../../../GameApp/BaseGameApp.h"
#include "../../../GameApp/BaseGameLogic.h"

const char* LiftableEnemyComponent::g_Name = "LiftableEnemyComponent";

LiftableEnemyComponent::LiftableEnemyComponent()
    :
    m_ThrowSpeed(Point(7.0, -1.5)),
    m_CarryOffset(Point(5, -78)),
    m_ThrowGravityScale(1.0f),
    // Original game: thrown enemy loses 4 health, hit enemy loses thrown enemy's attack damage (10 for
    // most enemies)
    m_ThrowDamage(10),
    m_ThrowSelfDamage(4),
    m_MaxFlyTime(2000),
    m_MaxBounces(2),
    m_BounceSpeedFactor(1.0f / 3.0f),
    m_PauseTime(500),
    m_RecoveryTime(1500),
    m_State(LiftableEnemyState_Free),
    m_bHasLiftAnimations(false),
    m_CarrierActorId(INVALID_ACTOR_ID),
    m_bCarryPositionSet(false),
    m_ThrowDirection(Direction_Right),
    m_FlyTime(0),
    m_BounceCount(0),
    m_RestTime(0),
    m_PauseTimeLeft(0),
    m_RecoveryTimeLeft(0),
    m_bPendingLift(false),
    m_bPendingThrow(false),
    m_bPendingDrop(false),
    m_bPendingLanding(false),
    m_bPendingWallHit(false),
    m_pAnimationComponent(NULL),
    m_pHealthComponent(NULL),
    m_pEnemyAIComponent(NULL)
{ }

LiftableEnemyComponent::~LiftableEnemyComponent()
{ }

bool LiftableEnemyComponent::VInit(TiXmlElement* pData)
{
    assert(pData != NULL);

    m_pPhysics = g_pApp->GetGameLogic()->VGetGamePhysics();
    if (!m_pPhysics)
    {
        LOG_WARNING("Attemtping to create liftable enemy component without valid physics");
        return false;
    }

    ParseValueFromXmlElem(&m_ThrowSpeed, pData->FirstChildElement("ThrowSpeed"), "x", "y");
    ParseValueFromXmlElem(&m_CarryOffset, pData->FirstChildElement("CarryOffset"), "x", "y");
    ParseValueFromXmlElem(&m_ThrowGravityScale, pData->FirstChildElement("ThrowGravityScale"));
    ParseValueFromXmlElem(&m_ThrowDamage, pData->FirstChildElement("ThrowDamage"));
    ParseValueFromXmlElem(&m_ThrowSelfDamage, pData->FirstChildElement("ThrowSelfDamage"));
    ParseValueFromXmlElem(&m_MaxFlyTime, pData->FirstChildElement("MaxFlyTime"));
    ParseValueFromXmlElem(&m_MaxBounces, pData->FirstChildElement("MaxBounces"));
    ParseValueFromXmlElem(&m_BounceSpeedFactor, pData->FirstChildElement("BounceSpeedFactor"));
    ParseValueFromXmlElem(&m_PauseTime, pData->FirstChildElement("PauseTime"));
    ParseValueFromXmlElem(&m_RecoveryTime, pData->FirstChildElement("RecoveryTime"));

    return true;
}

void LiftableEnemyComponent::VPostInit()
{
    LiftableComponent::VPostInit();

    m_pAnimationComponent = MakeStrongPtr(m_pOwner->GetComponent<AnimationComponent>()).get();
    m_pHealthComponent = MakeStrongPtr(m_pOwner->GetComponent<HealthComponent>()).get();
    m_pEnemyAIComponent = MakeStrongPtr(m_pOwner->GetComponent<EnemyAIComponent>()).get();
    assert(m_pAnimationComponent != NULL);
    assert(m_pHealthComponent != NULL);
    assert(m_pEnemyAIComponent != NULL);

    m_pHealthComponent->AddObserver(this);

    // Early levels name the grab animation "lifted", later ones "lifted1"
    std::string liftedAnim = "lifted";
    std::string liftedAnim1 = "lifted1";
    std::string carriedAnim = "lifted2";
    std::string thrownAnim = "thrown";
    if (m_pAnimationComponent->HasAnimation(liftedAnim))
    {
        m_LiftedAnimation = liftedAnim;
    }
    else if (m_pAnimationComponent->HasAnimation(liftedAnim1))
    {
        m_LiftedAnimation = liftedAnim1;
    }
    if (m_pAnimationComponent->HasAnimation(carriedAnim))
    {
        m_CarriedAnimation = carriedAnim;
    }
    if (m_pAnimationComponent->HasAnimation(thrownAnim))
    {
        m_ThrownAnimation = thrownAnim;
    }

    // Enemies without full set of lift animations cannot be lifted
    m_bHasLiftAnimations = !m_LiftedAnimation.empty() && !m_CarriedAnimation.empty() && !m_ThrownAnimation.empty();
}

TiXmlElement* LiftableEnemyComponent::VGenerateXml()
{
    TiXmlElement* pBaseElement = new TiXmlElement(VGetName());

    XML_ADD_2_PARAM_ELEMENT("ThrowSpeed", "x", ToStr(m_ThrowSpeed.x).c_str(), "y", ToStr(m_ThrowSpeed.y).c_str(), pBaseElement);
    XML_ADD_2_PARAM_ELEMENT("CarryOffset", "x", ToStr(m_CarryOffset.x).c_str(), "y", ToStr(m_CarryOffset.y).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("ThrowGravityScale", ToStr(m_ThrowGravityScale).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("ThrowDamage", ToStr(m_ThrowDamage).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("ThrowSelfDamage", ToStr(m_ThrowSelfDamage).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("MaxFlyTime", ToStr(m_MaxFlyTime).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("MaxBounces", ToStr(m_MaxBounces).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("BounceSpeedFactor", ToStr(m_BounceSpeedFactor).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("PauseTime", ToStr(m_PauseTime).c_str(), pBaseElement);
    XML_ADD_TEXT_ELEMENT("RecoveryTime", ToStr(m_RecoveryTime).c_str(), pBaseElement);

    return pBaseElement;
}

void LiftableEnemyComponent::VUpdate(uint32 msDiff)
{
    if (m_State == LiftableEnemyState_Dead)
    {
        return;
    }

    const uint32 enemyId = m_pOwner->GetGUID();

    if (m_RecoveryTimeLeft > 0)
    {
        m_RecoveryTimeLeft -= msDiff;
    }

    if (m_bPendingLift)
    {
        m_bPendingLift = false;

        // While carried the enemy does not take part in physics simulation, it just follows Claw
        m_pPhysics->VSetLinearSpeed(enemyId, Point(0, 0));
        m_pPhysics->VDeactivate(enemyId);
    }

    if (m_State == LiftableEnemyState_Carried && m_bCarryPositionSet)
    {
        m_pPhysics->VSetPosition(enemyId, m_CarryPosition);
    }

    if (m_bPendingThrow)
    {
        m_bPendingThrow = false;

        Point speed = m_ThrowSpeed;
        if (m_ThrowDirection == Direction_Left)
        {
            speed.x *= -1.0;
        }

        m_pPhysics->VSetPosition(enemyId, m_CarryPosition);
        m_pPhysics->VActivate(enemyId);
        // Restored when the enemy gets up, it can survive the throw
        m_SavedCollisionFilters = m_pPhysics->VGetCollisionFilters(enemyId);
        std::vector<std::pair<uint32_t, uint32_t>> thrownFilters = m_SavedCollisionFilters;
        for (auto& filter : thrownFilters)
        {
            const uint32_t clawFlags = CollisionFlag_Controller | CollisionFlag_InvisibleController;
            if ((filter.second & ~clawFlags) == 0)
            {
                // Agro range sensors (they only look for Claw) must not sense anything during the flight
                filter.second = 0;
            }
            else
            {
                // Body and hitbox behave like a projectile - other enemies' hitboxes react to it
                if (filter.first & CollisionFlag_DynamicActor)
                {
                    filter.first = (filter.first & ~CollisionFlag_DynamicActor) | CollisionFlag_Bullet;
                }
                filter.second = CollisionFlag_Solid |
                    CollisionFlag_Ground |
                    CollisionFlag_Death |
                    CollisionFlag_DynamicActor |
                    CollisionFlag_Crate |
                    CollisionFlag_PowderKeg;
            }
        }
        m_pPhysics->VSetCollisionFilters(enemyId, thrownFilters);
        m_pPhysics->VSetGravityScale(enemyId, m_ThrowGravityScale);
        m_pPhysics->VSetLinearSpeed(enemyId, speed);

        m_pAnimationComponent->SetAnimation(m_ThrownAnimation);

        m_FlyTime = 0;
        m_BounceCount = 0;
        m_RestTime = 0;
        m_HitActorIds.clear();
        m_State = LiftableEnemyState_Thrown;
    }

    if (m_bPendingDrop)
    {
        m_bPendingDrop = false;

        if (m_bCarryPositionSet)
        {
            m_pPhysics->VSetPosition(enemyId, m_CarryPosition);
        }
        m_pPhysics->VActivate(enemyId);
        m_pPhysics->VSetLinearSpeed(enemyId, Point(0, 0));

        m_State = LiftableEnemyState_Free;
        m_pEnemyAIComponent->SetLifted(false);
    }

    // Enemies (and crates, kegs) the thrown enemy flies through lose its attack damage, it flies on
    for (uint32 hitActorId : m_PendingHitActorIds)
    {
        StrongActorPtr pHitActor = MakeStrongPtr(g_pApp->GetGameLogic()->VGetActor(hitActorId));
        shared_ptr<HealthComponent> pHitHealth = pHitActor ? MakeStrongPtr(pHitActor->GetComponent<HealthComponent>()) : nullptr;
        if (pHitHealth)
        {
            pHitHealth->AddHealth(-1 * m_ThrowDamage, DamageType_MeleeAttack, m_pOwner->GetPositionComponent()->GetPosition(), m_CarrierActorId);
        }
    }
    m_PendingHitActorIds.clear();

    if (m_State == LiftableEnemyState_Thrown)
    {
        if (m_bPendingWallHit)
        {
            m_bPendingWallHit = false;

            // Hitting a wall stops the flight forward, the enemy falls down
            Point velocity = m_pPhysics->VGetVelocity(enemyId);
            m_pPhysics->VSetLinearSpeed(enemyId, Point(0, velocity.y > 0 ? velocity.y : 0));
        }

        // Last low bounce may not even leave the ground (no new contact) - resting counts as landing
        if (m_BounceCount > 0 && !m_bPendingLanding)
        {
            Point velocity = m_pPhysics->VGetVelocity(enemyId);
            m_RestTime = fabs(velocity.y) < 0.05 ? m_RestTime + msDiff : 0;
            if (m_RestTime > 100)
            {
                m_LandingVelocity = velocity;
                m_bPendingLanding = true;
            }
        }

        if (m_bPendingLanding)
        {
            m_bPendingLanding = false;
            m_RestTime = 0;
            m_BounceCount++;

            if (m_BounceCount <= m_MaxBounces)
            {
                // Bounce off the ground with 1/3 of the speed
                Point bounceSpeed(
                    m_LandingVelocity.x * m_BounceSpeedFactor,
                    -1.0 * fabs(m_LandingVelocity.y) * m_BounceSpeedFactor);
                m_pPhysics->VSetLinearSpeed(enemyId, bounceSpeed);
            }
            else
            {
                // Stop and lie on the ground for a while
                m_pPhysics->VSetLinearSpeed(enemyId, Point(0, 0));
                m_PauseTimeLeft = m_PauseTime;
                m_State = LiftableEnemyState_Paused;
            }
        }
        else if (m_BounceCount == 0)
        {
            // Still in the air long after the throw - it fell off somewhere
            m_FlyTime += msDiff;
            if (m_FlyTime > m_MaxFlyTime)
            {
                m_pHealthComponent->AddHealth(
                    -1 * (m_pHealthComponent->GetHealth() + 1),
                    DamageType_MeleeAttack,
                    m_pOwner->GetPositionComponent()->GetPosition(),
                    m_CarrierActorId);
                return;
            }
        }
    }

    if (m_State == LiftableEnemyState_Paused)
    {
        m_bPendingLanding = m_bPendingWallHit = false;

        m_PauseTimeLeft -= msDiff;
        if (m_PauseTimeLeft <= 0)
        {
            Recover();
        }
    }
}

void LiftableEnemyComponent::Recover()
{
    const uint32 enemyId = m_pOwner->GetGUID();
    Point position = m_pOwner->GetPositionComponent()->GetPosition();

    // Enemy is a regular enemy again ...
    m_pPhysics->VSetCollisionFilters(enemyId, m_SavedCollisionFilters);
    m_pOwner->GetPhysicsComponent()->RestoreGravityScale();

    m_State = LiftableEnemyState_Free;
    m_RecoveryTimeLeft = m_RecoveryTime;
    m_pEnemyAIComponent->SetLifted(false);

    // ... and loses part of its health. Taking damage interrupts it like a regular hit, it can also die
    m_pHealthComponent->AddHealth(-1 * m_ThrowSelfDamage, DamageType_MeleeAttack, position, m_CarrierActorId);
}

void LiftableEnemyComponent::VOnHealthBelowZero(DamageType damageType, int sourceActorId)
{
    m_State = LiftableEnemyState_Dead;
    m_bPendingLift = m_bPendingThrow = m_bPendingDrop = m_bPendingLanding = m_bPendingWallHit = false;
}

bool LiftableEnemyComponent::VCanBeLifted()
{
    if (!m_bHasLiftAnimations ||
        m_State != LiftableEnemyState_Free ||
        m_bPendingDrop ||
        m_RecoveryTimeLeft > 0 ||
        m_pEnemyAIComponent->IsDead() ||
        m_pEnemyAIComponent->IsLifted() ||
        m_pHealthComponent->GetHealth() <= 0)
    {
        return false;
    }

    // Original game does not let Claw grab an enemy in the middle of its strike or jump
    if (BaseEnemyAIStateComponent* pState = m_pEnemyAIComponent->GetCurrentState())
    {
        EnemyAIState stateType = pState->VGetStateType();
        if (stateType == EnemyAIState_Falling ||
            stateType == EnemyAIState_DiveAttacking)
        {
            return false;
        }

        bool bIsAttacking = stateType == EnemyAIState_MeleeAttacking ||
            stateType == EnemyAIState_DuckMeleeAttacking ||
            stateType == EnemyAIState_RangedAttacking ||
            stateType == EnemyAIState_DuckRangedAttacking;
        // Original enemy leaves its strike state between two strikes, OpenClaw's enemy keeps striking.
        // So only the swing (first half of the strike animation) protects it, the rest of the strike
        // and the pause between strikes do not
        if (bIsAttacking)
        {
            shared_ptr<Animation> pAnimation = m_pAnimationComponent->GetCurrentAnimation();
            bool bIsSwinging = !pAnimation->IsDelayed() &&
                (pAnimation->GetCurrentAnimationFrame()->idx * 2 < pAnimation->GetAnimFrames().size());
            if (bIsSwinging)
            {
                return false;
            }
        }
    }

    return true;
}

void LiftableEnemyComponent::VOnLifted(int carrierActorId)
{
    assert(VCanBeLifted());

    m_CarrierActorId = carrierActorId;
    m_CarryPosition = m_pOwner->GetPositionComponent()->GetPosition();
    m_bCarryPositionSet = false;
    m_State = LiftableEnemyState_Carried;
    m_bPendingLift = true;

    // AI has to stop immediately - no attacking Claw while being grabbed
    m_pEnemyAIComponent->SetLifted(true);

    m_pAnimationComponent->SetAnimation(m_LiftedAnimation);
    m_pAnimationComponent->ResumeAnimation();
}

void LiftableEnemyComponent::VSetCarryPosition(const Point& position, Direction carrierDirection, bool bAboveHead)
{
    if (m_State != LiftableEnemyState_Carried || !bAboveHead)
    {
        return;
    }

    m_CarryPosition = position;
    m_bCarryPositionSet = true;

    m_pAnimationComponent->SetAnimation(m_CarriedAnimation);
    m_pAnimationComponent->ResumeAnimation();
}

void LiftableEnemyComponent::VOnThrown(Direction direction)
{
    if (m_State != LiftableEnemyState_Carried)
    {
        return;
    }

    m_ThrowDirection = direction;
    m_bPendingThrow = true;
}

void LiftableEnemyComponent::VOnDropped()
{
    if (m_State != LiftableEnemyState_Carried)
    {
        return;
    }

    m_bPendingDrop = true;
}

bool LiftableEnemyComponent::VIsImpactFixture(FixtureType fixtureType) const
{
    // Only the body, not the agro range sensors
    return fixtureType == FixtureType_EnemyAI;
}

void LiftableEnemyComponent::VOnThrownImpact(Actor* pOtherActor, bool bLanded, const Point& impactVelocity)
{
    if (m_State != LiftableEnemyState_Thrown)
    {
        return;
    }

    if (pOtherActor != NULL)
    {
        uint32 hitActorId = pOtherActor->GetGUID();
        if (std::find(m_HitActorIds.begin(), m_HitActorIds.end(), hitActorId) == m_HitActorIds.end())
        {
            m_HitActorIds.push_back(hitActorId);
            m_PendingHitActorIds.push_back(hitActorId);
        }
        return;
    }

    if (bLanded)
    {
        m_LandingVelocity = impactVelocity;
        m_bPendingLanding = true;
    }
    else
    {
        m_bPendingWallHit = true;
    }
}
