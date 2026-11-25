// GameNamesSpec.cpp
// B-126: every element and status effect has a cooked-safe player-facing name.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRGameNames.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Grid/ECombatRow.h"
#include "Currency/CurrencyTypes.h"
#include "Field/FieldSkillTypes.h"
#include "Infusion/EInfusionSourceOption.h"
#include "Inventory/InventoryVerbs.h"
#include "Merchant/MerchantData.h"
#include "Skills/Definitions/SpellSchool.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"

BEGIN_DEFINE_SPEC(FGameNamesSpec, "WoR.Text.GameNames",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	/** Each value of TEnum has a non-empty name that isn't its raw identifier (a single-word identifier
	 *  such as "Fire" may be its own name). */
	template <typename TEnum>
	void ExpectEveryValueNamed(TFunctionRef<FText(TEnum)> GetName);
END_DEFINE_SPEC(FGameNamesSpec)

template <typename TEnum>
void FGameNamesSpec::ExpectEveryValueNamed(TFunctionRef<FText(TEnum)> GetName)
{
	const UEnum *Enum = StaticEnum<TEnum>();
	if (!TestNotNull(TEXT("Enum"), Enum))
	{
		return;
	}
	// The last entry is the generated _MAX.
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
	{
		const FString Identifier = Enum->GetNameStringByIndex(Index);
		const FString Name = GetName(static_cast<TEnum>(Enum->GetValueByIndex(Index))).ToString();
		TestFalse(FString::Printf(TEXT("%s has a name"), *Identifier), Name.IsEmpty());
		const bool bSingleWord = FName::NameToDisplayString(Identifier, false) == Identifier;
		TestTrue(FString::Printf(TEXT("%s's name '%s' is not its identifier"), *Identifier, *Name),
				 bSingleWord || Name != Identifier);
	}
}

void FGameNamesSpec::Define()
{
	It("names every element", [this]()
	{
		ExpectEveryValueNamed<ESpellElement>(&WoRGameNames::GetElementName);
	});

	It("names every infusion source by its piece (B-634)", [this]()
	{
		ExpectEveryValueNamed<EInfusionSourceOption>(&WoRGameNames::GetInfusionSourceWord);
		TestEqual(TEXT("the ring is the Adapter"), WoRGameNames::GetInfusionSourceWord(EInfusionSourceOption::PrimaryRing).ToString(), FString(TEXT("Adapter")));
	});

	It("names every status effect type", [this]()
	{
		ExpectEveryValueNamed<ESkillEffectType>(&WoRGameNames::GetSkillEffectTypeName);
	});

	It("names every character class", [this]()
	{
		ExpectEveryValueNamed<ECharacterClass>(&WoRGameNames::GetCharacterClassName);
	});

	It("names every defense, row, spell school and merchant type (B-207)", [this]()
	{
		ExpectEveryValueNamed<EDefenseType>(&WoRGameNames::GetDefenseTypeName);
		ExpectEveryValueNamed<ECombatRow>(&WoRGameNames::GetCombatRowName);
		ExpectEveryValueNamed<EFieldMovementType>(&WoRGameNames::GetFieldMovementName); // B-458
		ExpectEveryValueNamed<ESpellSchool>(&WoRGameNames::GetSpellSchoolName);
		ExpectEveryValueNamed<EMerchantType>(&WoRGameNames::GetMerchantTypeName);
		TestEqual(TEXT("Block reads Guard"), WoRGameNames::GetDefenseTypeName(EDefenseType::Block).ToString(), FString(TEXT("Guard")));
		TestEqual(TEXT("Legacy Parry reads Guard"), WoRGameNames::GetDefenseTypeName(EDefenseType::Parry).ToString(),
				  FString(TEXT("Guard")));
	});

	It("shows the run currency as Prisms and the hub currency as Gold (B-366, D-159)", [this]()
	{
		TestEqual(TEXT("run (ECurrencyType::Gold)"), WoRGameNames::GetCurrencyName(ECurrencyType::Gold).ToString(), FString(TEXT("Prisms")));
		TestEqual(TEXT("hub (ECurrencyType::Prisms)"), WoRGameNames::GetCurrencyName(ECurrencyType::Prisms).ToString(), FString(TEXT("Gold")));
		TestEqual(TEXT("the convert verb pays the run currency"), WoRGameNames::GetInventoryVerbName(EInventoryVerb::Convert).ToString(),
				  FString(TEXT("Convert to Prisms")));
	});

	It("names every essence: Generic as Quartz, Reality as Unknown (B-269, D-105, D-095)", [this]()
	{
		ExpectEveryValueNamed<EEssenceType>(&WoRGameNames::GetEssenceName);
		TestEqual(TEXT("Generic"), WoRGameNames::GetEssenceName(EEssenceType::Generic).ToString(), FString(TEXT("Quartz Essence")));
		TestEqual(TEXT("Reality"), WoRGameNames::GetEssenceName(EEssenceType::Reality).ToString(), FString(TEXT("Unknown Essence")));
	});

	It("names every balance: the seven of D-125 (B-333)", [this]()
	{
		ExpectEveryValueNamed<ECurrencyType>(&WoRGameNames::GetCurrencyName);
		TestEqual(TEXT("Elemental"), WoRGameNames::GetCurrencyName(ECurrencyType::ElementalEssence).ToString(),
				  FString(TEXT("Elemental Essence")));
		TestEqual(TEXT("Stat (D-235, was Pillar)"), WoRGameNames::GetCurrencyName(ECurrencyType::PillarEssence).ToString(),
				  FString(TEXT("Stat Essence")));
		TestEqual(TEXT("Mentor (B-361b)"), WoRGameNames::GetCurrencyName(ECurrencyType::MentorTokens).ToString(),
				  FString(TEXT("Mentor Tokens")));
	});

	It("shows Reality as Unknown (D-095)", [this]()
	{
		TestEqual(TEXT("Element"), WoRGameNames::GetElementName(ESpellElement::Reality).ToString(), FString(TEXT("Unknown")));
		TestFalse(TEXT("Immunity"),
				  WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::GrantRealityImmunity).ToString().Contains(TEXT("Reality")));
	});

	It("uses the player-facing word, not the code name", [this]()
	{
		TestEqual(TEXT("DOT"), WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::DOT).ToString(),
				  FString(TEXT("DoT")));
		TestEqual(TEXT("SkipTurn"), WoRGameNames::GetSkillEffectTypeName(ESkillEffectType::SkipTurn).ToString(),
				  FString(TEXT("Skip Turn")));
		TestEqual(TEXT("Caster"), WoRGameNames::GetCharacterClassName(ECharacterClass::Caster).ToString(), FString(TEXT("Refractor")));
	});

	It("names the weapon in hand by how it is held (B-513, Q-513)", [this]()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponType = EWeaponType::Sword;
		TestEqual(TEXT("one sword"), WoRGameNames::GetHeldWeaponTypeName(Weapon).ToString(), FString(TEXT("Sword")));
		UWeaponData *SecondSword = NewObject<UWeaponData>(GetTransientPackage());
		SecondSword->WeaponType = EWeaponType::Sword;
		TestEqual(TEXT("a sword paired with a sword"), WoRGameNames::GetHeldWeaponTypeName(Weapon, SecondSword).ToString(), FString(TEXT("Dual Swords")));
		UWeaponData *Shield = NewObject<UWeaponData>(GetTransientPackage());
		Shield->WeaponType = EWeaponType::Shield;
		TestEqual(TEXT("a sword with a shield piece (B-515)"), WoRGameNames::GetHeldWeaponTypeName(Weapon, Shield).ToString(), FString(TEXT("Sword and Shield")));
		Weapon->WieldMode = EWeaponWieldMode::OffHandShield;
		TestEqual(TEXT("with its shield"), WoRGameNames::GetHeldWeaponTypeName(Weapon).ToString(), FString(TEXT("Sword and Shield")));
		Weapon->WeaponType = EWeaponType::Axe;
		Weapon->WieldMode = EWeaponWieldMode::Dual;
		TestEqual(TEXT("a dual pair"), WoRGameNames::GetHeldWeaponTypeName(Weapon).ToString(), FString(TEXT("Dual Axes")));
		Weapon->WeaponType = EWeaponType::Gauntlets;
		TestEqual(TEXT("gauntlets are a pair already"), WoRGameNames::GetHeldWeaponTypeName(Weapon).ToString(), WoRGameNames::GetWeaponTypeName(EWeaponType::Gauntlets).ToString());
		TestTrue(TEXT("no weapon"), WoRGameNames::GetHeldWeaponTypeName(nullptr).IsEmpty());
	});

	It("names the pieces in hand, joining a separate off hand (B-513b)", [this]()
	{
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Flame Sword");
		UWeaponData *Shield = NewObject<UWeaponData>(GetTransientPackage());
		Shield->Name = TEXT("Void Shield");
		UWeaponData *Unnamed = NewObject<UWeaponData>(GetTransientPackage());
		Unnamed->Name = FString();
		Unnamed->WeaponType = EWeaponType::Axe;
		TestEqual(TEXT("one piece: its name"), WoRGameNames::GetHeldPiecesName(Sword).ToString(), FString(TEXT("Flame Sword")));
		TestEqual(TEXT("with its off hand"), WoRGameNames::GetHeldPiecesName(Sword, Shield).ToString(), FString(TEXT("Flame Sword + Void Shield")));
		TestEqual(TEXT("a piece with no name gives its type"), WoRGameNames::GetHeldPiecesName(Sword, Unnamed).ToString(),
				  FString::Printf(TEXT("Flame Sword + %s"), *WoRGameNames::GetWeaponTypeName(EWeaponType::Axe).ToString()));
		TestTrue(TEXT("no main hand"), WoRGameNames::GetHeldPiecesName(nullptr, Shield).IsEmpty());
	});

	It("names an adapter by what is in its socket (B-514b, D-223)", [this]()
	{
		URingData *Ring = NewObject<URingData>(GetTransientPackage());
		Ring->Form = EAdapterForm::Ring;
		const FString Form = WoRGameNames::GetAdapterFormName(EAdapterForm::Ring).ToString();
		Ring->Name = FString::Printf(TEXT("%s %s"), *WoRGameNames::GetAdapterWord().ToString(), *Form); // the empty adapter's own reading: not a name of its own
		FRuntimeAttachedItem Socket;
		TestEqual(TEXT("empty: the adapter word, then the form (D-223)"), WoRGameNames::GetAdapterName(Ring, Socket).ToString(),
				  FString::Printf(TEXT("%s %s"), *WoRGameNames::GetAdapterWord().ToString(), *Form));
		constexpr int32 LIVE_DURABILITY = 10;
		Socket.Kind = EAttachedItemKind::Crystal;
		Socket.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), LIVE_DURABILITY);
		TestEqual(TEXT("a crystal: its element, then the form"), WoRGameNames::GetAdapterName(Ring, Socket).ToString(),
				  FString::Printf(TEXT("%s %s"), *WoRGameNames::GetElementName(ESpellElement::Fire).ToString(), *Form));
		UEvolutionItemData *Phoenix = NewObject<UEvolutionItemData>(GetTransientPackage());
		Phoenix->ItemName = TEXT("Phoenix");
		FRuntimeAttachedItem Evolved;
		Evolved.Kind = EAttachedItemKind::Evolution;
		Evolved.Evolution.Item = Phoenix;
		TestEqual(TEXT("an evolution: its name, then the form"), WoRGameNames::GetAdapterName(Ring, Evolved).ToString(), FString::Printf(TEXT("Phoenix %s"), *Form));
		TestTrue(TEXT("no adapter"), WoRGameNames::GetAdapterName(nullptr, Socket).IsEmpty());
		Ring->Name = GetDefault<URingData>()->Name;
		TestEqual(TEXT("the class default name is not its own: by its socket"), WoRGameNames::GetAdapterName(Ring, Evolved).ToString(),
				  FString::Printf(TEXT("Phoenix %s"), *Form));
	});

	It("shows an adapter's own name whatever is in its socket (D-226)", [this]()
	{
		URingData *Ring = NewObject<URingData>(GetTransientPackage());
		Ring->Form = EAdapterForm::Ring;
		Ring->Name = TEXT("King's Ring");
		constexpr int32 LIVE_DURABILITY = 10;
		FRuntimeAttachedItem Socket;
		TestEqual(TEXT("empty"), WoRGameNames::GetAdapterName(Ring, Socket).ToString(), FString(TEXT("King's Ring")));
		Socket.Kind = EAttachedItemKind::Crystal;
		Socket.Crystal = FCrystalAttachment(FCrystalId(ECrystalType::Garnet, EItemTier::F_Tier), LIVE_DURABILITY);
		TestEqual(TEXT("with a crystal"), WoRGameNames::GetAdapterName(Ring, Socket).ToString(), FString(TEXT("King's Ring")));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
