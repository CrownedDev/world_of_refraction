// EffectText.cpp
// Wording follows wor-deckbuilder docs/reference/07-text-grammar.md: imperative bodies, the
// side as the verb's object, "for N turns" for standing effects and "each turn for N turns" for
// payouts. This game's own triggers, stats and effect kinds are worded in Q-045.

#include "Skills/Effects/EffectText.h"
#include "Skills/Effects/SkillTriggerUtils.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "EffectText"

namespace
{
    constexpr float PERCENT_SCALE = 100.0f;
    constexpr int32 INDENT_SPACES_PER_LEVEL = 2;
    constexpr int32 ACTIVATE_BLOCK = 0;
    constexpr int32 TRIGGERED_BLOCK = 1;
    constexpr int32 PASSIVE_BLOCK = 2;

    /** ModifyEnergyCost: positive is cheaper, like SpellCostBuff (ActionExecutor's cost multiplier). */
    bool IsCostCut(const FSkillEffectPayload &P)
    {
        return P.EffectType == ESkillEffectType::SpellCostBuff
               || (P.EffectType == ESkillEffectType::ModifyEnergyCost && (P.Value != 0 ? P.Value > 0 : P.Magnitude >= 0.0f));
    }

    /** The recipient of a payload, as the verb's object ("an Enemy", "2 Allies", "all Enemies"). */
    struct FSide
    {
        bool bSelf = false;
        bool bPlural = false;
        FText Noun;
        FText Possessive;
    };

    FSide GetSide(ETargetType Target, ETargetCount Count)
    {
        FSide Side;
        Side.bPlural = Count != ETargetCount::Single;
        switch (Target)
        {
        case ETargetType::Self:
            Side.bSelf = true;
            Side.bPlural = false;
            break;
        case ETargetType::Ally:
            Side.Noun = Count == ETargetCount::Single ? LOCTEXT("Side_Ally", "an Ally")
                        : Count == ETargetCount::Double ? LOCTEXT("Side_TwoAllies", "2 Allies")
                                                        : LOCTEXT("Side_AllAllies", "all Allies");
            Side.Possessive = Count == ETargetCount::Single ? LOCTEXT("Side_AllyPoss", "an Ally's")
                              : Count == ETargetCount::Double ? LOCTEXT("Side_TwoAlliesPoss", "2 Allies'")
                                                              : LOCTEXT("Side_AllAlliesPoss", "all Allies'");
            break;
        case ETargetType::Enemy:
            Side.Noun = Count == ETargetCount::Single ? LOCTEXT("Side_Enemy", "an Enemy")
                        : Count == ETargetCount::Double ? LOCTEXT("Side_TwoEnemies", "2 Enemies")
                                                        : LOCTEXT("Side_AllEnemies", "all Enemies");
            Side.Possessive = Count == ETargetCount::Single ? LOCTEXT("Side_EnemyPoss", "an Enemy's")
                              : Count == ETargetCount::Double ? LOCTEXT("Side_TwoEnemiesPoss", "2 Enemies'")
                                                              : LOCTEXT("Side_AllEnemiesPoss", "all Enemies'");
            break;
        case ETargetType::Anyone:
            Side.Noun = Count == ETargetCount::Single ? LOCTEXT("Side_Anyone", "a character")
                        : Count == ETargetCount::Double ? LOCTEXT("Side_TwoAnyone", "2 characters")
                                                        : LOCTEXT("Side_AllAnyone", "all characters");
            Side.Possessive = Count == ETargetCount::Single ? LOCTEXT("Side_AnyonePoss", "a character's")
                              : Count == ETargetCount::Double ? LOCTEXT("Side_TwoAnyonePoss", "2 characters'")
                                                              : LOCTEXT("Side_AllAnyonePoss", "all characters'");
            break;
        }
        return Side;
    }

    FText Percent(float Fraction)
    {
        return FText::Format(LOCTEXT("Percent", "{0}%"), FText::AsNumber(FMath::RoundToInt(FMath::Abs(Fraction) * PERCENT_SCALE)));
    }

    /** A flat Value wins; otherwise the Magnitude as a percent (FSkillEffectPayload: "Don't set with Magnitude"). */
    FText Amount(const FSkillEffectPayload &P)
    {
        return P.Value != 0 ? FText::AsNumber(FMath::Abs(P.Value)) : Percent(P.Magnitude);
    }

    /** Ignore Defense's amount: a Value is percent points (D-086); none (or a 1 = on flag) is all of it. */
    FText IgnoreDefenseAmount(const FSkillEffectPayload &P)
    {
        constexpr float IGNORE_DEFENSE_FLAG_VALUE = 1.0f;
        if (FMath::Abs(P.Value) > IGNORE_DEFENSE_FLAG_VALUE)
        {
            return Percent(P.Value / PERCENT_SCALE);
        }
        if (P.Value == 0 && P.Magnitude > 0.0f)
        {
            return Percent(P.Magnitude);
        }
        return LOCTEXT("IgnoreDefense_All", "all");
    }

    /** "Fire " before a noun, "Elemental " for Generic (it takes the caster's element), nothing for None. */
    FText ElementPrefix(ESpellElement Element)
    {
        if (Element == ESpellElement::None)
        {
            return FText::GetEmpty();
        }
        if (Element == ESpellElement::Generic)
        {
            return LOCTEXT("ElementPrefix_Generic", "Elemental ");
        }
        return FText::Format(LOCTEXT("ElementPrefix", "{0} "), WoRGameNames::GetElementName(Element));
    }

    /** " for 3 turns" on a standing effect; nothing when instant or permanent. */
    FText Span(const FSkillEffectPayload &P)
    {
        if (P.bPermanent || P.Duration <= 0)
        {
            return FText::GetEmpty();
        }
        return FText::Format(LOCTEXT("Span", " for {0} {0}|plural(one=turn,other=turns)"), P.Duration);
    }

    /** " each turn for 3 turns" on a payout repeated every turn. */
    FText EachTurnSpan(const FSkillEffectPayload &P)
    {
        if (P.bPermanent)
        {
            return LOCTEXT("EachTurnPermanent", " each turn");
        }
        if (P.Duration <= 0)
        {
            return FText::GetEmpty();
        }
        return FText::Format(LOCTEXT("EachTurnSpan", " each turn for {0} {0}|plural(one=turn,other=turns)"), P.Duration);
    }

