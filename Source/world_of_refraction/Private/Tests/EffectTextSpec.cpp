// EffectTextSpec.cpp
// B-045 (D-031): item and skill text generated from the effect data as the deckbuilder's
// card-text tree. Bodies follow wor-deckbuilder 07-text-grammar.md ("Verb bodies"); the tree's
// grouping follows its "The card-text tree" (rows with the same words share a node, wherever
// they sit). Same data gives the same text; condition lines report held / not held.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "WoRGameNames.h"
#include "Skills/Effects/EffectText.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr float LOW_HP_THRESHOLD = 30.0f;
	constexpr float LOW_ENERGY_THRESHOLD = 20.0f;

	FSkillCondition Cond(ESkillTrigger Trigger, ECondCombine Combine = ECondCombine::And,
						 ECondSubject Subject = ECondSubject::Self, float Threshold = LOW_HP_THRESHOLD)
	{
		FSkillCondition C;
		C.Trigger = Trigger;
		C.Combine = Combine;
		C.Subject = Subject;
		C.Threshold = Threshold;
		return C;
	}

	FSkillEffectPayload Pay(ESkillEffectType Type, ETargetType Target, int32 Value, float Magnitude = 0.0f, int32 Duration = 0,
							ESpellElement Element = ESpellElement::None, ETargetCount Count = ETargetCount::Single)
	{
		FSkillEffectPayload P;
		P.EffectType = Type;
		P.Target = Target;
		P.TargetCount = Count;
		P.Value = Value;
		P.Magnitude = Magnitude;
		P.Duration = Duration;
		P.Element = Element;
		return P;
	}

	FSkillEffect Eff(const TArray<FSkillCondition> &Conditions, const TArray<FSkillEffectPayload> &Payloads)
	{
		FSkillEffect E;
		E.Conditions = Conditions;
		E.Payloads = Payloads;
		return E;
	}

	FString Body(const FSkillEffectPayload &P)
	{
		return EffectText::DescribePayload(P).ToString();
	}

	TArray<FString> Texts(const TArray<FEffectTextLine> &Lines)
	{
		TArray<FString> Out;
		for (const FEffectTextLine &Line : Lines)
		{
			Out.Add(Line.Text.ToString());
		}
		return Out;
	}

	const FEffectTextLine *FindLine(const TArray<FEffectTextLine> &Lines, const FString &Text)
	{
		return Lines.FindByPredicate([&](const FEffectTextLine &L) { return L.Text.ToString() == Text; });
	}
}

