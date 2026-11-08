// WoRConditionsSpec.cpp
// B-337 (D-130): the one condition evaluator. Every memory condition kind gives the same answer
// through WoRConditions (after FromMemory) as through the old MemoryConditions::IsMet, met and not
// met, including the "whose" filter, the upgrade level, the level-up routes and requirement fits.
// Plus: a condition's trigger must match the event's, and a set needs all its conditions.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Conditions/WoRConditions.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/MemoryUnlock.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FName WOR_CONDITIONS_TEST_LOCATION(TEXT("Spot_WoRConditionsSpec"));
	const FName WOR_CONDITIONS_TEST_ELSEWHERE(TEXT("Elsewhere_WoRConditionsSpec"));
	constexpr int32 WOR_CONDITIONS_TEST_LEVEL = 2;
	constexpr int32 WOR_CONDITIONS_TEST_TOO_HIGH = WOR_CONDITIONS_TEST_LEVEL + 1;
	/** ER-59: Lord's Mind; one item asks for less (Lord is over by the gap), one for more (short by it). */
	constexpr int32 WOR_CONDITIONS_TEST_LORD_MIND = 4;
	constexpr int32 WOR_CONDITIONS_TEST_PILLAR_GAP = 2;
	const int32 WOR_CONDITIONS_TEST_MARGINS[] = {0, WOR_CONDITIONS_TEST_PILLAR_GAP, WOR_CONDITIONS_TEST_PILLAR_GAP + 1};

	EItemTier WoRConditionsTestTier(int32 StepsAboveF)
	{
		return static_cast<EItemTier>(static_cast<int32>(EItemTier::F_Tier) + StepsAboveF);
	}
}