    /** The stats a payload gains, loses or gives: the stat's word and whether the type raises it. */
    bool GetStatChange(ESkillEffectType Type, float Magnitude, FText &OutStat, bool &bOutRaises)
    {
        bOutRaises = SkillEffectClassification::IsBuff(Type, Magnitude);
        switch (Type)
        {
        case ESkillEffectType::MindBuff: case ESkillEffectType::MindDebuff:
            OutStat = LOCTEXT("Stat_Mind", "Mind"); return true;
        case ESkillEffectType::BodyBuff: case ESkillEffectType::BodyDebuff:
            OutStat = LOCTEXT("Stat_Body", "Body"); return true;
        case ESkillEffectType::SpiritBuff: case ESkillEffectType::SpiritDebuff:
            OutStat = LOCTEXT("Stat_Spirit", "Spirit"); return true;
        case ESkillEffectType::SpellDamageBuff: case ESkillEffectType::SpellDamageDebuff:
            OutStat = LOCTEXT("Stat_SpellDamage", "Spell Damage"); return true;
        case ESkillEffectType::StatusMultiplierBuff: case ESkillEffectType::StatusMultiplierDebuff:
            OutStat = LOCTEXT("Stat_StatusMultiplier", "Status Multiplier"); return true;
        case ESkillEffectType::CritChanceBuff: case ESkillEffectType::CritChanceDebuff:
        case ESkillEffectType::CritDebuff: case ESkillEffectType::ModifyCritChance:
            OutStat = LOCTEXT("Stat_CritChance", "Crit Chance"); return true;
        case ESkillEffectType::ModifyCritDamage: case ESkillEffectType::CritDamageDebuff:
            OutStat = LOCTEXT("Stat_CritDamage", "Crit Damage"); return true;
        case ESkillEffectType::SpellSpeedBuff: case ESkillEffectType::SpellSpeedDebuff:
            OutStat = LOCTEXT("Stat_SpellSpeed", "Spell Speed"); return true;
        case ESkillEffectType::DefenseBuff: case ESkillEffectType::DefenseDebuff:
            OutStat = LOCTEXT("Stat_Defense", "Defense"); return true;
        case ESkillEffectType::ActionSpeedBuff: case ESkillEffectType::ActionSpeedDebuff:
            OutStat = LOCTEXT("Stat_ActionSpeed", "Action Speed"); return true;
        case ESkillEffectType::RawDamageBuff: case ESkillEffectType::RawDamageDebuff:
            OutStat = LOCTEXT("Stat_RawDamage", "Raw Damage"); return true;
        case ESkillEffectType::MaxEnergyBuff: case ESkillEffectType::MaxEnergyDebuff:
            OutStat = LOCTEXT("Stat_MaxEnergy", "Max Energy"); return true;
        case ESkillEffectType::MaxHPBuff: case ESkillEffectType::MaxHPDebuff:
            OutStat = LOCTEXT("Stat_MaxHP", "Max HP"); return true;
        case ESkillEffectType::ResistanceBuff: case ESkillEffectType::ResistanceDebuff:
            OutStat = LOCTEXT("Stat_Resistance", "Resistance"); return true;
        case ESkillEffectType::SpellSizeBuff: case ESkillEffectType::SpellSizeDebuff:
            OutStat = LOCTEXT("Stat_SpellSize", "Spell Size"); return true;
        case ESkillEffectType::TurnSpeedBuff: case ESkillEffectType::TurnSpeedDebuff: case ESkillEffectType::ModifyTurnSpeed:
            OutStat = LOCTEXT("Stat_TurnSpeed", "Turn Speed"); return true;
        case ESkillEffectType::LuckBuff: case ESkillEffectType::LuckDebuff:
            OutStat = LOCTEXT("Stat_Luck", "Luck"); return true;
        case ESkillEffectType::EfficiencyBuff: case ESkillEffectType::EfficiencyDebuff:
            OutStat = LOCTEXT("Stat_Efficiency", "Efficiency"); return true;
        case ESkillEffectType::ReflexBuff: case ESkillEffectType::ReflexDebuff:
            OutStat = LOCTEXT("Stat_Reflex", "Reflex"); return true;
        case ESkillEffectType::DamageBuff: case ESkillEffectType::DamageDebuff: case ESkillEffectType::ModifyDamageDealt:
            OutStat = LOCTEXT("Stat_Damage", "Damage"); return true;
        case ESkillEffectType::SpeedBuff: case ESkillEffectType::SpeedDebuff:
            OutStat = LOCTEXT("Stat_Speed", "Speed"); return true;
        case ESkillEffectType::EnergyDebuff:
            OutStat = LOCTEXT("Stat_Energy", "Energy"); return true;
        case ESkillEffectType::ModifyStatusResist:
            OutStat = LOCTEXT("Stat_StatusResistance", "Status Resistance"); return true;
        case ESkillEffectType::HealingReceivedBuff:
            OutStat = LOCTEXT("Stat_HealingReceived", "Healing Received"); return true;
        default:
            return false;
        }
    }

    /** Gain, lose, give (07, "Gain, lose, give"): "gain +20% Defense", "lose 1 Defense", "give an Enemy -20% Defense". */
    FText StatBody(const FSkillEffectPayload &P, const FSide &Side, const FText &Stat, bool bRaises)
    {
        const FText Named = FText::Format(LOCTEXT("StatWithElement", "{0}{1}"), ElementPrefix(P.Element), Stat);
        if (Side.bSelf)
        {
            return bRaises ? FText::Format(LOCTEXT("Stat_Gain", "gain +{0} {1}{2}"), Amount(P), Named, Span(P))
                           : FText::Format(LOCTEXT("Stat_Lose", "lose {0} {1}{2}"), Amount(P), Named, Span(P));
        }
        return bRaises ? FText::Format(LOCTEXT("Stat_GivePlus", "give {0} +{1} {2}{3}"), Side.Noun, Amount(P), Named, Span(P))
                       : FText::Format(LOCTEXT("Stat_GiveMinus", "give {0} -{1} {2}{3}"), Side.Noun, Amount(P), Named, Span(P));
    }

