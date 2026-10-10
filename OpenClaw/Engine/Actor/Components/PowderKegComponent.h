#ifndef __POWDER_KEG_COMPONENT_H__
#define __POWDER_KEG_COMPONENT_H__

#include "../../SharedDefines.h"
#include "../ActorComponent.h"
#include "ControllerComponents/HealthComponent.h"
#include "LiftableComponent.h"

class AnimationComponent;

enum PowderKegState
{
    PowderKegState_Idle,
    PowderKegState_Carried,
    PowderKegState_Thrown,
    PowderKegState_Exploded
};

// Makes powder keg liftable and throwable by Claw. Like in the original game the thrown keg damages
// enemies it flies through and explodes when it hits level geometry.
class PowderKegComponent : public LiftableComponent, public HealthObserver
{
public:
    PowderKegComponent();
    virtual ~PowderKegComponent();

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
    virtual Point VGetCarryOffset() const override { return Point(8, -62); }
    virtual void VSetCarryPosition(const Point& position, Direction carrierDirection, bool bAboveHead) override;
    virtual void VOnThrown(Direction direction) override;
    virtual void VOnDropped() override;
    virtual bool VIsThrown() const override { return m_State == PowderKegState_Thrown; }
    virtual bool VIsImpactFixture(FixtureType fixtureType) const override;
    virtual void VOnThrownImpact(Actor* pOtherActor, bool bLanded, const Point& impactVelocity) override;

private:
    void ApplyCarryPosition();

    // XML Data
    Point m_ThrowSpeed;
    int m_ThrowDamage;
    float m_ThrowGravityScale;
    int m_MaxFlyTime;

    // Internal state
    PowderKegState m_State;
    int m_CarrierActorId;
    Point m_CarryPosition;
    Direction m_ThrowDirection;
    int m_FlyTime;

    bool m_bPendingLift;
    bool m_bPendingThrow;
    bool m_bPendingDrop;
    bool m_bPendingExplode;
    std::vector<uint32> m_HitActorIds;
    std::vector<uint32> m_PendingHitActorIds;

    shared_ptr<IGamePhysics> m_pPhysics;
    AnimationComponent* m_pAnimationComponent;
    HealthComponent* m_pHealthComponent;
};

#endif
