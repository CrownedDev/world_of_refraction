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
		Other = FWoRTestWorld::NewCharacterData();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
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
