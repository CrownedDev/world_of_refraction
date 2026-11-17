// FormsScreenModel.cpp

#include "UI/Hub/FormsScreenModel.h"

#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Character/Forms/SpiritLook.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
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

	/** B-558: the screen's members (the loadout screen's list: the local player first), MemberIndex clamped. */
	const FLoadoutScreenMember *GetMember(const TArray<FLoadoutScreenMember> &Members, int32 MemberIndex)
	{
		return Members.IsEmpty() ? nullptr : &Members[FMath::Clamp(MemberIndex, 0, Members.Num() - 1)];
	}

	/** B-558 (D-238): the party's recruit at MemberIndex (it wears its own spirit), or null (the player, anyone else). */
	FPartyMember *FindRecruit(UWorld *World, int32 MemberIndex)
	{
		const TArray<FLoadoutScreenMember> Members = WoRLoadoutScreen::BuildMembers(World);
		const FLoadoutScreenMember *Member = MemberIndex > 0 ? GetMember(Members, MemberIndex) : nullptr;
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		UParty *Party = Member && Member != &Members[0] && PartySession ? PartySession->GetLocalParty() : nullptr;
		FPartyMember *Found = Party ? Party->Members.FindByPredicate([Member](const FPartyMember &Each)
		{
			return UPoolSubsystem::GetCharacterKey(Each) == Member->Key;
		}) : nullptr;
		return Found && Found->Picks.bRecruit ? Found : nullptr;
	}
}

FFormsScreenPage WoRFormsScreen::BuildPage(const UCharacterFormSubsystem &Forms, const FText &CharacterName, TFunctionRef<bool(const UObject *)> Owns,
										   TFunctionRef<bool(const UObject *)> Wears)
{
	return BuildPage(Forms, CharacterName, Forms.GetWornForm(), Forms.GetBaseForm(), Owns, Wears);
}

FFormsScreenPage WoRFormsScreen::BuildPage(const UCharacterFormSubsystem &Forms, const FText &CharacterName, const UCharacterFormData *Worn,
										   EPlayerBaseForm WornBody, TFunctionRef<bool(const UObject *)> Owns, TFunctionRef<bool(const UObject *)> Wears)
{
	FFormsScreenPage Page;
	Page.Title = FText::Format(LOCTEXT("Title", "Body · {0}"), CharacterName); // D-241: was "Spirit of {0}"
	const FText Base = WoRGameNames::GetBaseFormName(WornBody);
	Page.WearingLine = Worn ? FText::Format(LOCTEXT("WearingForm", "Wearing: {0} (over {1})"), WoRGameNames::FormatBodyOf(Worn->FormName), Base)
							: FText::Format(LOCTEXT("Wearing", "Wearing: {0}"), Base);

	// D-234: your own bodies first (D-241: was spirits), Male Nobody then Female Nobody.
	for (const EPlayerBaseForm Body : {EPlayerBaseForm::Manny, EPlayerBaseForm::Quinn})
	{
		FFormsScreenRow &Own = Page.Rows.AddDefaulted_GetRef();
		Own.bBaseBody = true;
		Own.BaseBody = Body;
		Own.Name = WoRGameNames::GetBaseFormName(Body);
		Own.Line = LOCTEXT("AlwaysYours", "always yours");
		Own.Progress = 1.0f;
		Own.bUnlocked = true;
		Own.bWorn = !Worn && WornBody == Body;
		Own.Status = LOCTEXT("OwnStatus", "Your own body. A wears it (and takes a form off).");
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
		Row.Name = Row.bMet ? WoRGameNames::FormatBodyOf(Form->FormName) : LOCTEXT("Unmet", "???"); // D-241: "Body of Orthos"
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

int32 WoRFormsScreen::GetMemberCount(UWorld *World)
{
	return FMath::Max(1, WoRLoadoutScreen::BuildMembers(World).Num());
}

FFormsScreenPage WoRFormsScreen::BuildPage(UWorld *World, int32 MemberIndex)
{
	const UCharacterFormSubsystem *Forms = FormsScreenBuild::GetForms(World);
	const TArray<FLoadoutScreenMember> Members = WoRLoadoutScreen::BuildMembers(World);
	if (!Forms)
	{
		FFormsScreenPage Page;
		Page.Title = LOCTEXT("NoForms", "Body");
		return Page;
	}
	const FLoadoutScreenMember *Player = FormsScreenBuild::GetMember(Members, MemberIndex); // B-558: the member shown
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
	const FText Name = Player ? Player->Name : LOCTEXT("You", "you");
	if (const FPartyMember *Recruit = FormsScreenBuild::FindRecruit(World, MemberIndex))
	{
		return BuildPage(*Forms, Name, Recruit->SpiritForm.IsNone() ? nullptr : Forms->FindFormByName(Recruit->SpiritForm), Recruit->SpiritBody, Owns, Wears);
	}
	if (MemberIndex > 0 && Player && Player != &Members[0]) // neither the player nor a Spirit (an older Lord, a co-op player)
	{
		FFormsScreenPage Page = BuildPage(*Forms, Name, nullptr, EPlayerBaseForm::Manny, Owns, Wears);
		Page.WearingLine = LOCTEXT("NotRecruit", "Only you and the Spirits change body here.");
		for (FFormsScreenRow &Row : Page.Rows)
		{
			Row.bWorn = false;
		}
		return Page;
	}
	return BuildPage(*Forms, Name, Owns, Wears);
}

FText WoRFormsScreen::Wear(UWorld *World, const FFormsScreenRow &Row, int32 MemberIndex)
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
	// B-558 (D-238): a recruit wears its spirit on its party member, by the player's unlocks and the same rules.
	if (MemberIndex > 0)
	{
		// TODO: co-op (D-103): the party lives on the host and isn't replicated; a guest asks the host first.
		FPartyMember *Recruit = World && World->GetNetMode() != NM_Client ? FormsScreenBuild::FindRecruit(World, MemberIndex) : nullptr;
		const EFormSwitchRefusal RecruitRefusal = Recruit ? Forms->CanWearForm(Row.Form) : EFormSwitchRefusal::None;
		if (!Recruit || RecruitRefusal != EFormSwitchRefusal::None)
		{
			return Recruit ? CharacterFormSwitch::GetRefusalText(RecruitRefusal) : LOCTEXT("NotRecruit", "Only you and the Spirits change body here.");
		}
		Recruit->SpiritForm = Row.bBaseBody || !Row.Form ? NAME_None : Row.Form->GetFName();
		Recruit->SpiritBody = Row.bBaseBody ? Row.BaseBody : Recruit->SpiritBody;
		UE_LOG(LogWoRUI, Log, TEXT("[FormsScreen] %s"), *WoRSpiritLook::GetMemberSpiritString(*Recruit));
		WoRSpiritLook::DressMemberBodies(World, *Recruit);
		return FText::GetEmpty();
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

FText WoRFormsScreen::SwitchBaseBody(UWorld *World, int32 MemberIndex)
{
	UCharacterFormSubsystem *Forms = FormsScreenBuild::GetForms(World);
	if (MemberIndex > 0) // B-558: a recruit's body is its own spirit row (Male / Female Nobody), worn like one
	{
		const FPartyMember *Recruit = FormsScreenBuild::FindRecruit(World, MemberIndex);
		FFormsScreenRow Other;
		Other.bBaseBody = true;
		Other.BaseBody = Recruit && Recruit->SpiritBody == EPlayerBaseForm::Quinn ? EPlayerBaseForm::Manny : EPlayerBaseForm::Quinn;
		return Wear(World, Other, MemberIndex);
	}
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
