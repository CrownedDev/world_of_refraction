// CharacterFormSkinSpec.cpp
// B-279b: skins. Only an unlocked form is worn, never in a fight, only in the hub or at a rest spot;
// bound gear gives its bonus in its owner's skin; the worn skin survives save bytes; an unlock reveals
// the full loadout.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Engine/GameInstance.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryData.h"
#include "Kismet/GameplayStatics.h"
#include "Save/WoRSaveGame.h"
#include "Skills/Effects/EffectDefinition.h"

namespace
{
	constexpr int32 FORM_SKIN_TEST_BONUS_PERCENT = 10;
	constexpr int32 FORM_SKIN_REFERENCE_WEAPONS = 2;

	UEffectDefinition *FormSkinBoundBonus()
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DamageBuff;
		Payload.Value = FORM_SKIN_TEST_BONUS_PERCENT;
		Payload.Target = ETargetType::Self;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		Definition->Effects.Add(Effect);
		return Definition;
	}
}

BEGIN_DEFINE_SPEC(FCharacterFormSkinSpec, "WoR.Character.FormSkins",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UCharacterFormData *Form = nullptr;
	UGameInstance *GameInstance = nullptr;
	UCharacterFormSubsystem *Forms = nullptr;

	void Unlock(UCharacterFormSubsystem *InForms) const
	{
		InForms->RestoreProgress({{Form->GetFName(), CharacterFormMeter::FULL}});
	}
END_DEFINE_SPEC(FCharacterFormSkinSpec)

void FCharacterFormSkinSpec::Define()
{
	BeforeEach([this]()
	{
		UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
		for (int32 Index = 0; Index < FORM_SKIN_REFERENCE_WEAPONS; ++Index)
		{
			Inventory->Weapons.Add(NewObject<UWeaponData>(GetTransientPackage()));
		}
		UCharacterData *Character = NewObject<UCharacterData>(GetTransientPackage());
		Character->Inventory = Inventory;
		Form = NewObject<UCharacterFormData>(GetTransientPackage());
		Form->Character = Character;

		GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		Forms = NewObject<UCharacterFormSubsystem>(GameInstance);
		Forms->RegisterForm(Form);
	});

	It("switches only to an unlocked form, never in a fight, only in the hub or at a rest spot", [this]()
	{
		TestEqual(TEXT("locked"), CharacterFormSwitch::CanSwitch(false, false, true), EFormSwitchRefusal::Locked);
		TestEqual(TEXT("in a fight"), CharacterFormSwitch::CanSwitch(true, true, true), EFormSwitchRefusal::InFight);
		TestEqual(TEXT("away from a switch spot"), CharacterFormSwitch::CanSwitch(true, false, false), EFormSwitchRefusal::NotAtSwitchSpot);
		TestEqual(TEXT("allowed"), CharacterFormSwitch::CanSwitch(true, false, true), EFormSwitchRefusal::None);
		TestFalse(TEXT("every refusal has words"), CharacterFormSwitch::GetRefusalText(EFormSwitchRefusal::InFight).IsEmpty());
	});

	It("wears an unlocked form at a rest spot and refuses a locked one", [this]()
	{
		Forms->SetAtRestSpot(true);
		TestEqual(TEXT("locked form refused"), Forms->TryWearForm(Form), EFormSwitchRefusal::Locked);
		TestNull(TEXT("nothing worn"), Forms->GetWornForm());

		Unlock(Forms);
		Forms->SetAtRestSpot(false);
		TestEqual(TEXT("away from the hub and rest spots"), Forms->TryWearForm(Form), EFormSwitchRefusal::NotAtSwitchSpot);
		TestNull(TEXT("still nothing worn"), Forms->GetWornForm());

		Forms->SetAtRestSpot(true);
		TestEqual(TEXT("worn at a rest spot"), Forms->TryWearForm(Form), EFormSwitchRefusal::None);
		TestEqual(TEXT("the skin"), Forms->GetWornForm(), Form);
		TestEqual(TEXT("taken off"), Forms->TryWearForm(nullptr), EFormSwitchRefusal::None);
		TestNull(TEXT("back to the base form"), Forms->GetWornForm());
		TestTrue(TEXT("Quinn picked"), Forms->TrySetBaseForm(EPlayerBaseForm::Quinn));
		TestEqual(TEXT("base body"), Forms->GetBaseForm(), EPlayerBaseForm::Quinn);
	});

	It("gives bound gear its bonus in its owner's skin only (D-033)", [this]()
	{
		UCharacterData *Wearer = NewObject<UCharacterData>(GetTransientPackage());
		UCharacterData *Stranger = NewObject<UCharacterData>(GetTransientPackage());
		UWeaponData *Bound = NewObject<UWeaponData>(GetTransientPackage());
		Bound->BoundOwner = Form->Character;
		Bound->BoundEffects.Add(FormSkinBoundBonus());

		TestFalse(TEXT("no bonus without the skin"), Bound->IsBoundBonusActive(Wearer));
		TestFalse(TEXT("no bonus in another skin"), Bound->IsBoundBonusActive(Wearer, Stranger));
		TestTrue(TEXT("the bonus in the owner's skin"), Bound->IsBoundBonusActive(Wearer, Form->Character));
		TestEqual(TEXT("no bound effect gathered without the skin"), Bound->GetStartingEffectsGathered(Wearer).Num(), 0);
		TestTrue(TEXT("bound effect gathered in the skin"), Bound->GetStartingEffectsGathered(Wearer, Form->Character).Num() > 0);
		TestNull(TEXT("no skin character without a party"), Forms->GetSkinCharacterFor(Wearer));
	});

	It("keeps the worn skin and base body through save bytes; a locked skin loads as none", [this]()
	{
		Unlock(Forms);
		Forms->SetAtRestSpot(true);
		Forms->TryWearForm(Form);
		Forms->TrySetBaseForm(EPlayerBaseForm::Quinn);
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Forms.Progress = Forms->GetProgressByName();
		Save->Data.Forms.WornForm = Forms->GetWornFormName();
		Save->Data.Forms.BaseForm = static_cast<uint8>(Forms->GetBaseForm());
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("saved to bytes"), UGameplayStatics::SaveGameToMemory(Save, Bytes)))
		{
			return;
		}
		const UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!TestNotNull(TEXT("loaded"), Loaded))
		{
			return;
		}
		UCharacterFormSubsystem *NewForms = NewObject<UCharacterFormSubsystem>(GameInstance);
		NewForms->RegisterForm(Form);
		NewForms->RestoreProgress(Loaded->Data.Forms.Progress);
		NewForms->RestoreWorn(Loaded->Data.Forms.WornForm, static_cast<EPlayerBaseForm>(Loaded->Data.Forms.BaseForm));
		TestEqual(TEXT("skin restored"), NewForms->GetWornForm(), Form);
		TestEqual(TEXT("base restored"), NewForms->GetBaseForm(), EPlayerBaseForm::Quinn);

		UCharacterFormSubsystem *LockedForms = NewObject<UCharacterFormSubsystem>(GameInstance);
		LockedForms->RegisterForm(Form);
		LockedForms->RestoreWorn(Loaded->Data.Forms.WornForm, EPlayerBaseForm::Manny);
		TestNull(TEXT("a locked skin isn't worn"), LockedForms->GetWornForm());
	});

	It("reveals one line per reference piece", [this]()
	{
		TArray<FString> Lines;
		Form->GetRevealedLoadoutText().ToString().ParseIntoArrayLines(Lines);
		TestEqual(TEXT("lines"), Lines.Num(), FORM_SKIN_REFERENCE_WEAPONS);
	});

	AfterEach([this]()
	{
		Form = nullptr;
		Forms = nullptr;
		GameInstance = nullptr;
	});
}

#endif
