#pragma once
#include "playerbot/strategy/Strategy.h"
#include "playerbot/strategy/Trigger.h"
#include "playerbot/strategy/actions/MovementActions.h"

// Ranged spacing (custom, doro-changes)
//
// Keeps ranged dps and healers out of the melee pack so they stop eating cleaves and
// tank-centred AoE. The stock "enemy too close for spell" trigger only looks at the bot's
// current target; this module looks at every attacker around the bot.
//
// Self-contained on purpose so it can be swapped against an upstream rework:
//   - strategy "ranged spacing" (added by AiFactory when AiPlayerbot.RangedSpacing = 1)
//   - trigger  "ranged spacing"
//   - action   "ranged spacing"
// Compare in game with whisper "co -ranged spacing" / "co +ranged spacing".

namespace ai
{
    class RangedSpacingStrategy : public Strategy
    {
    public:
        RangedSpacingStrategy(PlayerbotAI* ai) : Strategy(ai) {}
        std::string getName() override { return "ranged spacing"; }

    private:
        void InitCombatTriggers(std::list<TriggerNode*>& triggers) override;
    };

    // Active when a ranged non-tank bot has an attacker closer than AiPlayerbot.RangedSpacingDistance
    class RangedSpacingTrigger : public Trigger
    {
    public:
        RangedSpacingTrigger(PlayerbotAI* ai) : Trigger(ai, "ranged spacing") {}
        bool IsActive() override;
    };

    // Moves away from the pack to a spot that keeps the current target in spell range and line of sight
    class RangedSpacingAction : public MovementAction
    {
    public:
        RangedSpacingAction(PlayerbotAI* ai) : MovementAction(ai, "ranged spacing") {}
        bool Execute(Event& event) override;
        bool isUseful() override;

    private:
        time_t lastMoveTime = 0;
    };

    namespace RangedSpacing
    {
        // Ranged, not a tank, free to move
        bool IsConcerned(PlayerbotAI* ai, Player* bot);

        // Alive attackers within range that are a reason to step back:
        // not crowd-controlled and not attacking the bot itself (running from those is pointless, the tank must pick them up)
        std::list<Unit*> GetThreats(PlayerbotAI* ai, Player* bot, float range);
    }
}
