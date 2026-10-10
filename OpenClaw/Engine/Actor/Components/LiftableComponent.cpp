#include "LiftableComponent.h"
#include "PositionComponent.h"
#include "../Actor.h"

std::map<uint32, LiftableComponent*> LiftableComponent::s_Liftables;

LiftableComponent::LiftableComponent()
    :
    m_LiftableActorId(INVALID_ACTOR_ID)
{ }

LiftableComponent::~LiftableComponent()
{
    auto findIt = s_Liftables.find(m_LiftableActorId);
    if (findIt != s_Liftables.end() && findIt->second == this)
    {
        s_Liftables.erase(findIt);
    }
}

void LiftableComponent::VPostInit()
{
    m_LiftableActorId = m_pOwner->GetGUID();
    s_Liftables[m_LiftableActorId] = this;
}

LiftableComponent* LiftableComponent::FindLiftable(const Point& position, Direction facingDirection, const Point& maxDistance)
{
    LiftableComponent* pClosest = NULL;
    double closestDistance = DBL_MAX;
    for (const auto& liftableIter : s_Liftables)
    {
        LiftableComponent* pLiftable = liftableIter.second;
        if (!pLiftable->VCanBeLifted())
        {
            continue;
        }

        Point liftablePosition = pLiftable->m_pOwner->GetPositionComponent()->GetPosition();
        double distanceX = fabs(liftablePosition.x - position.x);
        double distanceY = fabs(liftablePosition.y - position.y);
        if (distanceX > maxDistance.x || distanceY > maxDistance.y)
        {
            continue;
        }

        // Claw has to face the object, only a small overlap behind him is tolerated
        double distanceInFront = liftablePosition.x - position.x;
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
            pClosest = pLiftable;
        }
    }

    return pClosest;
}

LiftableComponent* LiftableComponent::GetLiftable(uint32 actorId)
{
    auto findIt = s_Liftables.find(actorId);
    return findIt != s_Liftables.end() ? findIt->second : NULL;
}
