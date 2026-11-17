// EnemyKitSpec.cpp
// B-567 (D-225, TrialPoolDesign stage 3): an enemy's always-has list and its six limits, headless. Unproven here: the
// authored kits on the real enemies (PIE sheet B-567, Print Enemy Kit).

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/CharacterData.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Misc/AutomationTest.h"
#include "Trial/EnemyKit.h"
#include "Trial/TrialPool.h"

BEGIN_DEFINE_SPEC(FEnemyKitSpec, "WoR.Trial.EnemyKit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	static FEnemyKitPiece MakePiece(UObject *Item, EItemTier Tier)
	{
		FEnemyKitPiece Piece;
		Piece.Item = Item;
		Piece.Tier = Tier;
		return Piece;
	}
END_DEFINE_SPEC(FEnemyKitSpec)

void FEnemyKitSpec::Define()
{
	It("leaves everything to the class and the zone when nothing is set, except skills: 3", [this]()
	{
		const FEnemyKit Kit;
		TestEqual(TEXT("a Generic: 2 weapons"), EnemyKitRules::GetMostWeapons(Kit, ECharacterClass::Generic), 2);
		TestEqual(TEXT("a Generic: 1 adapter"), EnemyKitRules::GetMostAdapters(Kit, ECharacterClass::Generic), 1);
		TestEqual(TEXT("a Resonator: 5 adapters"), EnemyKitRules::GetMostAdapters(Kit, ECharacterClass::Resonator), 5);
		TestEqual(TEXT("skills: 3"), EnemyKitRules::GetMostSkills(Kit, 16), EnemyKitRules::DEFAULT_MOST_SKILLS);
		TestTrue(TEXT("any weapon type"), EnemyKitRules::AllowsWeaponType(Kit, EWeaponType::Axe));
		TestTrue(TEXT("any element"), EnemyKitRules::AllowsElement(Kit, ESpellElement::Water));
		const FTrialTierRange Zone = TrialPool::GetZoneTierRange(2);
		const FTrialTierRange Range = EnemyKitRules::GetTierRange(Kit, Zone);
		TestTrue(TEXT("the zone's tiers"), Range.Min == Zone.Min && Range.Max == Zone.Max);
	});

	It("narrows with its own limits, never past the class or the slots", [this]()
	{
		FEnemyKit Kit;
		Kit.bLimitWeapons = true;
		Kit.MostWeapons = 1;
		TestEqual(TEXT("most weapons 1"), EnemyKitRules::GetMostWeapons(Kit, ECharacterClass::Generic), 1);
		Kit.MostWeapons = 9;
		TestEqual(TEXT("the class still caps it"), EnemyKitRules::GetMostWeapons(Kit, ECharacterClass::Generic), 2);
		Kit.bLimitSkills = true;
		Kit.MostSkills = 6;
		TestEqual(TEXT("six skills with 16 slots"), EnemyKitRules::GetMostSkills(Kit, 16), 6);
		TestEqual(TEXT("never more than its slots"), EnemyKitRules::GetMostSkills(Kit, 4), 4);
		Kit.WeaponTypes = {EWeaponType::Sword};
		TestFalse(TEXT("only swords"), EnemyKitRules::AllowsWeaponType(Kit, EWeaponType::Axe));
		Kit.bLimitTiers = true;
		Kit.MinTier = EItemTier::D_Tier;
		Kit.MaxTier = EItemTier::S_Tier;
		const FTrialTierRange Range = EnemyKitRules::GetTierRange(Kit, TrialPool::GetZoneTierRange(4));
		TestTrue(TEXT("D to A inside zone 4"), Range.Min == EItemTier::D_Tier && Range.Max == EItemTier::A_Tier);
		const FTrialTierRange None = EnemyKitRules::GetTierRange(Kit, TrialPool::GetZoneTierRange(0));
		TestTrue(TEXT("no meeting: nothing to take"), None.Min > None.Max);
	});

	It("checks the always-has list against the class", [this]()
	{
		UWeaponData *Sword = NewObject<UWeaponData>();
		URingData *Ring = NewObject<URingData>();
		FEnemyKit Kit;
		Kit.Pieces = {MakePiece(Sword, EItemTier::D_Tier), MakePiece(Ring, EItemTier::F_Tier)};
		TestEqual(TEXT("a Generic: a sword and a ring fit"), EnemyKitRules::CheckAlwaysHas(Kit, ECharacterClass::Generic).Num(), 0);
		TestTrue(TEXT("a Refractor carries one item"), EnemyKitRules::CheckAlwaysHas(Kit, ECharacterClass::Caster).Num() > 0);
		Kit.Pieces.Add(MakePiece(Sword, EItemTier::F_Tier));
		Kit.Pieces.Add(MakePiece(Sword, EItemTier::F_Tier));
		TestTrue(TEXT("a Generic: three weapons are too many"), EnemyKitRules::CheckAlwaysHas(Kit, ECharacterClass::Generic).Num() > 0);
		Kit.Pieces = {MakePiece(nullptr, EItemTier::F_Tier)};
		TestTrue(TEXT("an empty piece"), EnemyKitRules::CheckAlwaysHas(Kit, ECharacterClass::Generic).Num() > 0);
		Kit.Pieces = {MakePiece(Sword, EItemTier::D_Tier)};
		Kit.bLimitWeapons = true;
		Kit.MostWeapons = 0;
		TestTrue(TEXT("an always-has sword past its own weapon limit"), EnemyKitRules::CheckAlwaysHas(Kit, ECharacterClass::Generic).Num() > 0);
	});

	It("warns when a given tier is outside the zone's range, and keeps it", [this]()
	{
		UCharacterData *Enemy = NewObject<UCharacterData>();
		Enemy->EnemyKit.Pieces = {MakePiece(NewObject<UWeaponData>(), EItemTier::D_Tier)};
		FEncounterZoneInfo Zone;
		Zone.Tier = 0; // F to E
		TArray<UCharacterData *> Roster = {Enemy};
		TestFalse(TEXT("zone 0: warned"), EnemyKitRules::DescribeOutsideZoneRange(TEXT("Bandits"), Zone, Roster).IsEmpty());
		Zone.Tier = 1; // F to D
		TestTrue(TEXT("zone 1: fits"), EnemyKitRules::DescribeOutsideZoneRange(TEXT("Bandits"), Zone, Roster).IsEmpty());
		TestTrue(TEXT("the tier is untouched"), Enemy->EnemyKit.Pieces[0].Tier == EItemTier::D_Tier);
	});
}

#endif
