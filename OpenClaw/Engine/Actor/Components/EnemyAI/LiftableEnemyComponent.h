#ifndef __LIFTABLE_ENEMY_COMPONENT_H__
#define __LIFTABLE_ENEMY_COMPONENT_H__

#include "../../../SharedDefines.h"
#include "../LiftableComponent.h"
#include "../ControllerComponents/HealthComponent.h"

class AnimationComponent;
class EnemyAIComponent;

enum LiftableEnemyState
{
    LiftableEnemyState_Free,
    LiftableEnemyState_Carried,
    LiftableEnemyState_Thrown,
    LiftableEnemyState_Paused,
    LiftableEnemyState_Dead
};

// Lets Claw lift a (non-boss) enemy above his head and throw it. Follows the original game (CLAW.EXE):
//  - enemy cannot be lifted while striking / falling and for 1.5s after it recovered from a throw
//  - every enemy the thrown one flies through loses the thrown enemy's attack damage
//  - thrown enemy bounces off the ground twice (velocity * 1/3), stops on the third landing,
//    lies for 0.5s, then loses 4 (original units) health - it dies or gets up and fights on
//  - thrown enemy which is still in the air 2s after the throw (fell off) dies
// Enemy uses its lifted / lifted1, lifted2 and thrown animations.
class LiftableEnemyComponent : public LiftableComponent, public HealthObserver
{
public:
    LiftableEnemyComponent();
    virtual ~LiftableEnemyComponent();

    static const char* g_Name;
    virtual const char* VGetName() const override { return g_Name; }

    virtual bool VInit(TiXmlElement* pData) override;
    virtual void VPostInit() override;
    virtual TiXmlElement* VGenerateXml() override;
    virtual void VUpdate(uint32 msDiff) override;

    // HealthObserver API
    virtual void VOnHealthBelowZero(DamageType damageType, int sourceActorId) override;

    // LiftableComponent API
    virtual bool VCanBeLifted() override;
    virtual void VOnLifted(int carrierActorId) override;
    virtual Point VGetCarryOffset() const override { return m_CarryOffset; }
    virtual void VSetCarryPosition(const Point& position, Direction carrierDirection, bool bAboveHead) override;
    virtual void VOnThrown(Direction direction) override;
    virtual void VOnDropped() override;
    virtual bool VIsThrown() const override { return m_State == LiftableEnemyState_Thrown; }
    virtual bool VIsImpactFixture(FixtureType fixtureType) const override;
    virtual void VOnThrownImpact(Actor* pOtherActor, bool bLanded, const Point& impactVelocity) override;

private:
    void Recover();

    // XML Data
    Point m_ThrowSpeed;
    Point m_CarryOffset;
    float m_ThrowGravityScale;
    int m_ThrowDamage;
    int m_ThrowSelfDamage;
    int m_MaxFlyTime;
    int m_MaxBounces;
    float m_BounceSpeedFactor;
    int m_PauseTime;
    int m_RecoveryTime;

    // Internal state
    LiftableEnemyState m_State;
    std::string m_LiftedAnimation;
    std::string m_CarriedAnimation;
    std::string m_ThrownAnimation;
    bool m_bHasLiftAnimations;

    int m_CarrierActorId;
    Point m_CarryPosition;
    bool m_bCarryPositionSet;
    Direction m_ThrowDirection;
    int m_FlyTime;
    int m_BounceCount;
    int m_RestTime;
    int m_PauseTimeLeft;
    int m_RecoveryTimeLeft;
    Point m_LandingVelocity;
    std::vector<uint32> m_HitActorIds;
    std::vector<uint32> m_PendingHitActorIds;
    std::vector<std::pair<uint32_t, uint32_t>> m_SavedCollisionFilters;

    bool m_bPendingLift;
    bool m_bPendingThrow;
    bool m_bPendingDrop;
    bool m_bPendingLanding;
    bool m_bPendingWallHit;

    shared_ptr<IGamePhysics> m_pPhysics;
    AnimationComponent* m_pAnimationComponent;
    HealthComponent* m_pHealthComponent;
    EnemyAIComponent* m_pEnemyAIComponent;
};

#endif
