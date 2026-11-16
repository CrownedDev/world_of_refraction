// FormsScreenModel.cpp

#include "UI/Hub/FormsScreenModel.h"

#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "UI/Menus/LoadoutScreenModel.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "FormsScreen"

namespace FormsScreenBuild
{
	FText PieceName(const UObject *Asset)
	{
		if (const UEquipmentDataBase *Gear = Cast<UEquipmentDataBase>(Asset))
		{
			return FText::FromString(Gear->GetDisplayName());
		}
		if (const USkillDataBase *Skill = Cast<USkillDataBase>(Asset))
		{
			return FText::FromString(Skill->Name);
		}
		return FText::FromString(GetNameSafe(Asset));
	}

	UCharacterFormSubsystem *GetForms(const UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
	}
}

FFormsScreenPage WoRFormsScreen::BuildPage(const UCharacterFormSubsystem &Forms, const FText &CharacterName, TFunctionRef<bool(const UObject *)> Owns,
										   TFunctionRef<bool(const UObject *)> Wears)
{
	FFormsScreenPage Page;
	Page.Title = FText::Format(LOCTEXT("Title", "Spirit of {0}"), CharacterName);
	const UCharacterFormData *Worn = Forms.GetWornForm();
	const FText Base = WoRGameNames::GetBaseFormName(Forms.GetBaseForm());
	Page.WearingLine = Worn ? FText::Format(LOCTEXT("WearingForm", "Wearing: {0} (over {1})"), Worn->FormName, Base)
							: FText::Format(LOCTEXT("Wearing", "Wearing: {0}"), Base);

	// D-234: your own spirits first, Male Nobody then Female Nobody.
	for (const EPlayerBaseForm Body : {EPlayerBaseForm::Manny, EPlayerBaseForm::Quinn})
	{
		FFormsScreenRow &Own = Page.Rows.AddDefaulted_GetRef();
		Own.bBaseBody = true;
		Own.BaseBody = Body;
		Own.Name = WoRGameNames::GetBaseFormName(Body);
		Own.Line = LOCTEXT("AlwaysYours", "always yours");
		Own.Progress = 1.0f;
		Own.bUnlocked = true;
		Own.bWorn = !Worn && Forms.GetBaseForm() == Body;
		Own.Status = LOCTEXT("OwnStatus", "Your own spirit. A wears it (and takes a form off).");
	}

	for (UCharacterFormData *Form : Forms.GetForms())
	{
		if (!Form)
		{
			continue;
		}
		FFormsScreenRow &Row = Page.Rows.AddDefaulted_GetRef();
		Row.Form = Form;
		Row.Progress = Forms.GetProgress(Form);
		Row.bUnlocked = Forms.IsUnlocked(Form);
		Row.bWorn = Worn == Form;
		bool bAnyKnown = false;
		for (const FCharacterFormPiece &Reference : Form->GetReferencePieces())
		{
			FFormsScreenPiece &Piece = Row.Pieces.AddDefaulted_GetRef();
			Piece.bOwned = Owns(Reference.Asset);
			Piece.bWorn = Wears(Reference.Asset);
			Piece.bKnown = Row.bUnlocked || Piece.bOwned || Piece.bWorn; // the unlock reveals the whole loadout
			Piece.Name = Piece.bKnown ? FormsScreenBuild::PieceName(Reference.Asset) : LOCTEXT("UnknownPiece", "?");
			bAnyKnown |= Piece.bOwned || Piece.bWorn;
		}
		// Q-444: met once their meter has moved or you own one of their pieces.
		Row.bMet = Row.bUnlocked || Row.Progress > 0.0f || bAnyKnown;
		Row.Name = Row.bMet ? Form->FormName : LOCTEXT("Unmet", "???");
		Row.Status = Row.bUnlocked ? LOCTEXT("Unlocked", "Unlocked. A wears it; its bound gear works while you do.")
									: LOCTEXT("Locked", "Locked. Wear their pieces in fights you win to fill the meter.");
		if (!Row.bMet)
		{
			Row.Pieces.Reset();
			Row.Status = LOCTEXT("NotMet", "You haven't met them yet.");
		}
	}
	return Page;
}

