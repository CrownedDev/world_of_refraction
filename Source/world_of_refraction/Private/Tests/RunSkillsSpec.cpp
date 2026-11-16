// RunSkillsSpec.cpp
// B-523 (D-224 (2), stage S3): a run's skills. At a trial start a kit member's hub skill list is kept aside and the run
// list holds only what it keeps (innate spells) and the kit's StartingSkills (not slotted); a skill learned in the run
// goes into the run list; at the run's end the hub list comes back. The kept-aside list survives a save round trip.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/WornGear.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FName RUN_SKILLS_KEY(TEXT("RunSkillsTester"));
	constexpr EItemTier RUN_SKILLS_START_TIER = EItemTier::S_Tier;
	constexpr EItemTier RUN_SKILLS_DROPPED_TIER = EItemTier::C_Tier; // S less 3
}

BEGIN_DEFINE_SPEC(FRunSkillsSpec, "WoR.Run.Skills", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	USpellData *Innate = nullptr;
	USpellData *HubSpell = nullptr;
	UAbilityData *HubAbility = nullptr;
	USpellData *Bolt = nullptr;
	USpellData *Bought = nullptr;

	USpellData *NewSpell(const TCHAR *Name)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = Name;
		return Spell;
	}

	/** The hub knows its innate spell, a learned spell and a learned ability; then the trial starts. */
	void StartRun()
	{
		const auto Owned = [](auto Instance, EItemTier Tier) { Instance.Tier = Tier; return Instance; }; // as learned: the asset's tier
		Pool->BankSkills(RUN_SKILLS_KEY, {Owned(FSpellInstance(Innate), Innate->Tier), Owned(FSpellInstance(HubSpell), HubSpell->Tier)},
		                 {Owned(FAbilityInstance(HubAbility), HubAbility->Tier)});
		Pool->BeginRunLayer();
		Pool->BeginRunSkills(RUN_SKILLS_KEY, {Innate}, {Bolt});
	}

	/** B-525: the run buys Bought and HubSpell (back) and slots both in a worn adapter entry; the bolt stays unslotted. */
	void StartRunAndSlot()
	{
		for (USpellData *Spell : {Innate, HubSpell, Bought})
		{
			Spell->Tier = RUN_SKILLS_START_TIER;
		}
		HubAbility->Tier = RUN_SKILLS_START_TIER;
		StartRun();
		Pool->LearnSkillCopy(RUN_SKILLS_KEY, Bought);
		Pool->LearnSkillCopy(RUN_SKILLS_KEY, HubSpell);
		FWornGear Worn;
		FLoadoutSlot &Entry = Worn.Equipment.AddDefaulted_GetRef();
		Entry.SpellOverrides = {HubSpell, Bought};
		Pool->SetWornGear(RUN_SKILLS_KEY, Worn);
	}

	EItemTier SpellTier(const USpellData *Spell) const
	{
		const FPoolSkills *Skills = Pool->FindSkills(RUN_SKILLS_KEY);
		const FSpellInstance *Found = Skills ? Skills->Spells.FindByPredicate([Spell](const FSpellInstance &I) { return I.Spell == Spell; }) : nullptr;
		return Found ? Found->Tier : EItemTier::F_Tier; // never a case's expected tier
	}

	EItemTier AbilityTier(const UAbilityData *Ability) const
	{
		const FPoolSkills *Skills = Pool->FindSkills(RUN_SKILLS_KEY);
		const FAbilityInstance *Found = Skills ? Skills->Abilities.FindByPredicate([Ability](const FAbilityInstance &I) { return I.Ability == Ability; }) : nullptr;
		return Found ? Found->Tier : EItemTier::F_Tier; // never a case's expected tier
	}
END_DEFINE_SPEC(FRunSkillsSpec)

void FRunSkillsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("world"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Innate = NewSpell(TEXT("Run skills innate"));
		HubSpell = NewSpell(TEXT("Run skills hub spell"));
		HubAbility = NewObject<UAbilityData>(GetTransientPackage());
		HubAbility->Name = TEXT("Run skills hub ability");
		Bolt = NewSpell(TEXT("Run skills bolt"));
		Bought = NewSpell(TEXT("Run skills bought"));
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Innate = HubSpell = Bolt = Bought = nullptr;
		HubAbility = nullptr;
		TestWorld.Reset();
	});

	It("starts the run with the innate spell and the kit's starting skill only", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun();
		TestTrue(TEXT("the innate spell stays"), Pool->KnowsSpell(RUN_SKILLS_KEY, Innate));
		TestTrue(TEXT("the starting bolt is in the list"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bolt));
		TestFalse(TEXT("the hub's learned spell is not"), Pool->KnowsSpell(RUN_SKILLS_KEY, HubSpell));
		TestFalse(TEXT("nor its learned ability"), Pool->KnowsAbility(RUN_SKILLS_KEY, HubAbility));
		const FPoolSkills *Hub = Pool->GetRunStartSkills().Find(RUN_SKILLS_KEY);
		TestTrue(TEXT("the hub list is kept aside"), Hub && Hub->Spells.Num() == 2 && Hub->Abilities.Num() == 1);
	});

	It("learns a skill bought in the run into the run list, and puts the hub list back at the end", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun();
		TestTrue(TEXT("bought in the run"), Pool->LearnSkillCopy(RUN_SKILLS_KEY, Bought));
		TestTrue(TEXT("known in the run"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bought));
		Pool->CloseRun(EPoolRunEnding::Quit);
		TestTrue(TEXT("the hub spell is back"), Pool->KnowsSpell(RUN_SKILLS_KEY, HubSpell));
		TestTrue(TEXT("the hub ability is back"), Pool->KnowsAbility(RUN_SKILLS_KEY, HubAbility));
		TestFalse(TEXT("a skill bought in a quit run is gone"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bought));
		TestFalse(TEXT("the bolt is gone"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bolt));
		TestTrue(TEXT("nothing left aside"), Pool->GetRunStartSkills().IsEmpty());
	});

	It("keeps the first hub list when a run that was never closed starts again", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun();
		Pool->BeginRunSkills(RUN_SKILLS_KEY, {Innate}, {Bolt}); // the run list now is not the hub's
		Pool->EndRunLayer();
		TestTrue(TEXT("the hub spell is back"), Pool->KnowsSpell(RUN_SKILLS_KEY, HubSpell));
	});

	// B-525: the settle at the end. The run buys Bought (a found skill, S) and HubSpell back (owned, S in the hub),
	// slots both, and leaves the bolt unslotted.
	It("keeps a slotted found skill on an extract, at its tier, and sends an unslotted one back", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRunAndSlot();
		Pool->CloseRun(EPoolRunEnding::Kept);
		TestTrue(TEXT("the slotted found skill joins the hub"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bought));
		TestFalse(TEXT("the unslotted bolt goes back"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bolt));
		TestEqual(TEXT("it keeps its tier"), SpellTier(Bought), RUN_SKILLS_START_TIER);
		TestEqual(TEXT("the owned one too"), SpellTier(HubSpell), RUN_SKILLS_START_TIER);
	});

	It("keeps a slotted found skill out of wipes, and every slotted skill drops 3 tiers", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRunAndSlot();
		Pool->CloseRun(EPoolRunEnding::Lost);
		TestTrue(TEXT("the slotted found skill joins the hub"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bought));
		TestEqual(TEXT("the found one: S to C"), SpellTier(Bought), RUN_SKILLS_DROPPED_TIER);
		TestEqual(TEXT("the owned one: S to C"), SpellTier(HubSpell), RUN_SKILLS_DROPPED_TIER);
		TestEqual(TEXT("an unslotted owned one keeps S"), AbilityTier(HubAbility), RUN_SKILLS_START_TIER);
		TestEqual(TEXT("the innate spell is not a gear pick: kept as it was"), SpellTier(Innate), RUN_SKILLS_START_TIER);
	});

	It("drops found skills on a quit, and the owned slotted ones drop 3 tiers", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRunAndSlot();
		Pool->CloseRun(EPoolRunEnding::Quit);
		TestFalse(TEXT("the found skill is lost"), Pool->KnowsSpell(RUN_SKILLS_KEY, Bought));
		TestEqual(TEXT("the owned slotted one: S to C"), SpellTier(HubSpell), RUN_SKILLS_DROPPED_TIER);
	});

	It("keeps the hub list aside through a save round trip", [this]()
	{
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		StartRun();
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(*Pool);
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("saved to memory"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return;
		UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;

		FWoRTestWorld After;
		if (!TestTrue(TEXT("second world"), After.Create())) return;
		UPoolSubsystem *PoolAfter = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("PoolAfter"), PoolAfter)) return;
		USaveSubsystem::RestorePool(Loaded->Data.Pool, *PoolAfter);
		TestFalse(TEXT("the run list is restored"), PoolAfter->KnowsSpell(RUN_SKILLS_KEY, HubSpell));
		PoolAfter->PutBackRunStartSkills();
		TestTrue(TEXT("the hub spell comes back after the load"), PoolAfter->KnowsSpell(RUN_SKILLS_KEY, HubSpell));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