    /** "become immune to Fire" / "make an Ally immune to Stun". */
    FText ImmunityBody(const FSkillEffectPayload &P, const FSide &Side, const FText &What)
    {
        return Side.bSelf ? FText::Format(LOCTEXT("Immune_Self", "become immune to {0}{1}"), What, Span(P))
                          : FText::Format(LOCTEXT("Immune_Other", "make {0} immune to {1}{2}"), Side.Noun, What, Span(P));
    }

    /** The side fronted as the sentence's subject (07, "Fronted recipients"), its verb agreeing with the count. */
    FText Fronted(const FSide &Side, const FText &Singular, const FText &Plural, const FText &Rest)
    {
        return FText::Format(LOCTEXT("Fronted", "{0} {1}{2}"), Side.Noun, Side.bPlural ? Plural : Singular, Rest);
    }

    FText ReflectBody(const FSkillEffectPayload &P, const FSide &Side, const FText &DamageKind)
    {
        if (Side.bSelf)
        {
            return FText::Format(LOCTEXT("Reflect_Self", "reflect {0} of the {1}damage you take{2}"), Amount(P), DamageKind, Span(P));
        }
        return Fronted(Side, LOCTEXT("Reflect_Sg", "reflects"), LOCTEXT("Reflect_Pl", "reflect"),
                       FText::Format(LOCTEXT("Reflect_Rest", " {0} of the {1}damage they take{2}"), Amount(P), DamageKind, Span(P)));
    }

    FText DamageBody(const FSkillEffectPayload &P, const FSide &Side, const FText &Repeat)
    {
        const FText Element = ElementPrefix(P.Element);
        return Side.bSelf ? FText::Format(LOCTEXT("Damage_Self", "take {0} {1}damage{2}"), Amount(P), Element, Repeat)
                          : FText::Format(LOCTEXT("Damage_Other", "deal {0} {1}damage to {2}{3}"), Amount(P), Element, Side.Noun, Repeat);
    }

    /** "Capitalised where it opens a sentence" (07, "One row"). The pieces are already localised;
     *  this lifts the first letter with the culture's own upper-casing. */
    FText Capitalise(const FText &In)
    {
        const FString S = In.ToString();
        if (S.IsEmpty())
        {
            return In;
        }
        return FText::FromString(FText::FromString(S.Left(1)).ToUpper().ToString() + S.Mid(1));
    }

    FText JoinTexts(const TArray<FText> &Parts, const FText &Separator)
    {
        return FText::Join(Separator, Parts);
    }

    // ==================== TRIGGERS AND CONDITIONS ====================

    /** Your own event, named ("On Hit", "At the start of your turn"). */
    FText SelfEventPhrase(ESkillTrigger Trigger)
    {
        switch (Trigger)
        {
        case ESkillTrigger::OnCrit: return LOCTEXT("Trig_OnCrit", "On Critical Hit");
        case ESkillTrigger::OnHit: return LOCTEXT("Trig_OnHit", "On Hit");
        case ESkillTrigger::OnTakeDamage: return LOCTEXT("Trig_OnTakeDamage", "On Damage Received");
        case ESkillTrigger::OnBattleStart: return LOCTEXT("Trig_OnBattleStart", "At the start of the fight");
        case ESkillTrigger::OnTurnStart: return LOCTEXT("Trig_OnTurnStart", "At the start of your turn");
        case ESkillTrigger::OnTurnEnd: return LOCTEXT("Trig_OnTurnEnd", "At the end of your turn");
        case ESkillTrigger::OnKill: return LOCTEXT("Trig_OnKill", "When you defeat an Enemy");
        case ESkillTrigger::OnStatusApplied: return LOCTEXT("Trig_OnStatusApplied", "When you inflict a status");
        case ESkillTrigger::OnStatusReceived: return LOCTEXT("Trig_OnStatusReceived", "When you receive a status");
        case ESkillTrigger::OnSpellCast: return LOCTEXT("Trig_OnSpellCast", "When you cast a Refraction");
        case ESkillTrigger::OnAbilityUsed: return LOCTEXT("Trig_OnAbilityUsed", "When you use an Ability");
        case ESkillTrigger::OnDodge: return LOCTEXT("Trig_OnDodge", "On Dodge");
        case ESkillTrigger::OnPerfectDodge: return LOCTEXT("Trig_OnPerfectDodge", "On Perfect Dodge");
        case ESkillTrigger::OnBlock:
        case ESkillTrigger::OnGuard: return LOCTEXT("Trig_OnGuard", "On Guard");
        case ESkillTrigger::OnParry:
        case ESkillTrigger::OnPerfectParry:
        case ESkillTrigger::OnPerfectBlock:
        case ESkillTrigger::OnPerfectGuard: return LOCTEXT("Trig_OnPerfectGuard", "On Perfect Guard");
        default: return FText::GetEmpty();
        }
    }

    /** Another side's event, a sentence with its doer first (07: "When an Enemy hits"). */
    FText OtherEventVerb(ESkillTrigger Trigger)
    {
        switch (Trigger)
        {
        case ESkillTrigger::OnCrit: return LOCTEXT("TrigV_OnCrit", "lands a critical hit");
        case ESkillTrigger::OnHit: return LOCTEXT("TrigV_OnHit", "hits");
        case ESkillTrigger::OnTakeDamage: return LOCTEXT("TrigV_OnTakeDamage", "takes damage");
        case ESkillTrigger::OnTurnStart: return LOCTEXT("TrigV_OnTurnStart", "starts a turn");
        case ESkillTrigger::OnTurnEnd: return LOCTEXT("TrigV_OnTurnEnd", "ends a turn");
        case ESkillTrigger::OnKill: return LOCTEXT("TrigV_OnKill", "defeats an opponent");
        case ESkillTrigger::OnStatusApplied: return LOCTEXT("TrigV_OnStatusApplied", "inflicts a status");
        case ESkillTrigger::OnStatusReceived: return LOCTEXT("TrigV_OnStatusReceived", "receives a status");
        case ESkillTrigger::OnSpellCast: return LOCTEXT("TrigV_OnSpellCast", "casts a Refraction");
        case ESkillTrigger::OnAbilityUsed: return LOCTEXT("TrigV_OnAbilityUsed", "uses an Ability");
        case ESkillTrigger::OnDodge: return LOCTEXT("TrigV_OnDodge", "dodges");
        case ESkillTrigger::OnPerfectDodge: return LOCTEXT("TrigV_OnPerfectDodge", "lands a Perfect Dodge");
        case ESkillTrigger::OnBlock:
        case ESkillTrigger::OnGuard: return LOCTEXT("TrigV_OnGuard", "guards");
        case ESkillTrigger::OnParry:
        case ESkillTrigger::OnPerfectParry:
        case ESkillTrigger::OnPerfectBlock:
        case ESkillTrigger::OnPerfectGuard: return LOCTEXT("TrigV_OnPerfectGuard", "lands a Perfect Guard");
        default: return FText::GetEmpty();
        }
    }

