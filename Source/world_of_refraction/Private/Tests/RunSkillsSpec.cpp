// RunSkillsSpec.cpp
// B-523 (D-224 (2), stage S3): a run's skills. At a trial start a kit member's hub skill list is kept aside and the run
// list holds only what it keeps (innate spells) and the kit's StartingSkills (not slotted); a skill learned in the run
// goes into the run list; at the run's end the hub list comes back. The kept-aside list survives a save round trip.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Kismet/GameplayStatics.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	const FName RUN_SKILLS_KEY(TEXT("RunSkillsTester"));
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
		Pool->BankSkills(RUN_SKILLS_KEY, {FSpellInstance(Innate), FSpellInstance(HubSpell)}, {FAbilityInstance(HubAbility)});
		Pool->BeginRunLayer();
		Pool->BeginRunSkills(RUN_SKILLS_KEY, {Innate}, {Bolt});
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
