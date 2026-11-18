// TrialKitSightSpec.cpp
// B-572 (D-225, TrialPoolDesign §7, stage 8): seeing an enemy's kit. The rules, headless: which variation a character
// has from its known skills, what the card shows in full and as the type's shared part, when the field use is refused,
// and the run's memory of seen types. Unproven here: the use on a real map enemy and the card on screen (PIE sheet B-572).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Misc/AutomationTest.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/EnemyKitRoll.h"
#include "Trial/TrialKitSight.h"
#include "Trial/TrialRunSubsystem.h"
#include "WoRTestWorld.h"

BEGIN_DEFINE_SPEC(FTrialKitSightSpec, "WoR.Trial.KitSight", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	const FName KNOWER = TEXT("KitSightKnower");

	static USpellData *MakeSpell(EKitSight Sight)
	{
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->KitSight = Sight;
		return Spell;
	}
END_DEFINE_SPEC(FTrialKitSightSpec)

void FTrialKitSightSpec::Define()
{
	It("reads the variation from the skills a character knows: none, the field one, or always on", [this]()
	{
		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		UPoolSubsystem *Pool = TestWorld.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("pool"), Pool)) return;
		TestEqual(TEXT("never banked: none"), TrialKitSight::GetKnownSight(*Pool, KNOWER), EKitSight::None);
		Pool->BankSkills(KNOWER, {FSpellInstance(MakeSpell(EKitSight::None))}, {});
		TestEqual(TEXT("an ordinary spell: none"), TrialKitSight::GetKnownSight(*Pool, KNOWER), EKitSight::None);
		Pool->SetSkills(KNOWER, {FSpellInstance(MakeSpell(EKitSight::None)), FSpellInstance(MakeSpell(EKitSight::Field))}, {});
		TestEqual(TEXT("the field variation"), TrialKitSight::GetKnownSight(*Pool, KNOWER), EKitSight::Field);
		Pool->SetSkills(KNOWER, {FSpellInstance(MakeSpell(EKitSight::AlwaysOn)), FSpellInstance(MakeSpell(EKitSight::Field))}, {});
		TestEqual(TEXT("both known: always on wins"), TrialKitSight::GetKnownSight(*Pool, KNOWER), EKitSight::AlwaysOn);
	});

	It("shows the whole kit in full, and only the always-has pieces as the type's shared part", [this]()
	{
		UCharacterData *Enemy = NewObject<UCharacterData>(GetTransientPackage());
		Enemy->Name = TEXT("Wandering Duelist");
		UWeaponData *Own = NewObject<UWeaponData>(GetTransientPackage());
		Own->Name = TEXT("Arming Sword");
		UWeaponData *Rolled = NewObject<UWeaponData>(GetTransientPackage());
		Rolled->Name = TEXT("Hand Axe");
		UAbilityData *Skill = NewObject<UAbilityData>(GetTransientPackage());
		Skill->Name = TEXT("Lunge");
		FRolledEnemyKit Kit;
		FRolledKitPiece &Always = Kit.Pieces.AddDefaulted_GetRef();
		Always.Piece = Own;
		Always.Tier = EItemTier::D_Tier;
		Always.bAlwaysHas = true;
		FRolledKitPiece &FromPool = Kit.Pieces.AddDefaulted_GetRef();
		FromPool.Piece = Rolled;
		Kit.Abilities.Add(Skill);

		const FEnemyKitCard Full = TrialKitSight::MakeCard(*Enemy, Kit, /*bFull=*/true);
		TestEqual(TEXT("its name"), Full.Name.ToString(), FString(TEXT("Wandering Duelist")));
		TestEqual(TEXT("both pieces"), Full.Pieces.Num(), 2);
		TestTrue(TEXT("a piece line names the piece and its tier"), Full.Pieces[0].ToString().Contains(TEXT("Arming Sword")) && Full.Pieces[0].ToString().Contains(TEXT("D")));
		TestTrue(TEXT("its skill"), Full.Skills.Num() == 1 && Full.Skills[0].ToString() == TEXT("Lunge"));
		TestFalse(TEXT("no rank word (D-187)"), Full.ClassLine.ToString().Contains(TEXT("Elite")) || Full.ClassLine.ToString().Contains(TEXT("Boss")));

		const FEnemyKitCard Shared = TrialKitSight::MakeCard(*Enemy, Kit, /*bFull=*/false);
		TestTrue(TEXT("the shared part: the always-has piece only"), Shared.Pieces.Num() == 1 && Shared.Pieces[0].ToString().Contains(TEXT("Arming Sword")));
		TestTrue(TEXT("and no skills"), Shared.Skills.IsEmpty());
	});

	It("refuses the field use without the skill, out of range or short of EP; the run remembers a type until it ends", [this]()
	{
		const float Near = TrialKitSight::FIELD_RANGE * 0.5f;
		const int32 Enough = TrialKitSight::FIELD_EP_COST;
		TestFalse(TEXT("no skill"), TrialKitSight::GetFieldRefusal(EKitSight::None, Enough, Near).IsEmpty());
		TestFalse(TEXT("too far"), TrialKitSight::GetFieldRefusal(EKitSight::Field, Enough, TrialKitSight::FIELD_RANGE + 1.0f).IsEmpty());
		TestFalse(TEXT("short of EP"), TrialKitSight::GetFieldRefusal(EKitSight::Field, Enough - 1, Near).IsEmpty());
		TestTrue(TEXT("allowed"), TrialKitSight::GetFieldRefusal(EKitSight::Field, Enough, Near).IsEmpty());
		TestTrue(TEXT("always on may use it too"), TrialKitSight::GetFieldRefusal(EKitSight::AlwaysOn, Enough, Near).IsEmpty());

		FWoRTestWorld TestWorld;
		TestTrue(TEXT("test world created"), TestWorld.Create());
		UTrialRunSubsystem *TrialRun = TestWorld.GetSubsystem<UTrialRunSubsystem>();
		if (!TestNotNull(TEXT("trial run"), TrialRun)) return;
		const FName Bandit = TEXT("Bandit");
		TestFalse(TEXT("not seen yet"), TrialRun->IsKitTypeSeen(Bandit));
		TrialRun->MarkKitTypeSeen(Bandit);
		TrialRun->MarkKitTypeSeen(Bandit);
		TestTrue(TEXT("seen once is seen"), TrialRun->IsKitTypeSeen(Bandit) && TrialRun->GetSeenKitTypeCount() == 1);
	});
}

#endif
