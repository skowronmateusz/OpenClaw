#ifndef __EXPLOSION_COMPONENT_H__
#define __EXPLOSION_COMPONENT_H__

#include "../../SharedDefines.h"
#include "../ActorComponent.h"
#include "ControllerComponents/HealthComponent.h"
#include "PickupComponents/PickupComponent.h"

class AreaDamageComponent : public PickupComponent
{
public:
    AreaDamageComponent();

    static const char* g_Name;
    virtual const char* VGetName() const { return g_Name; }

    virtual void VUpdate(uint32 msDiff);

    virtual bool VOnApply(Actor* pActorWhoPickedThis);

    // Damage dealt to Claw when it should differ from the damage dealt to everything else
    void SetClawDamage(int32 clawDamage) { m_ClawDamage = clawDamage; }

protected:
    virtual bool VDelegateInit(TiXmlElement* data);
    virtual void VCreateInheritedXmlElements(TiXmlElement* pBaseElement);

private:
    int32 m_Damage;
    int32 m_ClawDamage;
    int32 m_Duration;
    Direction m_HitDirection;
    DamageType m_DamageType;
    int32 m_ActiveTime;
    int m_SourceActorId;
};

#endif