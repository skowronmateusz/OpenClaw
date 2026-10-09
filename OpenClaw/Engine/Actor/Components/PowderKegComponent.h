#ifndef __POWDER_KEG_COMPONENT_H__
#define __POWDER_KEG_COMPONENT_H__

#include "../../SharedDefines.h"
#include "../ActorComponent.h"
#include "ControllerComponents/HealthComponent.h"

class AnimationComponent;

enum PowderKegState
{
    PowderKegState_Idle,
    PowderKegState_Carried,
    PowderKegState_Thrown,
    PowderKegState_Exploded
};

// Makes powder keg liftable and throwable by Claw.
// All physics changes are applied in VUpdate since lift / throw / drop / impact
// requests can come from Box2D contact callbacks where the physics world is locked.
class PowderKegComponent : public ActorComponent, public HealthObserver
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

    // Returns closest keg in front of given position which can be lifted or NULL
    static PowderKegComponent* FindLiftableKeg(const Point& position, Direction facingDirection, const Point& maxDistance);

    uint32 GetKegActorId() const;

    bool CanBeLifted();
    bool IsThrown() const { return m_State == PowderKegState_Thrown; }

    void OnLifted(int carrierActorId);
    void SetCarryPosition(const Point& position) { m_CarryPosition = position; }
    void SetLiftedAnimation(bool bAboveHead);
    void OnThrown(Direction direction);
    void OnDropped();

    // Called from physics contact listener when thrown keg hits something
    void OnThrownKegCollided();

private:
    void ApplyCarryPosition();

    // XML Data
    Point m_ThrowSpeed;
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

    shared_ptr<IGamePhysics> m_pPhysics;
    AnimationComponent* m_pAnimationComponent;
    HealthComponent* m_pHealthComponent;

    static std::vector<PowderKegComponent*> s_PowderKegs;
};

#endif