    FText SubjectNoun(ECondSubject Subject)
    {
        switch (Subject)
        {
        case ECondSubject::SelfTeam: return LOCTEXT("Who_SelfTeam", "a Team Member");
        case ECondSubject::Target: return LOCTEXT("Who_Target", "the attacked Enemy");
        case ECondSubject::TargetTeam: return LOCTEXT("Who_TargetTeam", "an Enemy");
        default: return LOCTEXT("Who_Self", "you");
        }
    }

    FText SubjectPossessive(ECondSubject Subject)
    {
        switch (Subject)
        {
        case ECondSubject::SelfTeam: return LOCTEXT("Whose_SelfTeam", "a Team Member's");
        case ECondSubject::Target: return LOCTEXT("Whose_Target", "the attacked Enemy's");
        case ECondSubject::TargetTeam: return LOCTEXT("Whose_TargetTeam", "an Enemy's");
        default: return LOCTEXT("Whose_Self", "your");
        }
    }

    FText EventPhrase(const FSkillCondition &C)
    {
        // The fight's start belongs to no one.
        if (C.Subject == ECondSubject::Self || C.Trigger == ESkillTrigger::OnBattleStart)
        {
            return SelfEventPhrase(C.Trigger);
        }
        return FText::Format(LOCTEXT("OtherEvent", "When {0} {1}"), SubjectNoun(C.Subject), OtherEventVerb(C.Trigger));
    }

    /** A state check without its "While" ("your HP is below 30%"). */
    FText StateClause(const FSkillCondition &C)
    {
        const FText Threshold = FText::Format(LOCTEXT("ThresholdPercent", "{0}%"), FText::AsNumber(FMath::RoundToInt(C.Threshold)));
        const FText Whose = SubjectPossessive(C.Subject);
        switch (C.Trigger)
        {
        case ESkillTrigger::OnHPBelowThreshold: return FText::Format(LOCTEXT("State_HPBelow", "{0} HP is below {1}"), Whose, Threshold);
        case ESkillTrigger::OnHPAboveThreshold: return FText::Format(LOCTEXT("State_HPAbove", "{0} HP is above {1}"), Whose, Threshold);
        case ESkillTrigger::OnEnergyBelowThreshold: return FText::Format(LOCTEXT("State_EnergyBelow", "{0} Energy is below {1}"), Whose, Threshold);
        case ESkillTrigger::OnEnergyAboveThreshold: return FText::Format(LOCTEXT("State_EnergyAbove", "{0} Energy is above {1}"), Whose, Threshold);
        default: return FText::GetEmpty();
        }
    }

    bool IsStateTrigger(ESkillTrigger Trigger)
    {
        return SkillTriggerUtils::IsThresholdTrigger(Trigger);
    }

    /** EvaluateConditionGroup puts every OR entry, event or state, in one any-of set. When that
     *  set mixes both, it reads as one either-or heading ("On Hit or while your HP is below 30%"). */
    bool HasMixedAnyOf(const FSkillEffect &Effect)
    {
        bool bOrEvent = false;
        bool bOrState = false;
        for (const FSkillCondition &C : Effect.Conditions)
        {
            if (C.Combine == ECondCombine::Or)
            {
                bOrEvent = bOrEvent || EffectText::IsEventTrigger(C.Trigger);
                bOrState = bOrState || IsStateTrigger(C.Trigger);
            }
        }
        return bOrEvent && bOrState;
    }

    bool HasHeadingTrigger(const FSkillEffect &Effect)
    {
        return Effect.Conditions.ContainsByPredicate([](const FSkillCondition &C) { return EffectText::IsEventTrigger(C.Trigger); });
    }

    // ==================== THE TREE ====================

    struct FLevel
    {
        FText Text;
        TArray<int32> ConditionIndices;
        bool bAnyOf = false;
    };

    /** A condition level per AND state check (each must hold), one level for all the OR state
     *  checks (any may hold): the same rule as EvaluateConditionGroup. A mixed any-of set is
     *  the heading's instead. */
    TArray<FLevel> ConditionLevels(const FSkillEffect &Effect)
    {
        TArray<FLevel> Levels;
        FLevel AnyOf;
        AnyOf.bAnyOf = true;
        TArray<FText> AnyOfClauses;
        const bool bAnyOfInHeading = HasMixedAnyOf(Effect);
        for (int32 i = 0; i < Effect.Conditions.Num(); ++i)
        {
            const FSkillCondition &C = Effect.Conditions[i];
            if (!IsStateTrigger(C.Trigger))
            {
                continue;
            }
            if (C.Combine == ECondCombine::Or)
            {
                if (bAnyOfInHeading)
                {
                    continue;
                }
                AnyOf.ConditionIndices.Add(i);
                AnyOfClauses.Add(StateClause(C));
                continue;
            }
            FLevel Level;
            Level.Text = FText::Format(LOCTEXT("While", "While {0}"), StateClause(C));
            Level.ConditionIndices.Add(i);
            Levels.Add(Level);
        }
        if (AnyOfClauses.Num() > 0)
        {
            AnyOf.Text = FText::Format(LOCTEXT("While", "While {0}"), JoinTexts(AnyOfClauses, LOCTEXT("OrJoin", " or ")));
            Levels.Add(AnyOf);
        }
        return Levels;
    }

    struct FNode
    {
        FEffectTextLine Line;
        TArray<FNode> Children;
        int32 BlockOrder = 0;
    };

    FNode &FindOrAdd(TArray<FNode> &Nodes, const FEffectTextLine &Line)
    {
        for (FNode &Node : Nodes)
        {
            if (Node.Line.Kind == Line.Kind && Node.Line.Text.EqualTo(Line.Text))
            {
                return Node;
            }
        }
        FNode &Added = Nodes.AddDefaulted_GetRef();
        Added.Line = Line;
        return Added;
    }

