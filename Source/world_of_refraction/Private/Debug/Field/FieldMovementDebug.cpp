// FieldMovementDebug.cpp

#include "Debug/Field/FieldMovementDebug.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Field/FieldMovementComponent.h"
#include "Field/FieldSkillUnlockSubsystem.h"
#include "Combat/WoRFightSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "WoRLog.h"

namespace
{
	constexpr float FIELD_DEBUG_SCREEN_SECONDS = 10.0f;
	constexpr int32 FIELD_SET_SOURCE_ARG = 0;
	constexpr int32 FIELD_SET_TYPE_ARG = 1;
	constexpr int32 FIELD_UNLOCK_TYPE_ARG = 0;
	constexpr int32 FIELD_UNLOCK_ELEMENT_ARG = 1;
	constexpr int32 FIELD_FLIGHT_LIMITS_ARG = 0;
	constexpr int32 FIELD_FLY_VERTICAL_ARG = 0;
	constexpr int32 FIELD_ATTACK_HOLD_ARG = 0;

	/** The first player's pawn's field movement (the leader on the map). */
	UFieldMovementComponent *PlayerFieldMovement(UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		UFieldMovementComponent *FieldMovement = PC && PC->GetPawn() ? PC->GetPawn()->FindComponentByClass<UFieldMovementComponent>() : nullptr;
		if (!FieldMovement)
		{
			UE_LOG(LogWoR, Warning, TEXT("[FieldMovementDebug] The player's pawn has no field movement (only the hub / trial "
										 "controller adds it, B-214a)"));
		}
		return FieldMovement;
	}