FFormsScreenPage WoRFormsScreen::BuildPage(UWorld *World)
{
	const UCharacterFormSubsystem *Forms = FormsScreenBuild::GetForms(World);
	const TArray<FLoadoutScreenMember> Members = WoRLoadoutScreen::BuildMembers(World);
	if (!Forms)
	{
		FFormsScreenPage Page;
		Page.Title = LOCTEXT("NoForms", "Spirit");
		return Page;
	}
	const FLoadoutScreenMember *Player = Members.IsEmpty() ? nullptr : &Members[0];
	UInventoryComponent *Inventory = Player ? WoRGive::FindMemberInventory(World, Player->Key) : nullptr;
	TArray<FCharacterFormPiece> Equipped;
	if (Inventory && Inventory->GetOwner())
	{
		Equipped = UCharacterFormSubsystem::GatherEquippedPieces(*Inventory->GetOwner());
	}
	const auto Owns = [Inventory](const UObject *Asset)
	{
		if (!Inventory)
		{
			return false;
		}
		if (const UWeaponData *Weapon = Cast<UWeaponData>(Asset))
		{
			return Inventory->OwnsWeaponAsset(Weapon);
		}
		if (const URingData *Ring = Cast<URingData>(Asset))
		{
			return Inventory->OwnsRingAsset(Ring);
		}
		// HasAbility / HasSpell take a non-const pointer but only compare it.
		if (const UAbilityData *Ability = Cast<UAbilityData>(Asset))
		{
			return Inventory->Abilities.HasAbility(const_cast<UAbilityData *>(Ability));
		}
		if (const USpellData *Spell = Cast<USpellData>(Asset))
		{
			return Inventory->Spells.HasSpell(const_cast<USpellData *>(Spell));
		}
		return false; // TODO: B-444, owned evolutions and apparel once the reference loadout lists them
	};
	const auto Wears = [&Equipped](const UObject *Asset)
	{
		return Equipped.ContainsByPredicate([Asset](const FCharacterFormPiece &Piece) { return Piece.Asset == Asset; });
	};
	return BuildPage(*Forms, Player ? Player->Name : LOCTEXT("You", "you"), Owns, Wears);
}

FText WoRFormsScreen::Wear(UWorld *World, const FFormsScreenRow &Row)
{
	UCharacterFormSubsystem *Forms = FormsScreenBuild::GetForms(World);
	if (!Forms)
	{
		return LOCTEXT("NoFormSystem", "Forms aren't available here.");
	}
	if (!Row.bMet)
	{
		return LOCTEXT("WearUnmet", "You haven't met them yet.");
	}
	const EFormSwitchRefusal Refusal = Forms->TryWearForm(Row.Form);
	UE_LOG(LogWoRUI, Log, TEXT("[FormsScreen] wear %s: %s"), Row.bBaseBody ? *UEnum::GetValueAsString(Row.BaseBody) : *GetNameSafe(Row.Form),
		   *UEnum::GetValueAsString(Refusal));
	if (Refusal != EFormSwitchRefusal::None)
	{
		return CharacterFormSwitch::GetRefusalText(Refusal);
	}
	return !Row.bBaseBody || Forms->TrySetBaseForm(Row.BaseBody)
			   ? FText::GetEmpty()
			   : LOCTEXT("BaseRefused", "Change your body in the hub or at a rest spot, outside a fight.");
}

FText WoRFormsScreen::SwitchBaseBody(UWorld *World)
{
	UCharacterFormSubsystem *Forms = FormsScreenBuild::GetForms(World);
	const EPlayerBaseForm Other = Forms && Forms->GetBaseForm() == EPlayerBaseForm::Quinn ? EPlayerBaseForm::Manny : EPlayerBaseForm::Quinn;
	return Forms && Forms->TrySetBaseForm(Other) ? FText::GetEmpty()
												 : LOCTEXT("BaseRefused", "Change your body in the hub or at a rest spot, outside a fight.");
}

FString WoRFormsScreen::GetFormsScreenString(UWorld *World)
{
	const FFormsScreenPage Page = BuildPage(World);
	FString Out = FString::Printf(TEXT("%s - %s\n"), *Page.Title.ToString(), *Page.WearingLine.ToString());
	for (const FFormsScreenRow &Row : Page.Rows)
	{
		Out += FString::Printf(TEXT("  %s %s%s%s%s\n"), *Row.Name.ToString(), *FText::AsPercent(Row.Progress).ToString(), Row.bUnlocked ? TEXT(" unlocked") : TEXT(""),
							   Row.bWorn ? TEXT(" WORN") : TEXT(""), Row.bMet ? TEXT("") : TEXT(" (not met)"));
		for (const FFormsScreenPiece &Piece : Row.Pieces)
		{
			Out += FString::Printf(TEXT("      %s%s%s\n"), *Piece.Name.ToString(), Piece.bOwned ? TEXT(" owned") : TEXT(""), Piece.bWorn ? TEXT(" worn") : TEXT(""));
		}
	}
	return Out;
}

namespace FormsScreenDebug
{
	FAutoConsoleCommandWithWorld GPrintFormsCmd(TEXT("wor.UI.PrintForms"),
												TEXT("DEBUG: wor.UI.PrintForms - the Spirit of {character} screen's rows: meters, worn, met, pieces (B-444)."),
												FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
																							   { UE_LOG(LogWoRUI, Display, TEXT("\n%s"), *WoRFormsScreen::GetFormsScreenString(World)); }));
}

#undef LOCTEXT_NAMESPACE
