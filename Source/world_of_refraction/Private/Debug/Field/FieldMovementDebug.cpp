// FieldMovementDebug.cpp

#include "Debug/Field/FieldMovementDebug.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Field/FieldMovementComponent.h"
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
		TEXT("DEBUG (B-214a placeholder until B-214b's picker): wor.Field.Set <Innate|Weapon|Ring> <Dash|EnhancedSprint|...> - "
			 "picks the player's character's active field skill (refused without an elemental source, or not built yet)."),
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