BEGIN_DEFINE_SPEC(FEffectTextSpec, "WoR.Text.Effects",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	void ExpectLines(const TArray<FEffectTextLine> &Lines, const TArray<FString> &Expected);
END_DEFINE_SPEC(FEffectTextSpec)

void FEffectTextSpec::ExpectLines(const TArray<FEffectTextLine> &Lines, const TArray<FString> &Expected)
{
	const TArray<FString> Got = Texts(Lines);
	if (!TestEqual(TEXT("Line count"), Got.Num(), Expected.Num()))
	{
		AddInfo(FString::Join(Got, TEXT(" | ")));
		return;
	}
	for (int32 i = 0; i < Expected.Num(); ++i)
	{
		TestEqual(FString::Printf(TEXT("Line %d"), i), Got[i], Expected[i]);
	}
}

void FEffectTextSpec::Define()
{
	Describe("the tree", [this]()
	{
		It("puts a trigger, its condition and every effect under it on their own levels", [this]()
		{
			const TArray<FSkillEffect> Effects = {Eff(
				{Cond(ESkillTrigger::OnTakeDamage), Cond(ESkillTrigger::OnHPBelowThreshold)},
				{Pay(ESkillEffectType::Heal, ETargetType::Self, 2),
				 Pay(ESkillEffectType::DefenseBuff, ETargetType::Self, 0, 0.1f),
				 Pay(ESkillEffectType::EnergyRestore, ETargetType::Self, 1)})};
			const TArray<FEffectTextLine> Lines = EffectText::BuildTree(Effects, EEffectTextSource::Gear);
			ExpectLines(Lines, {TEXT("On Damage Received"), TEXT("While your HP is below 30%"),
								TEXT("Heal 2"), TEXT("Gain +10% Defense"), TEXT("Gain 1 Energy")});
			if (Lines.Num() == 5)
			{
				TestEqual(TEXT("Heading depth"), Lines[0].Depth, 0);
				TestEqual(TEXT("Condition depth"), Lines[1].Depth, 1);
				TestEqual(TEXT("Leaf depth"), Lines[2].Depth, 2);
				TestTrue(TEXT("Kinds"), Lines[0].Kind == EEffectTextLineKind::Heading
										&& Lines[1].Kind == EEffectTextLineKind::Condition
										&& Lines[4].Kind == EEffectTextLineKind::Effect);
			}
		});

		It("gives two conditions under one trigger two branches", [this]()
		{
			const TArray<FSkillEffect> Effects = {
				Eff({Cond(ESkillTrigger::OnTakeDamage), Cond(ESkillTrigger::OnHPBelowThreshold)},
					{Pay(ESkillEffectType::Heal, ETargetType::Self, 2)}),
				Eff({Cond(ESkillTrigger::OnTakeDamage), Cond(ESkillTrigger::OnHPBelowThreshold, ECondCombine::And, ECondSubject::TargetTeam)},
					{Pay(ESkillEffectType::EnergyRestore, ETargetType::Self, 1)})};
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Gear),
						{TEXT("On Damage Received"), TEXT("While your HP is below 30%"), TEXT("Heal 2"),
						 TEXT("While an Enemy's HP is below 30%"), TEXT("Gain 1 Energy")});
		});

		It("gives a different trigger its own heading", [this]()
		{
			const TArray<FSkillEffect> Effects = {
				Eff({Cond(ESkillTrigger::OnHit)}, {Pay(ESkillEffectType::Heal, ETargetType::Self, 2)}),
				Eff({Cond(ESkillTrigger::OnTurnEnd)}, {Pay(ESkillEffectType::EnergyRestore, ETargetType::Self, 1)})};
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Gear),
						{TEXT("On Hit"), TEXT("Heal 2"), TEXT("At the end of your turn"), TEXT("Gain 1 Energy")});
		});

		It("merges headings with the same words even when the rows are apart", [this]()
		{
			const TArray<FSkillEffect> Effects = {
				Eff({Cond(ESkillTrigger::OnHit)}, {Pay(ESkillEffectType::Heal, ETargetType::Self, 2)}),
				Eff({Cond(ESkillTrigger::OnTurnEnd)}, {Pay(ESkillEffectType::EnergyRestore, ETargetType::Self, 1)}),
				Eff({Cond(ESkillTrigger::OnHit)}, {Pay(ESkillEffectType::EnergyRestore, ETargetType::Self, 3)})};
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Gear),
						{TEXT("On Hit"), TEXT("Heal 2"), TEXT("Gain 3 Energy"), TEXT("At the end of your turn"), TEXT("Gain 1 Energy")});
		});

		It("puts untriggered gear under Passive and an untriggered skill effect under Activate", [this]()
		{
			const TArray<FSkillEffect> Effects = {Eff({}, {Pay(ESkillEffectType::DefenseBuff, ETargetType::Self, 0, 0.2f, 3)})};
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Gear), {TEXT("Passive"), TEXT("Gain +20% Defense for 3 turns")});
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Skill), {TEXT("Activate"), TEXT("Gain +20% Defense for 3 turns")});
		});

		It("names another side's event as a sentence and joins either-or triggers", [this]()
		{
			TestEqual(TEXT("Team member"),
					  EffectText::DescribeHeading(Eff({Cond(ESkillTrigger::OnTakeDamage, ECondCombine::And, ECondSubject::SelfTeam)}, {}),
												  EEffectTextSource::Gear).ToString(),
					  TEXT("When a Team Member takes damage"));
			TestEqual(TEXT("Either trigger"),
					  EffectText::DescribeHeading(Eff({Cond(ESkillTrigger::OnHit, ECondCombine::Or), Cond(ESkillTrigger::OnCrit, ECondCombine::Or)}, {}),
												  EEffectTextSource::Gear).ToString(),
					  TEXT("On Hit or On Critical Hit"));
		});

		It("builds headings from the AND and OR sets the way the game evaluates them", [this]()
		{
			TestEqual(TEXT("A lone OR entry must hold, so it reads as AND"),
					  EffectText::DescribeHeading(Eff({Cond(ESkillTrigger::OnHit), Cond(ESkillTrigger::OnCrit, ECondCombine::Or)}, {}),
												  EEffectTextSource::Gear).ToString(),
					  TEXT("On Hit and On Critical Hit"));
			TestEqual(TEXT("Every AND event, then the any-of set"),
					  EffectText::DescribeHeading(Eff({Cond(ESkillTrigger::OnHit, ECondCombine::Or), Cond(ESkillTrigger::OnCrit),
													   Cond(ESkillTrigger::OnDodge, ECondCombine::Or)}, {}),
												  EEffectTextSource::Gear).ToString(),
					  TEXT("On Critical Hit and either On Hit or On Dodge"));
		});

		It("reads an any-of set of an event and a state check as one either-or heading", [this]()
		{
			const TArray<FSkillEffect> Effects = {Eff(
				{Cond(ESkillTrigger::OnHit, ECondCombine::Or), Cond(ESkillTrigger::OnHPBelowThreshold, ECondCombine::Or)},
				{Pay(ESkillEffectType::Heal, ETargetType::Self, 2)})};
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Gear),
						{TEXT("On Hit or while your HP is below 30%"), TEXT("Heal 2")});
		});

		It("puts Passive last", [this]()
		{
			const TArray<FSkillEffect> Effects = {
				Eff({}, {Pay(ESkillEffectType::DefenseBuff, ETargetType::Self, 0, 0.2f)}),
				Eff({Cond(ESkillTrigger::OnHit)}, {Pay(ESkillEffectType::Heal, ETargetType::Self, 2)})};
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Gear),
						{TEXT("On Hit"), TEXT("Heal 2"), TEXT("Passive"), TEXT("Gain +20% Defense")});
			ExpectLines(EffectText::BuildTree(Effects, EEffectTextSource::Skill),
						{TEXT("Activate"), TEXT("Gain +20% Defense"), TEXT("On Hit"), TEXT("Heal 2")});
			FSkillEffect CostCut = Eff({}, {Pay(ESkillEffectType::ModifyEnergyCost, ETargetType::Self, 0, 0.2f)});
			const TArray<FEffectTextLine> CostLines = EffectText::BuildTree({CostCut}, EEffectTextSource::Gear);
			if (TestEqual(TEXT("Cost lines"), CostLines.Num(), 2))
			{
				TestFalse(TEXT("A cheaper cost is not a debuff"), CostLines[1].bDebuff);
			}
		});

		It("prints the OR checks as one level that holds when any does", [this]()
		{
			const TArray<FSkillEffect> Effects = {Eff(
				{Cond(ESkillTrigger::OnHit), Cond(ESkillTrigger::OnHPBelowThreshold, ECondCombine::Or),
				 Cond(ESkillTrigger::OnEnergyBelowThreshold, ECondCombine::Or, ECondSubject::Self, LOW_ENERGY_THRESHOLD)},
				{Pay(ESkillEffectType::Heal, ETargetType::Self, 2)})};
			const TArray<FEffectTextLine> Lines = EffectText::BuildTree(Effects, EEffectTextSource::Gear);
			ExpectLines(Lines, {TEXT("On Hit"), TEXT("While your HP is below 30% or your Energy is below 20%"), TEXT("Heal 2")});
			if (Lines.Num() == 3)
			{
				TestTrue(TEXT("Any of"), Lines[1].bAnyOf);
				TestEqual(TEXT("Both OR conditions"), Lines[1].ConditionIndices.Num(), 2);
			}
		});

		It("adds stacking and once-per-fight to the effect", [this]()
		{
			FSkillEffect Effect = Eff({Cond(ESkillTrigger::OnHit)}, {Pay(ESkillEffectType::DefenseBuff, ETargetType::Self, 0, 0.1f)});
			Effect.bStackable = true;
			Effect.MaxStacks = 3;
			Effect.bFiresOncePerMatch = true;
			ExpectLines(EffectText::BuildTree({Effect}, EEffectTextSource::Gear),
						{TEXT("On Hit"), TEXT("Gain +10% Defense, stacks up to 3 times, once per fight")});
			Effect.MaxStacks = 1;
			Effect.bFiresOncePerMatch = false;
			ExpectLines(EffectText::BuildTree({Effect}, EEffectTextSource::Gear),
						{TEXT("On Hit"), TEXT("Gain +10% Defense, stacks up to 1 time")});
		});

		It("gives the same text for the same data", [this]()
		{
			const TArray<FSkillEffect> Effects = {
				Eff({Cond(ESkillTrigger::OnHit), Cond(ESkillTrigger::OnHPBelowThreshold)},
					{Pay(ESkillEffectType::DOT, ETargetType::Enemy, 3, 0.0f, 2, ESpellElement::Fire)}),
				Eff({}, {Pay(ESkillEffectType::ResistanceBuff, ETargetType::Ally, 0, 0.15f, 2)})};
			TestEqual(TEXT("Tree"), EffectText::ToMultilineText(EffectText::BuildTree(Effects, EEffectTextSource::Gear)).ToString(),
					  EffectText::ToMultilineText(EffectText::BuildTree(Effects, EEffectTextSource::Gear)).ToString());
			TestEqual(TEXT("Rendered"), EffectText::ToMultilineText(EffectText::BuildTree(Effects, EEffectTextSource::Gear)).ToString(),
					  FString(TEXT("On Hit\n  While your HP is below 30%\n    • Deal 3 Fire damage to an Enemy each turn for 2 turns\nPassive\n  • Give an Ally +15% Resistance for 2 turns")));
		});
	});

	Describe("the short form", [this]()
	{
		It("prints one line per trigger, starred when anything under it has a condition", [this]()
		{
			const TArray<FSkillEffect> Effects = {
				Eff({Cond(ESkillTrigger::OnHit), Cond(ESkillTrigger::OnHPBelowThreshold)},
					{Pay(ESkillEffectType::Heal, ETargetType::Self, 3), Pay(ESkillEffectType::EnergyRestore, ETargetType::Self, 1)}),
				Eff({Cond(ESkillTrigger::OnTurnStart)}, {Pay(ESkillEffectType::Heal, ETargetType::Self, 2)})};
			const TArray<FText> Short = EffectText::BuildShortForm(Effects, EEffectTextSource::Gear);
			if (TestEqual(TEXT("Lines"), Short.Num(), 2))
			{
				TestEqual(TEXT("Conditional"), Short[0].ToString(), FString(TEXT("On Hit*: Heal 3, gain 1 Energy")));
				TestEqual(TEXT("Plain"), Short[1].ToString(), FString(TEXT("At the start of your turn: Heal 2")));
			}
		});
	});

	Describe("the effect bodies", [this]()
	{
		It("read as the deckbuilder's verbs", [this]()
		{
			TestEqual(TEXT("DoT"), Body(Pay(ESkillEffectType::DOT, ETargetType::Enemy, 3, 0.0f, 2, ESpellElement::Fire)),
					  FString(TEXT("deal 3 Fire damage to an Enemy each turn for 2 turns")));
			TestEqual(TEXT("DoT at self"), Body(Pay(ESkillEffectType::DOT, ETargetType::Self, 3, 0.0f, 2)),
					  FString(TEXT("take 3 damage each turn for 2 turns")));
			TestEqual(TEXT("Heal an ally"), Body(Pay(ESkillEffectType::Heal, ETargetType::Ally, 3)), FString(TEXT("heal an Ally for 3")));
			TestEqual(TEXT("Regen"), Body(Pay(ESkillEffectType::HealthRestore, ETargetType::Self, 2, 0.0f, 3)),
					  FString(TEXT("heal 2 each turn for 3 turns")));
			TestEqual(TEXT("Give a debuff"), Body(Pay(ESkillEffectType::DefenseDebuff, ETargetType::Enemy, 0, 0.2f)),
					  FString(TEXT("give an Enemy -20% Defense")));
			TestEqual(TEXT("Lose"), Body(Pay(ESkillEffectType::DefenseDebuff, ETargetType::Self, 0, 0.2f)), FString(TEXT("lose 20% Defense")));
			TestEqual(TEXT("Element-keyed stat"), Body(Pay(ESkillEffectType::StatusMultiplierBuff, ETargetType::Self, 0, 0.25f, 0, ESpellElement::Lightning)),
					  FString(TEXT("gain +25% Lightning Status Multiplier")));
			TestEqual(TEXT("Drain"), Body(Pay(ESkillEffectType::DrainEnergy, ETargetType::Enemy, 2)), FString(TEXT("drain 2 Energy from an Enemy")));
			TestEqual(TEXT("Delay, plural"), Body(Pay(ESkillEffectType::SkipTurn, ETargetType::Enemy, 0, 0.0f, 1, ESpellElement::None, ETargetCount::All)),
					  FString(TEXT("delay all Enemies' next turns")));
			TestEqual(TEXT("Delay, singular"), Body(Pay(ESkillEffectType::SkipTurn, ETargetType::Enemy, 0, 0.0f, 2)),
					  FString(TEXT("delay an Enemy's next turn")));
			TestEqual(TEXT("Fronted"), Body(Pay(ESkillEffectType::ReduceDamageTaken, ETargetType::Ally, 0, 0.2f, 0, ESpellElement::None, ETargetCount::All)),
					  FString(TEXT("all Allies take 20% less damage")));
			TestEqual(TEXT("One extra turn"), Body(Pay(ESkillEffectType::ExtraAction, ETargetType::Self, 2)), FString(TEXT("take an extra turn")));
			TestEqual(TEXT("Positive Energy cost is cheaper"), Body(Pay(ESkillEffectType::ModifyEnergyCost, ETargetType::Self, 0, 0.2f)),
					  FString(TEXT("your Refractions and Abilities cost 20% less")));
			TestEqual(TEXT("Random stat"), Body(Pay(ESkillEffectType::RandomDebuff, ETargetType::Enemy, 0, 0.1f, 2)),
					  FString(TEXT("give an Enemy -10% to a random stat for 2 turns")));
			TestEqual(TEXT("Stun"), Body(Pay(ESkillEffectType::Stun, ETargetType::Enemy, 0, 0.0f, 1)), FString(TEXT("stun an Enemy for 1 turn")));
			TestEqual(TEXT("Immunity"), Body(Pay(ESkillEffectType::GrantFireImmunity, ETargetType::Self, 0, 0.0f, 2)),
					  FString(TEXT("become immune to Fire for 2 turns")));
			TestEqual(TEXT("Negate"), Body(Pay(ESkillEffectType::AbsorbDamage, ETargetType::Ally, 5)),
					  FString(TEXT("negate the next 5 damage an Ally takes")));
			TestEqual(TEXT("Two targets"), Body(Pay(ESkillEffectType::Heal, ETargetType::Ally, 4, 0.0f, 0, ESpellElement::None, ETargetCount::Double)),
					  FString(TEXT("heal 2 Allies for 4")));
		});

		It("have a sentence for every effect kind", [this]()
		{
			const UEnum *Enum = StaticEnum<ESkillEffectType>();
			if (!TestNotNull(TEXT("Enum"), Enum))
			{
				return;
			}
			// The last entry is the generated _MAX.
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
			{
				const ESkillEffectType Type = static_cast<ESkillEffectType>(Enum->GetValueByIndex(Index));
				// None is no effect; ModifyHealing is dead and hidden from authoring (ESkillEffectType.h).
				if (Type == ESkillEffectType::None || Type == ESkillEffectType::ModifyHealing)
				{
					continue;
				}
				const FString Text = Body(Pay(Type, ETargetType::Enemy, 1));
				TestNotEqual(FString::Printf(TEXT("%s has a sentence"), *Enum->GetNameStringByIndex(Index)),
							 Text, WoRGameNames::GetSkillEffectTypeName(Type).ToString());
			}
		});
	});

	Describe("live lighting", [this]()
	{
		It("leaves the base item's text unlit", [this]()
		{
			const TArray<FSkillEffect> Effects = {Eff({Cond(ESkillTrigger::OnHit), Cond(ESkillTrigger::OnHPBelowThreshold)},
													   {Pay(ESkillEffectType::Heal, ETargetType::Self, 2)})};
			for (const FEffectTextLine &Line : EffectText::BuildTree(Effects, EEffectTextSource::Gear))
			{
				TestEqual(TEXT("Unlit"), Line.Light, EEffectTextLight::Unlit);
			}
		});

		It("marks each condition line held or not held", [this]()
		{
			const TArray<FSkillEffect> Effects = {
				Eff({Cond(ESkillTrigger::OnHit), Cond(ESkillTrigger::OnHPBelowThreshold),
					 Cond(ESkillTrigger::OnEnergyBelowThreshold, ECondCombine::And, ECondSubject::Self, LOW_ENERGY_THRESHOLD)},
					{Pay(ESkillEffectType::Heal, ETargetType::Self, 2)}),
				Eff({Cond(ESkillTrigger::OnTurnEnd), Cond(ESkillTrigger::OnHPAboveThreshold, ECondCombine::Or),
					 Cond(ESkillTrigger::OnHPBelowThreshold, ECondCombine::Or)},
					{Pay(ESkillEffectType::DefenseDebuff, ETargetType::Enemy, 0, 0.1f)})};
			TArray<FEffectTextLine> Lines = EffectText::BuildTree(Effects, EEffectTextSource::Gear);
			EffectText::LightTree(Lines, Effects, [](const FSkillCondition &C) { return C.Trigger == ESkillTrigger::OnHPBelowThreshold; });

			const FEffectTextLine *HP = FindLine(Lines, TEXT("While your HP is below 30%"));
			const FEffectTextLine *Energy = FindLine(Lines, TEXT("While your Energy is below 20%"));
			const FEffectTextLine *Either = FindLine(Lines, TEXT("While your HP is above 30% or your HP is below 30%"));
			if (TestNotNull(TEXT("HP line"), HP) && TestNotNull(TEXT("Energy line"), Energy) && TestNotNull(TEXT("Either line"), Either))
			{
				TestEqual(TEXT("HP holds"), HP->Light, EEffectTextLight::Held);
				TestEqual(TEXT("Energy does not"), Energy->Light, EEffectTextLight::NotHeld);
				TestEqual(TEXT("One of the OR checks holds"), Either->Light, EEffectTextLight::Held);
				TestTrue(TEXT("A debuff branch lights red"), Either->bDebuff);
				TestFalse(TEXT("A buff branch lights green"), HP->bDebuff);
			}
			const FEffectTextLine *Heading = FindLine(Lines, TEXT("On Hit"));
			if (TestNotNull(TEXT("Heading"), Heading))
			{
				TestEqual(TEXT("Headings are not lit"), Heading->Light, EEffectTextLight::Unlit);
			}
		});

		It("reads the character's HP through the effect manager", [this]()
		{
			FWoRTestWorld TestWorld;
			if (!TestTrue(TEXT("Test world"), TestWorld.Create()))
			{
				return;
			}
			AActor *Actor = TestWorld.SpawnCombatant(FWoRTestWorld::NewCharacterData());
			UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
			const USkillEffectManager *Manager = TestWorld.GetSubsystem<USkillEffectManager>();
			if (!TestNotNull(TEXT("Character"), CharComp) || !TestNotNull(TEXT("Manager"), Manager))
			{
				return;
			}
			constexpr int32 TEST_MAX_HP = 100;
			constexpr int32 TEST_LOW_HP = 10;
			CharComp->MaxHP = TEST_MAX_HP;
			CharComp->CurrentHP = TEST_LOW_HP;
			TestTrue(TEXT("HP below 30% holds"), Manager->IsConditionHeld(Cond(ESkillTrigger::OnHPBelowThreshold), Actor, nullptr));
			TestFalse(TEXT("HP above 30% does not"), Manager->IsConditionHeld(Cond(ESkillTrigger::OnHPAboveThreshold), Actor, nullptr));
			TestTrue(TEXT("No target: the runtime skips a target check, so it holds"),
					  Manager->IsConditionHeld(Cond(ESkillTrigger::OnHPBelowThreshold, ECondCombine::And, ECondSubject::Target), Actor, nullptr));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
