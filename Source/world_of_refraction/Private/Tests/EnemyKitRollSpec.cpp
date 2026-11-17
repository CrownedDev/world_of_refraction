// EnemyKitRollSpec.cpp
// B-568 (D-225, TrialPoolDesign stage 4): an enemy's kit rolled from a made-up pool with a fixed seed, headless.
// Unproven here: the real pool's assets and an enemy fighting with its kit (PIE sheet B-568).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Misc/AutomationTest.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/EnemyKitRoll.h"

BEGIN_DEFINE_SPEC(FEnemyKitRollSpec, "WoR.Trial.EnemyKitRoll", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static constexpr int32 RUN_SEED = 1234;
	static constexpr int32 TRIES = 20;

	UCharacterData *Enemy = nullptr;
	UWeaponData *Sword = nullptr;
	UWeaponData *Axe = nullptr;
	URingData *Adapter = nullptr;
	UAbilityData *SwordSkill = nullptr;
	UAbilityData *AxeSkill = nullptr;
	USpellData *FireSpell = nullptr;
	FEnemyKitRollInput Input;

	int32 CountWeapons(const FRolledEnemyKit &Kit) const
	{
		return Kit.Pieces.FilterByPredicate([](const FRolledKitPiece &Piece) { return Cast<UWeaponData>(Piece.Piece) != nullptr; }).Num();
	}
END_DEFINE_SPEC(FEnemyKitRollSpec)

void FEnemyKitRollSpec::Define()
{
	BeforeEach([this]()
	{
		Enemy = NewObject<UCharacterData>();
		Enemy->CharacterClass = ECharacterClass::Generic;
		Sword = NewObject<UWeaponData>();
		Sword->WeaponType = EWeaponType::Sword;
		Axe = NewObject<UWeaponData>();
		Axe->WeaponType = EWeaponType::Axe;
		Adapter = NewObject<URingData>();
		SwordSkill = NewObject<UAbilityData>();
		SwordSkill->RequiredWeaponType = EWeaponType::Sword;
		AxeSkill = NewObject<UAbilityData>();
		AxeSkill->RequiredWeaponType = EWeaponType::Axe;
		FireSpell = NewObject<USpellData>();
		FireSpell->Element = ESpellElement::Fire;
		Input = FEnemyKitRollInput();
		Input.Pool = {Sword, Axe, Adapter, SwordSkill, AxeSkill, FireSpell};
		Input.TrialElement = ESpellElement::Fire;
		Input.ZoneRange = TrialPool::GetZoneTierRange(2);
	});

	It("gives the same kit for the same seed, and a different one per enemy of the type", [this]()
	{
		Input.Seed = EnemyKitRoll::MakeSeed(RUN_SEED, EnemyKitRoll::MakeEncounterKey({TEXT("B"), TEXT("A")}), Enemy, 0);
		const FString First = EnemyKitRoll::GetRolledKitString(EnemyKitRoll::Roll(*Enemy, Input));
		TestEqual(TEXT("rolled again"), EnemyKitRoll::GetRolledKitString(EnemyKitRoll::Roll(*Enemy, Input)), First);
		TestEqual(TEXT("the encounters in any order"), EnemyKitRoll::MakeEncounterKey({TEXT("A"), TEXT("B")}), EnemyKitRoll::MakeEncounterKey({TEXT("B"), TEXT("A")}));
		bool bAnyDiffers = false;
		for (int32 Occurrence = 1; Occurrence < TRIES && !bAnyDiffers; ++Occurrence)
		{
			Input.Seed = EnemyKitRoll::MakeSeed(RUN_SEED, EnemyKitRoll::MakeEncounterKey({TEXT("A"), TEXT("B")}), Enemy, Occurrence);
			bAnyDiffers = EnemyKitRoll::GetRolledKitString(EnemyKitRoll::Roll(*Enemy, Input)) != First;
		}
		TestTrue(TEXT("another bandit rolls its own open part"), bAnyDiffers);
	});

	It("keeps the always-has part on every enemy of the type, and stays inside its limits", [this]()
	{
		FEnemyKitPiece Given;
		Given.Item = Sword;
		Given.Tier = EItemTier::D_Tier;
		Enemy->EnemyKit.Pieces = {Given};
		Enemy->EnemyKit.Skills = {SwordSkill};
		Enemy->EnemyKit.bLimitWeapons = true;
		Enemy->EnemyKit.MostWeapons = 1;
		for (int32 Occurrence = 0; Occurrence < TRIES; ++Occurrence)
		{
			Input.Seed = EnemyKitRoll::MakeSeed(RUN_SEED, TEXT("A"), Enemy, Occurrence);
			const FRolledEnemyKit Kit = EnemyKitRoll::Roll(*Enemy, Input);
			TestTrue(TEXT("the sword at D, first"), Kit.Pieces.Num() > 0 && Kit.Pieces[0].Piece == Sword && Kit.Pieces[0].Tier == EItemTier::D_Tier && Kit.Pieces[0].bAlwaysHas);
			TestEqual(TEXT("most weapons 1"), CountWeapons(Kit), 1);
			TestTrue(TEXT("a Generic: at most one adapter"), Kit.Pieces.Num() - CountWeapons(Kit) <= 1);
			TestTrue(TEXT("the always-has skill first"), Kit.Abilities.Num() > 0 && Kit.Abilities[0] == SwordSkill);
			TestFalse(TEXT("no axe skill without an axe"), Kit.Abilities.Contains(AxeSkill));
			const int32 Skills = Kit.Abilities.Num() + Kit.Spells.Num();
			TestTrue(TEXT("skills: at most 3 and the slots"), Skills <= EnemyKitRules::DEFAULT_MOST_SKILLS && Skills <= EnemyKitRoll::GetSlotCount(Kit));
		}
	});

	It("socketing only crystals of its elements, and spells only through them", [this]()
	{
		Enemy->EnemyKit.Elements = {ESpellElement::Water};
		for (int32 Occurrence = 0; Occurrence < TRIES; ++Occurrence)
		{
			Input.Seed = EnemyKitRoll::MakeSeed(RUN_SEED, TEXT("A"), Enemy, Occurrence);
			const FRolledEnemyKit Kit = EnemyKitRoll::Roll(*Enemy, Input);
			for (const FRolledKitPiece &Piece : Kit.Pieces)
			{
				const bool bWeapon = Cast<UWeaponData>(Piece.Piece) != nullptr;
				TestTrue(TEXT("weapons stay empty; adapters hold Water"), bWeapon ? Piece.Crystal.Type == ECrystalType::None
																				  : CrystalTypeHelpers::GetElement(Piece.Crystal.Type) == ESpellElement::Water);
				TestTrue(TEXT("tier inside the zone"), Piece.Tier >= Input.ZoneRange.Min && Piece.Tier <= Input.ZoneRange.Max);
			}
			TestFalse(TEXT("no Fire spell"), Kit.Spells.Contains(FireSpell));
		}
	});

	It("rolls nothing without a pool, so the fight keeps the authored inventories", [this]()
	{
		Input.Pool.Empty();
		const TArray<UCharacterData *> Team = {Enemy, Enemy};
		const TArray<FRolledEnemyKit> Kits = EnemyKitRoll::RollTeam(Team, Input, RUN_SEED, TEXT("A"));
		TestEqual(TEXT("one per member"), Kits.Num(), 2);
		TestFalse(TEXT("no gear"), Kits[0].HasGear() || Kits[1].HasGear());
	});
}

#endif
