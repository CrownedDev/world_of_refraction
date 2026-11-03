// CharacterFormSpec.cpp
// B-279: the loadout-match meter. Wearing part of a form's reference loadout fills it slower than the
// full set; 100% unlocks the form; restored progress reads back.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Engine/GameInstance.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

BEGIN_DEFINE_SPEC(FCharacterFormSpec, "WoR.Character.Forms",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UCharacterFormData *Form = nullptr;
	UGameInstance *GameInstance = nullptr;
	UCharacterFormSubsystem *Forms = nullptr;
	/** The form's six reference pieces: two weapons, two rings (A tier), a spell and an ability. */
	TArray<FCharacterFormPiece> Reference;

	TArray<FCharacterFormPiece> FirstPieces(int32 Count) const;
END_DEFINE_SPEC(FCharacterFormSpec)

TArray<FCharacterFormPiece> FCharacterFormSpec::FirstPieces(int32 Count) const
{
	TArray<FCharacterFormPiece> Pieces;
	for (int32 Index = 0; Index < Count && Index < Reference.Num(); ++Index)
	{
		Pieces.Add(Reference[Index]);
	}
	return Pieces;
}

void FCharacterFormSpec::Define()
{
	BeforeEach([this]()
	{
		UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
			Weapon->Tier = EItemTier::A_Tier;
			Inventory->Weapons.Add(Weapon);
			URingData *Ring = NewObject<URingData>(GetTransientPackage());
			Ring->Tier = EItemTier::A_Tier;
			Inventory->Rings.Add(Ring);
		}
		Inventory->Spells.Add(NewObject<USpellData>(GetTransientPackage()));
		Inventory->Abilities.Add(NewObject<UAbilityData>(GetTransientPackage()));

		UCharacterData *Character = NewObject<UCharacterData>(GetTransientPackage());
		Character->Inventory = Inventory;
		Form = NewObject<UCharacterFormData>(GetTransientPackage());
		Form->Character = Character;
		Reference = Form->GetReferencePieces();

		GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		Forms = NewObject<UCharacterFormSubsystem>(GameInstance);
		Forms->RegisterForm(Form);
	});

	It("reads six reference pieces from the character's inventory", [this]()
	{
		TestEqual(TEXT("pieces"), Reference.Num(), 6);
	});

	It("fills a fight's full share only for the whole set at its tiers", [this]()
	{
		TestEqual(TEXT("full set"), CharacterFormMeter::GetFightFill(Reference, Reference), CharacterFormMeter::FULL_SET_FILL_PER_FIGHT);
		TestEqual(TEXT("half the set"), CharacterFormMeter::GetFightFill(Reference, FirstPieces(3)),
				  CharacterFormMeter::FULL_SET_FILL_PER_FIGHT / 2.0f);
		TestEqual(TEXT("nothing worn"), CharacterFormMeter::GetFightFill(Reference, {}), 0.0f);

		TArray<FCharacterFormPiece> OtherTier = Reference;
		OtherTier[0].Tier = EItemTier::F_Tier;
		const float Expected = CharacterFormMeter::FULL_SET_FILL_PER_FIGHT * (5.0f + CharacterFormMeter::OTHER_TIER_CREDIT) / 6.0f;
		TestEqual(TEXT("one weapon at another tier"), CharacterFormMeter::GetFightFill(Reference, OtherTier), Expected);
	});

	It("fills slower wearing three of six pieces than wearing them all", [this]()
	{
		Forms->RecordFightWonWith({FirstPieces(3)});
		const float Partial = Forms->GetProgress(Form);
		Forms->RestoreProgress({});
		Forms->RecordFightWonWith({Reference});
		const float Full = Forms->GetProgress(Form);
		TestTrue(TEXT("partial set fills"), Partial > 0.0f);
		TestTrue(FString::Printf(TEXT("partial %.3f < full %.3f"), Partial, Full), Partial < Full);
	});

	It("credits the best-matching fighter, not the sum", [this]()
	{
		Forms->RecordFightWonWith({FirstPieces(3), Reference, FirstPieces(1)});
		TestEqual(TEXT("progress"), Forms->GetProgress(Form), CharacterFormMeter::FULL_SET_FILL_PER_FIGHT);
	});

	It("unlocks at 100% and stops there", [this]()
	{
		const int32 FightsToUnlock = FMath::CeilToInt(CharacterFormMeter::FULL / CharacterFormMeter::FULL_SET_FILL_PER_FIGHT);
		for (int32 Fight = 0; Fight < FightsToUnlock - 1; ++Fight)
		{
			Forms->RecordFightWonWith({Reference});
		}
		TestFalse(TEXT("locked one fight short"), Forms->IsUnlocked(Form));
		Forms->RecordFightWonWith({Reference});
		Forms->RecordFightWonWith({Reference});
		TestTrue(TEXT("unlocked"), Forms->IsUnlocked(Form));
		TestEqual(TEXT("capped"), Forms->GetProgress(Form), CharacterFormMeter::FULL);
	});

	It("reads back restored progress", [this]()
	{
		Forms->RecordFightWonWith({FirstPieces(3)});
		const TMap<FName, float> Saved = Forms->GetProgressByName();
		UCharacterFormSubsystem *Loaded = NewObject<UCharacterFormSubsystem>(GameInstance);
		Loaded->RegisterForm(Form);
		Loaded->RestoreProgress(Saved);
		TestEqual(TEXT("progress"), Loaded->GetProgress(Form), Forms->GetProgress(Form));
	});

	AfterEach([this]()
	{
		Form = nullptr;
		Forms = nullptr;
		GameInstance = nullptr;
		Reference.Empty();
	});
}

#endif
