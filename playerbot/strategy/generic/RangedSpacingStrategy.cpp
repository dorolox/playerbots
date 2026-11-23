#include "playerbot/playerbot.h"
#include "RangedSpacingStrategy.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/values/PossibleAttackTargetsValue.h"

using namespace ai;

void RangedSpacingStrategy::InitCombatTriggers(std::list<TriggerNode*>& triggers)
{
    // Same level as the stock "enemy too close for spell" flee: above damage spells, below heals and dispels
    triggers.push_back(new TriggerNode(
        "ranged spacing",
        NextAction::array(0, new NextAction("ranged spacing", ACTION_MOVE), NULL)));
}

bool RangedSpacing::IsConcerned(PlayerbotAI* ai, Player* bot)
{
    if (!ai->IsRanged(bot) || ai->IsTank(bot))
        return false;

    if (ai->HasStrategy("stay", BotState::BOT_STATE_COMBAT) ||
        ai->HasStrategy("guard", BotState::BOT_STATE_COMBAT) ||
        ai->HasStrategy("follow", BotState::BOT_STATE_COMBAT))
        return false;

    return ai->CanMove();
}

std::list<Unit*> RangedSpacing::GetThreats(PlayerbotAI* ai, Player* bot, float range)
{
    std::list<Unit*> threats;

    AiObjectContext* context = ai->GetAiObjectContext();
    for (const ObjectGuid& guid : AI_VALUE(std::list<ObjectGuid>, "attackers"))
    {
        Unit* unit = ai->GetUnit(guid);
        if (!unit || !unit->IsAlive() || !bot->IsWithinDist(unit, range))
            continue;

        if (unit->GetVictim() == bot)
            continue;

        if (PossibleAttackTargetsValue::HasBreakableCC(unit, bot) || PossibleAttackTargetsValue::HasUnBreakableCC(unit, bot))
            continue;

        threats.push_back(unit);
    }

    return threats;
}

bool RangedSpacingTrigger::IsActive()
{
    if (!RangedSpacing::IsConcerned(ai, bot))
        return false;

    return !RangedSpacing::GetThreats(ai, bot, sPlayerbotAIConfig.rangedSpacingDistance).empty();
}

bool RangedSpacingAction::isUseful()
{
    // Do not ping-pong: one repositioning every few seconds at most
    return time(nullptr) - lastMoveTime >= 3;
}

bool RangedSpacingAction::Execute(Event& event)
{
    const float minDistance = sPlayerbotAIConfig.rangedSpacingDistance;
    // Aim a bit further than the trigger distance so that a small mob shuffle does not retrigger right away
    const float wantedDistance = minDistance + 3.0f;
    const float maxTravel = 20.0f;

    // Pack centre: every threat within twice the trigger distance
    std::list<Unit*> threats = RangedSpacing::GetThreats(ai, bot, minDistance * 2.0f);
    if (threats.empty())
        return false;

    float cx = 0.0f, cy = 0.0f;
    for (Unit* unit : threats)
    {
        cx += unit->GetPositionX();
        cy += unit->GetPositionY();
    }
    cx /= threats.size();
    cy /= threats.size();

    Unit* target = ai->GetUnit(AI_VALUE(ObjectGuid, "current target"));
    const bool isHealer = ai->IsHeal(bot);
    const float keepRange = (isHealer ? 30.0f : ai->GetRange("spell")) - 1.0f;

    std::list<ObjectGuid> attackers = AI_VALUE(std::list<ObjectGuid>, "attackers");

    // Also back off after a failed search so a cornered bot does not redo it every tick
    lastMoveTime = time(nullptr);

    // Try straight away from the pack first, then fan out on both sides
    const float baseAngle = atan2(bot->GetPositionY() - cy, bot->GetPositionX() - cx);
    static const float offsets[] = { 0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f, 2.0f, -2.0f };

    for (float offset : offsets)
    {
        const float angle = baseAngle + offset;
        float x = cx + cos(angle) * wantedDistance;
        float y = cy + sin(angle) * wantedDistance;
        float z = bot->GetPositionZ();
        bot->UpdateAllowedPositionZ(x, y, z);

        // Avoid cliffs and ledges
        if (fabs(z - bot->GetPositionZ()) > 5.0f)
            continue;

        if (bot->GetDistance2d(x, y) > maxTravel)
            continue;

        const TerrainInfo* terrain = bot->GetMap()->GetTerrain();
        if (terrain && terrain->IsInWater(x, y, z))
            continue;

        if (!bot->IsWithinLOS(x, y, z + bot->GetCollisionHeight(), true))
            continue;

        // Keep doing the job from the new spot: the target (or the pack for healers) must stay in range and line of sight
        if (target)
        {
            if (target->GetDistance2d(x, y) > keepRange)
                continue;

            if (!target->IsWithinLOS(x, y, z + bot->GetCollisionHeight(), true))
                continue;
        }
        else if (sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) > keepRange)
            continue;

        // The spot must really be clear of every attacker, including the ones on the bot
        bool clear = true;
        for (const ObjectGuid& guid : attackers)
        {
            Unit* unit = ai->GetUnit(guid);
            if (unit && unit->IsAlive() && unit->GetDistance2d(x, y) < minDistance)
            {
                clear = false;
                break;
            }
        }

        if (!clear)
            continue;

        if (MoveTo(bot->GetMapId(), x, y, z))
            return true;
    }

    return false;
}
