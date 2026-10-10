#ifndef __LIFTABLE_COMPONENT_H__
#define __LIFTABLE_COMPONENT_H__

#include "../../SharedDefines.h"
#include "../ActorComponent.h"

// Base for everything Claw can lift, carry above his head and throw (powder kegs, enemies).
// Concrete liftables register themselves so that they can be found by position or actor id
// without knowing their concrete component type.
// Physics changes must be deferred to VUpdate since lift / throw / drop / impact
// requests can come from Box2D callbacks where the physics world is locked.
class LiftableComponent : public ActorComponent
{
public:
    LiftableComponent();
    virtual ~LiftableComponent();

    virtual void VPostInit() override;

    // Returns closest liftable in front of given position or NULL
    static LiftableComponent* FindLiftable(const Point& position, Direction facingDirection, const Point& maxDistance);
    // Returns liftable component of given actor or NULL
    static LiftableComponent* GetLiftable(uint32 actorId);

    uint32 GetLiftableActorId() const { return m_LiftableActorId; }

    virtual bool VCanBeLifted() = 0;
    virtual void VOnLifted(int carrierActorId) = 0;
    // Offset from Claw's position (when he faces right) where the carried object is held above his head
    virtual Point VGetCarryOffset() const = 0;
    virtual void VSetCarryPosition(const Point& position, Direction carrierDirection, bool bAboveHead) = 0;
    virtual void VOnThrown(Direction direction) = 0;
    virtual void VOnDropped() = 0;

    virtual bool VIsThrown() const = 0;
    // Whether given own fixture hitting something counts as thrown object's impact
    virtual bool VIsImpactFixture(FixtureType fixtureType) const = 0;
    // Called from physics contact listener. pOtherActor is NULL when it hit level geometry, then
    // bLanded tells whether it fell onto ground (as opposed to hitting a wall / ceiling).
    // impactVelocity is the thrown object's velocity right before the impact
    virtual void VOnThrownImpact(Actor* pOtherActor, bool bLanded, const Point& impactVelocity) = 0;

private:
    uint32 m_LiftableActorId;

    static std::map<uint32, LiftableComponent*> s_Liftables;
};

#endif