	void Report(const FString &Line)
	{
		UE_LOG(LogWoR, Log, TEXT("[FieldMovementDebug] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, FIELD_DEBUG_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}

	FAutoConsoleCommandWithWorld GFieldPrintCmd(
		TEXT("wor.Field.Print"),
		TEXT("DEBUG (B-214a): every field pawn's active field skill (source, type, the source's element and whether it may "
			 "be picked), Enhanced Sprint on / off and its speed multiplier, EP, the dash cooldown, and whether the choice "
			 "lives on a party record (saved)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { WoRFieldMovementDebug::PrintFieldState(World); }));

	FAutoConsoleCommandWithWorld GFieldActivateCmd(
		TEXT("wor.Field.Activate"),
		TEXT("DEBUG (B-214a, until B-214g's IA_Field_MovementSkill): the player's character uses its active field skill, "
			 "as a double-tap of sprint does (Dash dashes, Enhanced Sprint toggles)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				if (UFieldMovementComponent *FieldMovement = PlayerFieldMovement(World))
				{
					const EFieldSkillRefusal Refusal = FieldMovement->ActivateFieldSkill();
					Report(FString::Printf(TEXT("Activate: %s | %s"), *FieldSkillRules::RefusalName(Refusal),
										   *FieldMovement->GetFieldStateString()));
				}
			}));

	FAutoConsoleCommandWithWorld GFieldDashCmd(
		TEXT("wor.Field.Dash"),
		TEXT("DEBUG (B-214a): the player's character does the normal dash (everyone's, free, on a cooldown)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				if (UFieldMovementComponent *FieldMovement = PlayerFieldMovement(World))
				{
					Report(FString::Printf(TEXT("Dash: %s"), *FieldSkillRules::RefusalName(FieldMovement->Dash())));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GFieldSetCmd(
		TEXT("wor.Field.Set"),
		TEXT("DEBUG (B-214a; the party screen's picker is B-214b's): wor.Field.Set <Innate|Weapon|Ring> <Dash|EnhancedSprint|...> - "
			 "picks the player's character's active field skill (refused without an elemental source, not unlocked, or not built yet)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const int64 Source = Args.IsValidIndex(FIELD_SET_SOURCE_ARG)
										 ? StaticEnum<EFieldSkillSource>()->GetValueByNameString(Args[FIELD_SET_SOURCE_ARG])
										 : INDEX_NONE;
				const int64 Type = Args.IsValidIndex(FIELD_SET_TYPE_ARG)
									   ? StaticEnum<EFieldMovementType>()->GetValueByNameString(Args[FIELD_SET_TYPE_ARG])
									   : INDEX_NONE;
				if (Source == INDEX_NONE || Type == INDEX_NONE)
				{
					UE_LOG(LogWoR, Warning, TEXT("[FieldMovementDebug] Usage: wor.Field.Set <Innate|Weapon|Ring> <Dash|EnhancedSprint|Glide|...>"));
					return;
				}
				if (UFieldMovementComponent *FieldMovement = PlayerFieldMovement(World))
				{
					const EFieldSkillRefusal Refusal =
						FieldMovement->SetActiveFieldSkill(static_cast<EFieldSkillSource>(Source), static_cast<EFieldMovementType>(Type));
					Report(FString::Printf(TEXT("Set: %s | %s"), *FieldSkillRules::RefusalName(Refusal),
										   *FieldMovement->GetFieldStateString()));
				}
			}));

	FAutoConsoleCommandWithWorld GFieldUnlocksCmd(
		TEXT("wor.Field.Unlocks"),
		TEXT("DEBUG (B-214b): the account's field skill unlocks: the best rank each aura reached per element, and every "
			 "type unlocked per element (after a scan of the store's spells)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { WoRFieldMovementDebug::PrintUnlocks(World); }));

	FAutoConsoleCommandWithWorldAndArgs GFieldUnlockCmd(
		TEXT("wor.Field.Unlock"),
		TEXT("DEBUG (B-214b): wor.Field.Unlock <EnhancedSprint|Glide|...> <Fire|Lightning|...> - unlocks a movement type for "
			 "an element for the account, as ranking its aura spells would (refused for Teleport but Void, and for "
			 "Generic / None)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const int64 Type = Args.IsValidIndex(FIELD_UNLOCK_TYPE_ARG)
									   ? StaticEnum<EFieldMovementType>()->GetValueByNameString(Args[FIELD_UNLOCK_TYPE_ARG])
									   : INDEX_NONE;
				const int64 Element = Args.IsValidIndex(FIELD_UNLOCK_ELEMENT_ARG)
										  ? StaticEnum<ESpellElement>()->GetValueByNameString(Args[FIELD_UNLOCK_ELEMENT_ARG])
										  : INDEX_NONE;
				UFieldSkillUnlockSubsystem *Unlocks = WoRFight::Get<UFieldSkillUnlockSubsystem>(World);
				if (Type == INDEX_NONE || Element == INDEX_NONE || !Unlocks)
				{
					UE_LOG(LogWoR, Warning, TEXT("[FieldMovementDebug] Usage: wor.Field.Unlock <EnhancedSprint|Glide|...> <Fire|Lightning|...>"));
					return;
				}
				const bool bUnlocked = Unlocks->UnlockForDebug(static_cast<EFieldMovementType>(Type), static_cast<ESpellElement>(Element));
				Report(FString::Printf(TEXT("Unlock %s %s: %s"), *Args[FIELD_UNLOCK_TYPE_ARG], *Args[FIELD_UNLOCK_ELEMENT_ARG],
									   bUnlocked ? TEXT("unlocked") : TEXT("refused (that element can't unlock it)")));
			}));

	FAutoConsoleCommandWithWorldAndArgs GFieldFlightLimitsCmd(
		TEXT("wor.Field.FlightLimits"),
		TEXT("DEBUG (B-214d): wor.Field.FlightLimits <None|Unlimited|NoCeiling|IgnoreNoFly|All> - lifts the player's "
			 "character's flight limits as an effect would (unlimited EP, no height ceiling, no-fly zones ignored). Run it "
			 "on the host: the limits are the server's."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const FString Mode = Args.IsValidIndex(FIELD_FLIGHT_LIMITS_ARG) ? Args[FIELD_FLIGHT_LIMITS_ARG] : FString();
				const bool bAll = Mode == TEXT("All");
				FFieldFlightLimits Limits;
				Limits.bUnlimitedEP = bAll || Mode == TEXT("Unlimited");
				Limits.bNoCeiling = bAll || Mode == TEXT("NoCeiling");
				Limits.bIgnoreNoFly = bAll || Mode == TEXT("IgnoreNoFly");
				if (!bAll && Mode != TEXT("None") && !Limits.bUnlimitedEP && !Limits.bNoCeiling && !Limits.bIgnoreNoFly)
				{
					UE_LOG(LogWoR, Warning, TEXT("[FieldMovementDebug] Usage: wor.Field.FlightLimits <None|Unlimited|NoCeiling|IgnoreNoFly|All>"));
					return;
				}
				if (UFieldMovementComponent *FieldMovement = PlayerFieldMovement(World))
				{
					FieldMovement->SetFlightLimits(Limits);
					Report(FString::Printf(TEXT("FlightLimits %s | %s"), *Mode, *FieldMovement->GetFieldStateString()));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GFieldFlyVerticalCmd(
		TEXT("wor.Field.FlyVertical"),
		TEXT("DEBUG (B-214d, until B-214g's fly up / down keys): wor.Field.FlyVertical <1|-1> - pushes the flying player's "
			 "character up or down."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const float Direction = Args.IsValidIndex(FIELD_FLY_VERTICAL_ARG) ? FCString::Atof(*Args[FIELD_FLY_VERTICAL_ARG]) : 0.0f;
				if (FMath::IsNearlyZero(Direction))
				{
					UE_LOG(LogWoR, Warning, TEXT("[FieldMovementDebug] Usage: wor.Field.FlyVertical <1|-1>"));
					return;
				}
				if (UFieldMovementComponent *FieldMovement = PlayerFieldMovement(World))
				{
					FieldMovement->NudgeFlightVertical(Direction);
					Report(FString::Printf(TEXT("FlyVertical %s | flying %s"), *Args[FIELD_FLY_VERTICAL_ARG],
										   FieldMovement->IsFlying() ? TEXT("yes") : TEXT("no (nothing done)")));
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GFieldAttackCmd(
		TEXT("wor.Field.Attack"),
		TEXT("DEBUG (B-214f, until B-214g's IA_Field_Attack): wor.Field.Attack [Hold] - the player's character swings; an "
			 "enemy struck starts the fight with first strike and the hit's buildup. Hold: the held attack (Water = Ice)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				const bool bHeld = Args.IsValidIndex(FIELD_ATTACK_HOLD_ARG) && Args[FIELD_ATTACK_HOLD_ARG] == TEXT("Hold");
				if (UFieldMovementComponent *FieldMovement = PlayerFieldMovement(World))
				{
					const FFieldStrike Strike = FieldMovement->GetFieldStrike(bHeld);
					const EFieldSkillRefusal Refusal = FieldMovement->FieldAttack(bHeld);
					Report(FString::Printf(TEXT("Attack%s: %s | strike %s%s, %s, buildup %.0f"), bHeld ? TEXT(" (held)") : TEXT(""),
										   *FieldSkillRules::RefusalName(Refusal), *UEnum::GetValueAsString(Strike.Element),
										   Strike.bIce ? TEXT(" (Ice)") : TEXT(""), *UEnum::GetValueAsString(Strike.Physical), Strike.Buildup));
				}
			}));

	FAutoConsoleCommandWithWorld GFieldSwitchSourceCmd(
		TEXT("wor.Field.SwitchSource"),
		TEXT("DEBUG (B-214f, until B-214g's IA_Field_SwitchSource): the field attack's source steps Innate -> Weapon -> Ring."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				if (UFieldMovementComponent *FieldMovement = PlayerFieldMovement(World))
				{
					FieldMovement->SwitchFieldAttackSource();
					Report(FieldMovement->GetFieldStateString());
				}
			}));
}

void WoRFieldMovementDebug::PrintUnlocks(UWorld *World)
{
	UFieldSkillUnlockSubsystem *Unlocks = WoRFight::Get<UFieldSkillUnlockSubsystem>(World);
	if (!Unlocks)
	{
		Report(TEXT("No field skill unlock subsystem"));
		return;
	}
	Unlocks->RecordFromPool();
	Report(Unlocks->GetUnlocksString());
}

void WoRFieldMovementDebug::PrintFieldState(UWorld *World)
{
	int32 Count = 0;
	for (TObjectIterator<UFieldMovementComponent> It; It; ++It)
	{
		if (!It->IsTemplate() && It->GetWorld() == World)
		{
			Report(It->GetFieldStateString());
			++Count;
		}
	}
	if (Count == 0)
	{
		Report(TEXT("No field pawn (the hub / trial controller adds field movement on possess, B-214a)"));
	}
}
