// GuardDebug.cpp

#include "Debug/Combat/GuardDebug.h"
#include "Combat/Defense/EGuardStyle.h"
#include "Combat/Defense/EParryElementPriority.h"
#include "Combat/Presentation/ParryVisuals.h"
#include "Loadout/LoadoutComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Animation/AnimMontage.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "WoRLog.h"

namespace
{
	constexpr float GUARD_DEBUG_SCREEN_SECONDS = 10.0f;
	constexpr int32 GUARD_SET_STYLE_ARG = 0;
	constexpr int32 GUARD_SET_NAME_ARG = 1;

	FString GuardName(EGuardStyle Style)
	{
		return StaticEnum<EGuardStyle>()->GetNameStringByValue(static_cast<int64>(Style));
	}

	/** Every live loadout in World. */
	TArray<ULoadoutComponent *> WorldLoadouts(UWorld *World)
	{
		TArray<ULoadoutComponent *> Loadouts;
		for (TObjectIterator<ULoadoutComponent> It; It; ++It)
		{
			if (!It->IsTemplate() && It->GetWorld() == World)
			{
				Loadouts.Add(*It);
			}
		}
		return Loadouts;
	}

	/** The first player's pawn's loadout, else the first loadout in the world. */
	ULoadoutComponent *PlayerLoadout(UWorld *World)
	{
		if (const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			if (ULoadoutComponent *Loadout = PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<ULoadoutComponent>() : nullptr)
			{
				return Loadout;
			}
		}
		const TArray<ULoadoutComponent *> Loadouts = WorldLoadouts(World);
		return Loadouts.Num() > 0 ? Loadouts[0] : nullptr;
	}

	FAutoConsoleCommandWithWorld GGuardPrintCmd(
		TEXT("wor.Guard.Print"),
		TEXT("DEBUG (B-392, B-393): every combatant's guard (Weapon / Unarmed), weapon in hand, the montage a Guard plays, "
			 "and what a landed parry plays (spark, parry effect and its element colour)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { WoRGuardDebug::PrintGuardState(World); }));

	FAutoConsoleCommandWithWorldAndArgs GGuardSetCmd(
		TEXT("wor.Guard.Set"),
		TEXT("DEBUG (B-392 placeholder until the loadout screen): wor.Guard.Set <Weapon|Unarmed> [name part] - the player's "
			 "character, or every combatant whose name holds the part, parries with its weapon or unarmed (D-170)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const int64 Value = Args.IsValidIndex(GUARD_SET_STYLE_ARG) ? StaticEnum<EGuardStyle>()->GetValueByNameString(Args[GUARD_SET_STYLE_ARG]) : INDEX_NONE;
				if (Value == INDEX_NONE)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] usage: wor.Guard.Set <Weapon|Unarmed> [name part]"));
					return;
				}
				const EGuardStyle Style = static_cast<EGuardStyle>(Value);
				TArray<ULoadoutComponent *> Targets;
				if (Args.IsValidIndex(GUARD_SET_NAME_ARG))
				{
					for (ULoadoutComponent *Loadout : WorldLoadouts(World))
					{
						if (GetNameSafe(Loadout->GetOwner()).Contains(Args[GUARD_SET_NAME_ARG]))
						{
							Targets.Add(Loadout);
						}
					}
				}
				else if (ULoadoutComponent *Loadout = PlayerLoadout(World))
				{
					Targets.Add(Loadout);
				}
				if (Targets.Num() == 0)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] no combatant found (in a level with the player?)"));
					return;
				}
				for (ULoadoutComponent *Loadout : Targets)
				{
					if (!Loadout->SetGuardStyle(Style))
					{
						UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] %s: refused (no weapon in hand: already unarmed)"),
							   *GetNameSafe(Loadout->GetOwner()));
					}
					UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] %s"), *WoRGuardDebug::GetGuardString(Loadout));
				}
			}));

	constexpr int32 PARRY_PRIORITY_ARG = 0;
	constexpr int32 PARRY_PRIORITY_NAME_ARG = 1;

	FAutoConsoleCommandWithWorldAndArgs GGuardSetParryPriorityCmd(
		TEXT("wor.Guard.SetParryPriority"),
		TEXT("DEBUG (B-393 placeholder until the loadout screen): wor.Guard.SetParryPriority <Weapon|Magic> [name part] - "
			 "which element colours the landed-parry effect when the weapon guard has both a weapon crystal and the "
			 "character's own magic (D-170)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const int64 Value = Args.IsValidIndex(PARRY_PRIORITY_ARG)
										? StaticEnum<EParryElementPriority>()->GetValueByNameString(Args[PARRY_PRIORITY_ARG])
										: INDEX_NONE;
				if (Value == INDEX_NONE)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] usage: wor.Guard.SetParryPriority <Weapon|Magic> [name part]"));
					return;
				}
				TArray<ULoadoutComponent *> Targets;
				if (Args.IsValidIndex(PARRY_PRIORITY_NAME_ARG))
				{
					for (ULoadoutComponent *Loadout : WorldLoadouts(World))
					{
						if (GetNameSafe(Loadout->GetOwner()).Contains(Args[PARRY_PRIORITY_NAME_ARG]))
						{
							Targets.Add(Loadout);
						}
					}
				}
				else if (ULoadoutComponent *Loadout = PlayerLoadout(World))
				{
					Targets.Add(Loadout);
				}
				if (Targets.Num() == 0)
				{
					UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] no combatant found (in a level with the player?)"));
					return;
				}
				for (ULoadoutComponent *Loadout : Targets)
				{
					Loadout->SetParryElementPriority(static_cast<EParryElementPriority>(Value));
					UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] %s"), *WoRGuardDebug::GetGuardString(Loadout));
				}
			}));
}

FString WoRGuardDebug::GetGuardString(const ULoadoutComponent *Loadout)
{
	if (!Loadout)
	{
		return TEXT("(no loadout)");
	}
	const UWeaponData *Weapon = Loadout->GetActiveWeapon();
	const UAnimMontage *Montage = Loadout->GetGuardMontage();
	const UCharacterDataComponent *CharComp =
		Loadout->GetOwner() ? Loadout->GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const bool bOnRecord = CharComp && CharComp->PartyMemberId.IsValid() && CharComp->FindCarriedPartyMember();
	const AActor *Owner = Loadout->GetOwner();
	return FString::Printf(TEXT("%s: guard %s | weapon %s | montage %s | record %s\n  parry (B-393): %s -> %s"),
						   *GetNameSafe(Owner), *GuardName(Loadout->GetGuardStyle()), Weapon ? *Weapon->Name : TEXT("none"),
						   *GetNameSafe(Montage), bOnRecord ? TEXT("yes (saved)") : TEXT("no (this pawn only)"),
						   *ParryVisuals::GetInputsString(ParryVisuals::GatherInputs(Owner, false)),
						   *ParryVisuals::GetChoiceString(ParryVisuals::ResolveForDefender(Owner, false)));
}

void WoRGuardDebug::PrintGuardState(UWorld *World)
{
	const TArray<ULoadoutComponent *> Loadouts = WorldLoadouts(World);
	for (const ULoadoutComponent *Loadout : Loadouts)
	{
		const FString Line = GetGuardString(Loadout);
		UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, GUARD_DEBUG_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
	UE_LOG(LogWoRLoadout, Display, TEXT("[Guard] %d combatant(s)"), Loadouts.Num());
}