BEGIN_DEFINE_SPEC(FWoRConditionsSpec, "WoR.Conditions",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UGameInstance *GameInstance = nullptr;
	UMemoryJournalSubsystem *Journal = nullptr;
	UCharacterData *Lord = nullptr;
	UCharacterData *Other = nullptr;
	UWeaponData *Sword = nullptr;
	/** Asks for Mind below Lord's: Lord meets it, over by the gap. */
	UWeaponData *Demanding = nullptr;
	/** Asks for Mind above Lord's: Lord is short by the gap. */
	UWeaponData *OutOfReach = nullptr;
	UEvolutionItemData *Evolution = nullptr;
	USpellData *Spell = nullptr;
	UMemoryData *Collected = nullptr;
	UMemoryData *NotCollected = nullptr;

	struct FCase
	{
		FString Name;
		FMemoryCondition Condition;
		FMemoryEvent Event;
	};
	TArray<FCase> Cases;

	static FMemoryCondition Make(EMemoryConditionKind Kind)
	{
		FMemoryCondition Out;
		Out.Kind = Kind;
		return Out;
	}

	/** Lord, wearing the sword and with the evolution at WOR_CONDITIONS_TEST_LEVEL, the spell slotted,
	 *  and every fight usage recorded. */
	FMemoryEvent FullEvent() const
	{
		FMemoryEvent Event;
		Event.Source = EMemorySource::FightWon;
		FMemoryEventMember Member;
		Member.Character = Lord;
		Member.Worn.Add({Sword, WoRConditionsTestTier(WOR_CONDITIONS_TEST_LEVEL)});
		Member.Worn.Add({Demanding, EItemTier::F_Tier});
		Member.Worn.Add({OutOfReach, EItemTier::F_Tier});
		Member.Evolution = Evolution;
		Member.EvolutionTier = WoRConditionsTestTier(WOR_CONDITIONS_TEST_LEVEL);
		Member.Slotted.Add(Spell);
		Member.Used.Skills.Add(Spell);
		Member.Used.Breakthroughs.Add(Evolution);
		Member.Used.FinishingWeapons.Add(Sword);
		Event.Party.Add(Member);
		Event.Locations.Add(WOR_CONDITIONS_TEST_LOCATION);
		Event.Beaten.Add(Other);
		Event.bNoDamageTaken = true;
		return Event;
	}

	/** Lord alone, nothing worn, used or beaten; damage taken. */
	FMemoryEvent BareEvent() const
	{
		FMemoryEvent Event;
		Event.Source = EMemorySource::Fragment;
		FMemoryEventMember Member;
		Member.Character = Lord;
		Event.Party.Add(Member);
		return Event;
	}

	/** A level-up event: Item / evolution levelled by Lord, nobody wearing anything. */
	FMemoryEvent UpgradeEvent() const
	{
		FMemoryEvent Event = BareEvent();
		Event.UpgradedItem = Sword;
		Event.UpgradedEvolution = Evolution;
		Event.UpgradedTier = WoRConditionsTestTier(WOR_CONDITIONS_TEST_LEVEL);
		Event.UpgradedBy = Lord;
		return Event;
	}

	void Add(const TCHAR *Name, const FMemoryCondition &Condition)
	{
		Cases.Add({FString::Printf(TEXT("%s, full event"), Name), Condition, FullEvent()});
		Cases.Add({FString::Printf(TEXT("%s, bare event"), Name), Condition, BareEvent()});
		Cases.Add({FString::Printf(TEXT("%s, level-up event"), Name), Condition, UpgradeEvent()});
	}

	/** Every kind, with variations, each against the three events. */
	void BuildCases()
	{
		FMemoryCondition C = Make(EMemoryConditionKind::Character);
		C.Character = Lord;
		Add(TEXT("Character Lord"), C);
		C.Character = Other;
		Add(TEXT("Character Other"), C);
		// B-376: "party has" reads the whole team (Present), else the party, as Character does solo.
		FMemoryCondition Has = Make(EMemoryConditionKind::PartyHas);
		Has.Character = Lord;
		Add(TEXT("PartyHas Lord"), Has);
		Has.Character = Other;
		Add(TEXT("PartyHas Other"), Has);
		// B-378: the old evaluator has no curse state; with none held both say no (CurseSpec covers a held curse).
		FMemoryCondition Curse = Make(EMemoryConditionKind::CurseHeld);
		Curse.CurseId = TEXT("Curse.Parity");
		Add(TEXT("CurseHeld, none held"), Curse);
		// B-379: the old evaluator has no counters; with none counted both say no (HiddenCounterSpec covers a reached one).
		FMemoryCondition Counter = Make(EMemoryConditionKind::CounterReached);
		Counter.CounterId = TEXT("Counter.Parity");
		Counter.CounterTarget = 1;
		Add(TEXT("CounterReached, none counted"), Counter);

		for (const EMemoryRequirementFit Fit : {EMemoryRequirementFit::Any, EMemoryRequirementFit::Met, EMemoryRequirementFit::Under})
		{
			const FString Suffix = FString::Printf(TEXT(" fit %d"), static_cast<int32>(Fit));
			FMemoryCondition Gear = Make(EMemoryConditionKind::GearEquipped);
			Gear.Item = Sword;
			Gear.UpgradeLevel = WOR_CONDITIONS_TEST_LEVEL;
			Gear.RequirementFit = Fit;
			Add(*(TEXT("Gear at level") + Suffix), Gear);
			Gear.UpgradeLevel = WOR_CONDITIONS_TEST_TOO_HIGH;
			Add(*(TEXT("Gear too high") + Suffix), Gear);
			Gear.UpgradeLevel = WOR_CONDITIONS_TEST_LEVEL;
			Gear.Character = Other;
			Add(*(TEXT("Gear, someone else's") + Suffix), Gear);

			FMemoryCondition Evo = Make(EMemoryConditionKind::EvolutionEquipped);
			Evo.Evolution = Evolution;
			Evo.UpgradeLevel = WOR_CONDITIONS_TEST_LEVEL;
			Evo.RequirementFit = Fit;
			Add(*(TEXT("Evolution at level") + Suffix), Evo);
			Evo.UpgradeLevel = WOR_CONDITIONS_TEST_TOO_HIGH;
			Add(*(TEXT("Evolution too high") + Suffix), Evo);

			FMemoryCondition Slotted = Make(EMemoryConditionKind::SkillSlotted);
			Slotted.Skill = Spell;
			Slotted.RequirementFit = Fit;
			Add(*(TEXT("Skill slotted") + Suffix), Slotted);
		}

		// ER-59: the pillar maths, every fit and margin, on items with requirements.
		for (const EMemoryRequirementFit Fit :
			 {EMemoryRequirementFit::Any, EMemoryRequirementFit::Met, EMemoryRequirementFit::Above, EMemoryRequirementFit::Under})
		{
			for (const int32 Margin : WOR_CONDITIONS_TEST_MARGINS)
			{
				for (UWeaponData *Item : {Demanding, OutOfReach})
				{
					FMemoryCondition Gear = Make(EMemoryConditionKind::GearEquipped);
					Gear.Item = Item;
					Gear.RequirementFit = Fit;
					Gear.RequirementMargin = Margin;
					Add(*FString::Printf(TEXT("%s fit %d margin %d"), Item == Demanding ? TEXT("Demanding") : TEXT("Out of reach"),
										 static_cast<int32>(Fit), Margin), Gear);
				}
			}
		}

		FMemoryCondition Location = Make(EMemoryConditionKind::Location);
		Location.LocationId = WOR_CONDITIONS_TEST_LOCATION;
		Add(TEXT("Location here"), Location);
		Location.LocationId = WOR_CONDITIONS_TEST_ELSEWHERE;
		Add(TEXT("Location elsewhere"), Location);

		FMemoryCondition Beaten = Make(EMemoryConditionKind::EnemyBeaten);
		Beaten.Enemy = Other;
		Add(TEXT("Enemy beaten"), Beaten);

		Add(TEXT("No damage taken"), Make(EMemoryConditionKind::NoDamageTaken));

		FMemoryCondition Prior = Make(EMemoryConditionKind::PriorMemory);
		Prior.PriorMemory = Collected;
		Add(TEXT("Prior memory collected"), Prior);
		Prior.PriorMemory = NotCollected;
		Add(TEXT("Prior memory not collected"), Prior);

		Add(TEXT("Hidden"), Make(EMemoryConditionKind::Hidden));

		FMemoryCondition Used = Make(EMemoryConditionKind::SkillUsed);
		Used.Skill = Spell;
		Add(TEXT("Skill used"), Used);

		FMemoryCondition Breakthrough = Make(EMemoryConditionKind::BreakthroughUsed);
		Breakthrough.Evolution = Evolution;
		Add(TEXT("Breakthrough used"), Breakthrough);

		FMemoryCondition Finishing = Make(EMemoryConditionKind::FinishingBlow);
		Finishing.Item = Sword;
		Add(TEXT("Finishing blow"), Finishing);
		Finishing.Character = Other;
		Add(TEXT("Finishing blow, someone else's"), Finishing);
	}
END_DEFINE_SPEC(FWoRConditionsSpec)

void FWoRConditionsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		GameInstance = TestWorld->GetWorld() ? TestWorld->GetWorld()->GetGameInstance() : nullptr;
		Journal = TestWorld->GetSubsystem<UMemoryJournalSubsystem>();
		Lord = FWoRTestWorld::NewCharacterData();
		Lord->WorldMindLevel = WOR_CONDITIONS_TEST_LORD_MIND;
		Other = FWoRTestWorld::NewCharacterData();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Demanding = NewObject<UWeaponData>(GetTransientPackage());
		Demanding->Requirements.RequiredWorldMind = WOR_CONDITIONS_TEST_LORD_MIND - WOR_CONDITIONS_TEST_PILLAR_GAP;
		OutOfReach = NewObject<UWeaponData>(GetTransientPackage());
		OutOfReach->Requirements.RequiredWorldMind = WOR_CONDITIONS_TEST_LORD_MIND + WOR_CONDITIONS_TEST_PILLAR_GAP;
		Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		Spell = NewObject<USpellData>(GetTransientPackage());
		Collected = NewObject<UMemoryData>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMemoryData::StaticClass()));
		NotCollected = NewObject<UMemoryData>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMemoryData::StaticClass()));
		if (TestTrue(TEXT("Subsystems"), GameInstance && Journal))
		{
			TestTrue(TEXT("one memory collected"), MemoryUnlock::Unlock(GameInstance, Collected, EMemorySource::Fragment));
		}
		Cases.Reset();
		BuildCases();
	});

	AfterEach([this]()
	{
		Cases.Reset();
		TestWorld.Reset();
	});

	It("gives every memory condition kind the same answer as the old evaluator", [this]()
	{
		if (!Journal) return;
		TSet<EMemoryConditionKind> Kinds;
		int32 MetCount = 0;
		for (const FCase &Case : Cases)
		{
			const bool bOld = MemoryConditions::IsMet(Case.Condition, Case.Event, Journal);
			const bool bNew = WoRConditions::IsMet(WoRConditions::FromMemory(Case.Condition), WoRConditions::MakeContext(Case.Event, Journal));
			TestTrue(Case.Name, bNew == bOld);
			Kinds.Add(Case.Condition.Kind);
			MetCount += bOld ? 1 : 0;
		}
		const UEnum *KindEnum = StaticEnum<EMemoryConditionKind>();
		TestEqual(TEXT("every kind covered"), Kinds.Num(), static_cast<int32>(KindEnum->NumEnums() - 1)); // minus the generated _MAX
		TestTrue(TEXT("some cases met, some not"), MetCount > 0 && MetCount < Cases.Num());
	});

	It("measures the wearer against requirements: over and short by a margin (ER-59)", [this]()
	{
		const FMemoryEvent Full = FullEvent();
		const FWoRConditionContext Context = WoRConditions::MakeContext(Full, Journal);
		auto Fits = [&](UWeaponData *Item, EWoRRequirementFit Fit, int32 Margin)
		{
			FWoRCondition Gear;
			Gear.Subject = EWoRConditionSubject::GearEquipped;
			Gear.Item = Item;
			Gear.RequirementFit = Fit;
			Gear.RequirementMargin = Margin;
			return WoRConditions::IsMet(Gear, Context);
		};
		TestTrue(TEXT("Met: Lord meets the lower ask"), Fits(Demanding, EWoRRequirementFit::Met, 0));
		TestFalse(TEXT("Met: not the higher one"), Fits(OutOfReach, EWoRRequirementFit::Met, 0));
		TestTrue(TEXT("Above by the gap"), Fits(Demanding, EWoRRequirementFit::Above, WOR_CONDITIONS_TEST_PILLAR_GAP));
		TestFalse(TEXT("Not above by more than the gap"), Fits(Demanding, EWoRRequirementFit::Above, WOR_CONDITIONS_TEST_PILLAR_GAP + 1));
		TestTrue(TEXT("Under by the gap"), Fits(OutOfReach, EWoRRequirementFit::Under, WOR_CONDITIONS_TEST_PILLAR_GAP));
		TestFalse(TEXT("Not under by more than the gap"), Fits(OutOfReach, EWoRRequirementFit::Under, WOR_CONDITIONS_TEST_PILLAR_GAP + 1));
		TestFalse(TEXT("Under: not when it meets them"), Fits(Demanding, EWoRRequirementFit::Under, 0));
	});

	It("needs the event's trigger when a condition names one", [this]()
	{
		FWoRCondition Condition;
		Condition.Subject = EWoRConditionSubject::None;
		Condition.Trigger = EWoRConditionTrigger::FightWon;
		const FMemoryEvent Won = FullEvent();
		const FMemoryEvent Fragment = BareEvent();
		TestTrue(TEXT("on a fight won"), WoRConditions::IsMet(Condition, WoRConditions::MakeContext(Won, Journal)));
		TestFalse(TEXT("not on a fragment"), WoRConditions::IsMet(Condition, WoRConditions::MakeContext(Fragment, Journal)));
		Condition.Trigger = EWoRConditionTrigger::Any;
		TestTrue(TEXT("Any: on a fragment too"), WoRConditions::IsMet(Condition, WoRConditions::MakeContext(Fragment, Journal)));
	});

	It("needs every condition of a set, and any one set of several", [this]()
	{
		FWoRCondition InParty;
		InParty.Subject = EWoRConditionSubject::CharacterInParty;
		InParty.Character = Lord;
		FWoRCondition Clean;
		Clean.Subject = EWoRConditionSubject::NoDamageTaken;
		FWoRConditionSet Both;
		Both.Conditions = {InParty, Clean};
		FWoRConditionSet PartyOnly;
		PartyOnly.Conditions = {InParty};
		const FMemoryEvent Bare = BareEvent();
		const FWoRConditionContext Context = WoRConditions::MakeContext(Bare, Journal);
		TestFalse(TEXT("one of two fails: the set fails"), WoRConditions::IsMet(Both, Context));
		TestTrue(TEXT("another route holds"), WoRConditions::IsAnyMet({Both, PartyOnly}, Context));
		TestTrue(TEXT("no routes: nothing to meet"), WoRConditions::IsAnyMet({}, Context));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