    void Flatten(const TArray<FNode> &Nodes, TArray<FEffectTextLine> &Out)
    {
        for (const FNode &Node : Nodes)
        {
            Out.Add(Node.Line);
            Flatten(Node.Children, Out);
        }
    }

    void CollectLeaves(const FNode &Node, TArray<FText> &OutLeaves, bool &bOutConditional)
    {
        for (const FNode &Child : Node.Children)
        {
            if (Child.Line.Kind == EEffectTextLineKind::Condition)
            {
                bOutConditional = true;
            }
            if (Child.Line.Kind == EEffectTextLineKind::Effect)
            {
                OutLeaves.Add(Child.Line.Text);
            }
            CollectLeaves(Child, OutLeaves, bOutConditional);
        }
    }

    /** The effect-level riders a leaf carries: stacking, once per fight, charges. */
    FText Riders(const FSkillEffect &Effect, const FSkillEffectPayload &P)
    {
        FText Out = FText::GetEmpty();
        if (P.Charges > 0)
        {
            Out = FText::Format(LOCTEXT("Rider_Charges", "{0} ({1} {1}|plural(one=use,other=uses))"), Out, P.Charges);
        }
        if (Effect.bStackable)
        {
            Out = FText::Format(LOCTEXT("Rider_Stacks", "{0}, stacks up to {1} {1}|plural(one=time,other=times)"), Out, Effect.MaxStacks);
        }
        if (Effect.bFiresOncePerMatch)
        {
            Out = FText::Format(LOCTEXT("Rider_Once", "{0}, once per fight"), Out);
        }
        return Out;
    }

    TArray<FNode> BuildNodes(const TArray<FSkillEffect> &Effects, EEffectTextSource Source)
    {
        TArray<FNode> Headings;
        for (int32 e = 0; e < Effects.Num(); ++e)
        {
            const FSkillEffect &Effect = Effects[e];
            const bool bDebuff = Effect.IsDebuff() && !Effect.IsBuff();

            FEffectTextLine Heading;
            Heading.Kind = EEffectTextLineKind::Heading;
            Heading.Text = EffectText::DescribeHeading(Effect, Source);
            Heading.EffectIndex = e;
            Heading.bDebuff = bDebuff;

            const TArray<FLevel> Levels = ConditionLevels(Effect);
            for (const FSkillEffectPayload &P : Effect.Payloads)
            {
                if (P.EffectType == ESkillEffectType::None)
                {
                    continue;
                }
                FNode &HeadingNode = FindOrAdd(Headings, Heading);
                HeadingNode.BlockOrder = HasHeadingTrigger(Effect) ? TRIGGERED_BLOCK
                                         : Source == EEffectTextSource::Skill ? ACTIVATE_BLOCK : PASSIVE_BLOCK;
                TArray<FNode> *Siblings = &HeadingNode.Children;
                for (int32 d = 0; d < Levels.Num(); ++d)
                {
                    FEffectTextLine Condition;
                    Condition.Kind = EEffectTextLineKind::Condition;
                    Condition.Depth = d + 1;
                    Condition.Text = Levels[d].Text;
                    Condition.EffectIndex = e;
                    Condition.ConditionIndices = Levels[d].ConditionIndices;
                    Condition.bAnyOf = Levels[d].bAnyOf;
                    Condition.bDebuff = bDebuff;
                    Siblings = &FindOrAdd(*Siblings, Condition).Children;
                }
                FNode &Leaf = Siblings->AddDefaulted_GetRef();
                Leaf.Line.Kind = EEffectTextLineKind::Effect;
                Leaf.Line.Depth = Levels.Num() + 1;
                Leaf.Line.Text = FText::Format(LOCTEXT("LeafWithRiders", "{0}{1}"),
                                               Capitalise(EffectText::DescribePayload(P)), Riders(Effect, P));
                Leaf.Line.EffectIndex = e;
                // ModifyEnergyCost is classed a debuff whatever its sign; the text lights by what it does.
                Leaf.Line.bDebuff = P.EffectType == ESkillEffectType::ModifyEnergyCost
                                        ? !IsCostCut(P)
                                        : SkillEffectClassification::IsDebuff(P.EffectType, P.Magnitude);
            }
        }
        // Activate first, Passive last (07, "Block structure" and "The card-text tree").
        Headings.StableSort([](const FNode &A, const FNode &B) { return A.BlockOrder < B.BlockOrder; });
        return Headings;
    }
}

namespace EffectText
{
    bool IsEventTrigger(ESkillTrigger Trigger)
    {
        return Trigger != ESkillTrigger::None && Trigger != ESkillTrigger::Always && !IsStateTrigger(Trigger);
    }

    /** Built from EvaluateConditionGroup's two sets: every AND event, then the any-of set
     *  ("On Crit and either On Hit or On Dodge"). A lone OR entry must hold, so it reads as AND. */
    FText DescribeHeading(const FSkillEffect &Effect, EEffectTextSource Source)
    {
        const bool bMixed = HasMixedAnyOf(Effect);
        TArray<FText> AllOf;
        TArray<FText> AnyOfEvents;
        TArray<FText> AnyOfStates;
        for (const FSkillCondition &C : Effect.Conditions)
        {
            const bool bEvent = IsEventTrigger(C.Trigger);
            if (C.Combine == ECondCombine::And)
            {
                if (bEvent)
                {
                    AllOf.Add(EventPhrase(C));
                }
            }
            else if (bEvent)
            {
                AnyOfEvents.Add(EventPhrase(C));
            }
            else if (bMixed && IsStateTrigger(C.Trigger))
            {
                AnyOfStates.Add(StateClause(C));
            }
        }

        const int32 AnyOfCount = AnyOfEvents.Num() + AnyOfStates.Num();
        if (AnyOfCount == 1)
        {
            AllOf.Append(AnyOfEvents);
        }
        else if (AnyOfCount > 1)
        {
            FText AnyOf = JoinTexts(AnyOfEvents, LOCTEXT("OrJoin", " or "));
            if (AnyOfStates.Num() > 0)
            {
                AnyOf = FText::Format(LOCTEXT("HeadingOrWhile", "{0} or while {1}"), AnyOf, JoinTexts(AnyOfStates, LOCTEXT("OrJoin", " or ")));
            }
            AllOf.Add(AllOf.Num() > 0 ? FText::Format(LOCTEXT("HeadingEither", "either {0}"), AnyOf) : AnyOf);
        }
        if (AllOf.Num() > 0)
        {
            return JoinTexts(AllOf, LOCTEXT("AndJoin", " and "));
        }
        return Source == EEffectTextSource::Skill ? LOCTEXT("Heading_Activate", "Activate")
                                                  : LOCTEXT("Heading_Passive", "Passive");
    }

