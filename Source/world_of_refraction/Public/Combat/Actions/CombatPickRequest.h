// CombatPickRequest.h
// B-229 (Design/CoopStateOwnership.md, "what breaks first" #3): a command-menu pick in a shape that can cross
// the network. A client's menu builds an FAction, packs it here with the acting character, and sends it to the
// server (ACombatPlayerController::Server_SubmitMenuPick); the server unpacks it back into an FAction.
// The infusion commit (source + level, B-034) rides in the pick: the menu stamps it at confirm time (Q-229).
// Data assets (spell / skill) and replicated combatants are net-addressable, so they travel as pointers.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/ActionStructs.h"
#include "CombatPickRequest.generated.h"

class AActor;
class USpellData;
class USkillDataBase;

USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCombatPickRequest
{
	GENERATED_BODY()

	/** The character the pick is for (the server checks it has the turn and the sender controls it). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick")
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick")
	EActionType ActionType = EActionType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick")
	TObjectPtr<USpellData> SpellData = nullptr;

	/** Attacks and abilities (the merged pointer, FAction::SkillData). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick")
	TObjectPtr<USkillDataBase> SkillData = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick")
	FCrystalId ItemData;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick")
	ESpellSource SpellSource = ESpellSource::Innate;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick")
	TArray<TObjectPtr<AActor>> Targets;

	/** The infusion commit: the source and level the menu stamped (level 0 = none). */
	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick|Infusion")
	EInfusionSourceOption InfusionSource = EInfusionSourceOption::None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Pick|Infusion")
	int32 InfusionLevel = 0;

	/** Packs Action for InActor. The infusion level is the one for the action's type (FAction::GetChargeLevel). */
	static FCombatPickRequest FromAction(AActor *InActor, const FAction &Action);

	/** Unpacks into an FAction. The level goes back to the field for the action's type, clamped to
	 *  0..MAX_INFUSION_LEVEL; any other action type carries no infusion. */
	FAction ToAction() const;

	/** Every enum holds a value its type defines (the server's RPC validation). */
	bool HasValidEnums() const;

	/** "Actor: Type -> N targets, infusion L (source)" for the input log. */
	FString GetDebugString() const;
};