    FText DescribePayload(const FSkillEffectPayload &P)
    {
        const FSide Side = GetSide(P.Target, P.TargetCount);
        FText Stat;
        bool bRaises = false;
        if (GetStatChange(P.EffectType, P.Magnitude, Stat, bRaises))
        {
            return StatBody(P, Side, Stat, bRaises);
        }

        const bool bDrain = P.DrainPercent > 0.0f;
        const FText DrainShare = Percent(P.DrainPercent);
        switch (P.EffectType)
        {
        // ---- damage ----
        case ESkillEffectType::DOT:
            return DamageBody(P, Side, EachTurnSpan(P));
        case ESkillEffectType::BurstDamage:
        case ESkillEffectType::RetaliationDamage:
            return DamageBody(P, Side, FText::GetEmpty());
        case ESkillEffectType::SelfDamage:
            return FText::Format(LOCTEXT("SelfDamage", "take {0} {1}damage"), Amount(P), ElementPrefix(P.Element));

        // ---- healing and energy ----
        case ESkillEffectType::Heal:
        case ESkillEffectType::HealthRestore:
            if (bDrain)
            {
                return Side.bSelf ? FText::Format(LOCTEXT("HealDrain_Self", "heal for {0} of the damage dealt"), DrainShare)
                                  : FText::Format(LOCTEXT("HealDrain_Other", "heal {0} for {1} of the damage dealt"), Side.Noun, DrainShare);
            }
            return Side.bSelf ? FText::Format(LOCTEXT("Heal_Self", "heal {0}{1}"), Amount(P), EachTurnSpan(P))
                              : FText::Format(LOCTEXT("Heal_Other", "heal {0} for {1}{2}"), Side.Noun, Amount(P), EachTurnSpan(P));
        case ESkillEffectType::RestoreHPPercent:
            return Side.bSelf ? FText::Format(LOCTEXT("HealPct_Self", "heal {0} of your max HP{1}"), Amount(P), EachTurnSpan(P))
                              : FText::Format(LOCTEXT("HealPct_Other", "heal {0} for {1} of their max HP{2}"), Side.Noun, Amount(P), EachTurnSpan(P));
        case ESkillEffectType::EnergyRestore:
            if (bDrain)
            {
                return Side.bSelf ? FText::Format(LOCTEXT("EnergyDrain_Self", "gain Energy equal to {0} of the damage dealt"), DrainShare)
                                  : FText::Format(LOCTEXT("EnergyDrain_Other", "give {0} Energy equal to {1} of the damage dealt"), Side.Noun, DrainShare);
            }
            return Side.bSelf ? FText::Format(LOCTEXT("Energy_Self", "gain {0} Energy{1}"), Amount(P), EachTurnSpan(P))
                              : FText::Format(LOCTEXT("Energy_Other", "give {0} {1} Energy{2}"), Side.Noun, Amount(P), EachTurnSpan(P));
        case ESkillEffectType::RestoreEnergyPercent:
            return Side.bSelf ? FText::Format(LOCTEXT("EnergyPct_Self", "gain {0} of your max Energy{1}"), Amount(P), EachTurnSpan(P))
                              : FText::Format(LOCTEXT("EnergyPct_Other", "give {0} {1} of their max Energy{2}"), Side.Noun, Amount(P), EachTurnSpan(P));
        case ESkillEffectType::EnergyDrain:
        case ESkillEffectType::DrainEnergy:
            return Side.bSelf ? FText::Format(LOCTEXT("DrainEnergy_Self", "lose {0} Energy{1}"), Amount(P), EachTurnSpan(P))
                              : FText::Format(LOCTEXT("DrainEnergy_Other", "drain {0} Energy from {1}{2}"), Amount(P), Side.Noun, EachTurnSpan(P));
        case ESkillEffectType::DrainHP:
            return Side.bSelf ? FText::Format(LOCTEXT("DrainHP_Self", "lose {0} HP{1}"), Amount(P), EachTurnSpan(P))
                              : FText::Format(LOCTEXT("DrainHP_Other", "drain {0} HP from {1}{2}"), Amount(P), Side.Noun, EachTurnSpan(P));
        case ESkillEffectType::Lifesteal:
            return Side.bSelf ? FText::Format(LOCTEXT("Lifesteal_Self", "heal for {0} of the damage you deal{1}"), Amount(P), Span(P))
                              : Fronted(Side, LOCTEXT("Lifesteal_Sg", "heals"), LOCTEXT("Lifesteal_Pl", "heal"),
                                        FText::Format(LOCTEXT("Lifesteal_Rest", " for {0} of the damage they deal{1}"), Amount(P), Span(P)));

        // ---- statuses ----
        case ESkillEffectType::Stun:
        case ESkillEffectType::ApplyStunToTarget:
            return Side.bSelf ? FText::Format(LOCTEXT("Stun_Self", "become stunned{0}"), Span(P))
                              : FText::Format(LOCTEXT("Stun_Other", "stun {0}{1}"), Side.Noun, Span(P));
        case ESkillEffectType::SkipTurn:
            // Wind's break pushes the next turn back (D-014); nothing skips a turn outright.
            return Side.bSelf ? LOCTEXT("Delay_Self", "delay your next turn")
                              : FText::Format(Side.bPlural ? LOCTEXT("Delay_Pl", "delay {0} next turns") : LOCTEXT("Delay_Sg", "delay {0} next turn"),
                                              Side.Possessive);
        case ESkillEffectType::RandomDebuff:
            return Side.bSelf ? FText::Format(LOCTEXT("RandomDebuff_Self", "lose {0} of a random stat{1}"), Amount(P), Span(P))
                              : FText::Format(LOCTEXT("RandomDebuff_Other", "give {0} -{1} to a random stat{2}"), Side.Noun, Amount(P), Span(P));
        case ESkillEffectType::Silenced:
            return Side.bSelf ? FText::Format(LOCTEXT("Silence_Self", "become silenced{0}"), Span(P))
                              : FText::Format(LOCTEXT("Silence_Other", "silence {0}{1}"), Side.Noun, Span(P));
        case ESkillEffectType::HealBlock:
            return Side.bSelf ? FText::Format(LOCTEXT("HealBlock_Self", "you can't be healed{0}"), Span(P))
                              : FText::Format(LOCTEXT("HealBlock_Other", "{0} can't be healed{1}"), Side.Noun, Span(P));
        case ESkillEffectType::RandomSkill:
            return Side.bSelf ? FText::Format(LOCTEXT("Random_Self", "act at random{0}"), Span(P))
                              : Fronted(Side, LOCTEXT("Random_Sg", "acts"), LOCTEXT("Random_Pl", "act"),
                                        FText::Format(LOCTEXT("Random_Rest", " at random{0}"), Span(P)));
        case ESkillEffectType::ApplyBurnToTarget:
            return FText::Format(LOCTEXT("Burn", "inflict Burn on {0}{1}"), Side.bSelf ? LOCTEXT("Yourself", "yourself") : Side.Noun, Span(P));
        case ESkillEffectType::ApplyChillToTarget:
            return FText::Format(LOCTEXT("Chill", "inflict Chill on {0}{1}"), Side.bSelf ? LOCTEXT("Yourself", "yourself") : Side.Noun, Span(P));

        // ---- immunities ----
        case ESkillEffectType::GrantDOTImmunity: return ImmunityBody(P, Side, LOCTEXT("Immune_DOT", "damage over time"));
        case ESkillEffectType::GrantStunImmunity: return ImmunityBody(P, Side, LOCTEXT("Immune_Stun", "Stun"));
        case ESkillEffectType::GrantSilenceImmunity: return ImmunityBody(P, Side, LOCTEXT("Immune_Silence", "Silence"));
        case ESkillEffectType::GrantAllStatusImmunity: return ImmunityBody(P, Side, LOCTEXT("Immune_All", "every status"));
        case ESkillEffectType::GrantFireImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Fire));
        case ESkillEffectType::GrantWaterImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Water));
        case ESkillEffectType::GrantEarthImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Earth));
        case ESkillEffectType::GrantWindImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Wind));
        case ESkillEffectType::GrantLightImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Light));
        case ESkillEffectType::GrantDarknessImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Darkness));
        case ESkillEffectType::GrantLightningImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Lightning));
        case ESkillEffectType::GrantVoidImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Void));
        case ESkillEffectType::GrantRealityImmunity: return ImmunityBody(P, Side, WoRGameNames::GetElementName(ESpellElement::Reality));

        // ---- cleansing ----
        case ESkillEffectType::CleanseSelf:
            return LOCTEXT("Cleanse_Self", "remove all debuffs");
        case ESkillEffectType::CleanseAllies:
            return LOCTEXT("Cleanse_Allies", "remove all debuffs from all Allies");
        case ESkillEffectType::Cleanse:
            return Side.bSelf ? LOCTEXT("Cleanse_Self", "remove all debuffs")
                              : FText::Format(LOCTEXT("Cleanse_Other", "remove all debuffs from {0}"), Side.Noun);
        case ESkillEffectType::RemoveSpeedDebuff:
        case ESkillEffectType::RemoveDamageDebuff:
        case ESkillEffectType::RemoveDefenseDebuff:
        {
            const FText Which = P.EffectType == ESkillEffectType::RemoveSpeedDebuff ? LOCTEXT("Stat_Speed", "Speed")
                                : P.EffectType == ESkillEffectType::RemoveDamageDebuff ? LOCTEXT("Stat_Damage", "Damage")
                                                                                        : LOCTEXT("Stat_Defense", "Defense");
            return Side.bSelf ? FText::Format(LOCTEXT("RemoveDebuff_Self", "remove {0} debuffs"), Which)
                              : FText::Format(LOCTEXT("RemoveDebuff_Other", "remove {0} debuffs from {1}"), Which, Side.Noun);
        }

        // ---- defence ----
        case ESkillEffectType::DamageReflect: return ReflectBody(P, Side, FText::GetEmpty());
        case ESkillEffectType::ReflectPhysicalDamage: return ReflectBody(P, Side, LOCTEXT("Kind_Physical", "Physical "));
        case ESkillEffectType::ReflectSpellDamage: return ReflectBody(P, Side, LOCTEXT("Kind_Refraction", "Refraction "));
        case ESkillEffectType::AbsorbDamage:
            return Side.bSelf ? FText::Format(LOCTEXT("Absorb_Self", "negate the next {0} damage you take{1}"), Amount(P), Span(P))
                              : FText::Format(LOCTEXT("Absorb_Other", "negate the next {0} damage {1} {2}{3}"), Amount(P), Side.Noun,
                                              Side.bPlural ? LOCTEXT("Take_Pl", "take") : LOCTEXT("Take_Sg", "takes"), Span(P));
        case ESkillEffectType::ReduceDamageTaken:
            return Side.bSelf ? FText::Format(LOCTEXT("LessDamage_Self", "take {0} less damage{1}"), Amount(P), Span(P))
                              : Fronted(Side, LOCTEXT("Take_Sg", "takes"), LOCTEXT("Take_Pl", "take"),
                                        FText::Format(LOCTEXT("LessDamage_Rest", " {0} less damage{1}"), Amount(P), Span(P)));
        case ESkillEffectType::IncreaseDamageTaken:
            return Side.bSelf ? FText::Format(LOCTEXT("MoreDamage_Self", "take {0} more damage{1}"), Amount(P), Span(P))
                              : Fronted(Side, LOCTEXT("Take_Sg", "takes"), LOCTEXT("Take_Pl", "take"),
                                        FText::Format(LOCTEXT("MoreDamage_Rest", " {0} more damage{1}"), Amount(P), Span(P)));
        case ESkillEffectType::LastStand:
            return Side.bSelf ? FText::Format(LOCTEXT("LastStand_Self", "survive the next lethal hit{0}"), Span(P))
                              : Fronted(Side, LOCTEXT("Survive_Sg", "survives"), LOCTEXT("Survive_Pl", "survive"),
                                        FText::Format(LOCTEXT("LastStand_Rest", " the next lethal hit{0}"), Span(P)));

        // ---- attacks ----
        case ESkillEffectType::ExtraAction:
            // The runtime grants one turn per apply (TurnManager::RequestExtraTurn).
            return Side.bSelf ? LOCTEXT("Extra_Self", "take an extra turn")
                              : FText::Format(LOCTEXT("Extra_Other", "give {0} an extra turn"), Side.Noun);
        case ESkillEffectType::GuaranteedCrit:
            return FText::Format(LOCTEXT("Crit", "{0} next attack is a critical hit"),
                                 Side.bSelf ? LOCTEXT("Whose_Self", "your") : Side.Possessive);
        case ESkillEffectType::DoubleHit:
            return FText::Format(LOCTEXT("DoubleHit", "{0} next attack hits twice"),
                                 Side.bSelf ? LOCTEXT("Whose_Self", "your") : Side.Possessive);
        case ESkillEffectType::IgnoreDefense:
            return FText::Format(LOCTEXT("IgnoreDefense", "{0} attacks ignore {1} Defense{2}"),
                                 Side.bSelf ? LOCTEXT("Whose_Self", "your") : Side.Possessive, IgnoreDefenseAmount(P), Span(P));
        case ESkillEffectType::SpellCostBuff:
        case ESkillEffectType::SpellCostDebuff:
        case ESkillEffectType::ModifyEnergyCost:
        {
            // The cost multiplier applies to Refractions and Abilities alike.
            const FText Whose = Side.bSelf ? LOCTEXT("Whose_Self", "your") : Side.Possessive;
            return IsCostCut(P)
                       ? FText::Format(LOCTEXT("Cost_Less", "{0} Refractions and Abilities cost {1} less{2}"), Whose, Amount(P), Span(P))
                       : FText::Format(LOCTEXT("Cost_More", "{0} Refractions and Abilities cost {1} more{2}"), Whose, Amount(P), Span(P));
        }

        // ---- status bars and rows ----
        case ESkillEffectType::StatusIncrease:
            return Side.bSelf ? FText::Format(LOCTEXT("Buildup_Self", "gain {0} {1}buildup"), Amount(P), ElementPrefix(P.Element))
                              : FText::Format(LOCTEXT("Buildup_Other", "add {0} {1}buildup to {2}"), Amount(P), ElementPrefix(P.Element), Side.Noun);
        case ESkillEffectType::StatusDecrease:
            return Side.bSelf ? FText::Format(LOCTEXT("Unbuild_Self", "remove {0} {1}buildup"), Amount(P), ElementPrefix(P.Element))
                              : FText::Format(LOCTEXT("Unbuild_Other", "remove {0} {1}buildup from {2}"), Amount(P), ElementPrefix(P.Element), Side.Noun);
        case ESkillEffectType::PushBack:
            return Side.bSelf ? LOCTEXT("Push_Self", "move back one row")
                              : FText::Format(LOCTEXT("Push_Other", "push {0} back one row"), Side.Noun);
        case ESkillEffectType::PullForward:
            return Side.bSelf ? LOCTEXT("Pull_Self", "move forward one row")
                              : FText::Format(LOCTEXT("Pull_Other", "pull {0} forward one row"), Side.Noun);

        default:
            // A kind with no sentence yet prints its name, the loud failure (07: "a gap to close").
            return FText::Format(LOCTEXT("Unworded", "{0}{1}"), WoRGameNames::GetSkillEffectTypeName(P.EffectType), Span(P));
        }
    }

    TArray<FEffectTextLine> BuildTree(const TArray<FSkillEffect> &Effects, EEffectTextSource Source)
    {
        TArray<FEffectTextLine> Lines;
        Flatten(BuildNodes(Effects, Source), Lines);
        return Lines;
    }

    TArray<FText> BuildShortForm(const TArray<FSkillEffect> &Effects, EEffectTextSource Source)
    {
        TArray<FText> Out;
        for (const FNode &Heading : BuildNodes(Effects, Source))
        {
            TArray<FText> Leaves;
            bool bConditional = false;
            CollectLeaves(Heading, Leaves, bConditional);
            // Leaves are built capitalised; after the first they continue the sentence in lower case.
            for (int32 i = 1; i < Leaves.Num(); ++i)
            {
                const FString S = Leaves[i].ToString();
                Leaves[i] = FText::FromString(FText::FromString(S.Left(1)).ToLower().ToString() + S.Mid(1));
            }
            Out.Add(FText::Format(LOCTEXT("ShortLine", "{0}{1}: {2}"), Heading.Line.Text,
                                  bConditional ? LOCTEXT("ShortStar", "*") : FText::GetEmpty(),
                                  JoinTexts(Leaves, LOCTEXT("ShortJoin", ", "))));
        }
        return Out;
    }

    FText ToMultilineText(const TArray<FEffectTextLine> &Lines)
    {
        TArray<FText> Rows;
        for (const FEffectTextLine &Line : Lines)
        {
            const FText Indent = FText::AsCultureInvariant(FString::ChrN(Line.Depth * INDENT_SPACES_PER_LEVEL, TEXT(' ')));
            Rows.Add(Line.Kind == EEffectTextLineKind::Effect
                         ? FText::Format(LOCTEXT("TreeLeaf", "{0}• {1}"), Indent, Line.Text)
                         : FText::Format(LOCTEXT("TreeNode", "{0}{1}"), Indent, Line.Text));
        }
        return FText::Join(FText::AsCultureInvariant(TEXT("\n")), Rows);
    }

    void LightTree(TArray<FEffectTextLine> &Lines, const TArray<FSkillEffect> &Effects,
                   TFunctionRef<bool(const FSkillCondition &)> IsHeld)
    {
        for (FEffectTextLine &Line : Lines)
        {
            if (Line.Kind != EEffectTextLineKind::Condition || !Effects.IsValidIndex(Line.EffectIndex))
            {
                continue;
            }
            const TArray<FSkillCondition> &Conditions = Effects[Line.EffectIndex].Conditions;
            bool bAny = false;
            bool bAll = true;
            for (const int32 Index : Line.ConditionIndices)
            {
                const bool bHeld = Conditions.IsValidIndex(Index) && IsHeld(Conditions[Index]);
                bAny = bAny || bHeld;
                bAll = bAll && bHeld;
            }
            Line.Light = (Line.bAnyOf ? bAny : bAll) ? EEffectTextLight::Held : EEffectTextLight::NotHeld;
        }
    }
}

#undef LOCTEXT_NAMESPACE
