// FieldMovementComponent.h
// B-214a (D-099, Design/FieldAbilities.md): field movement on the map pawn. AWoRHubController adds it on possess
// (server) and it replicates to the owning client.
//   - The normal dash: everyone, free, on a cooldown.
//   - The active field skill (FFieldSkillChoice), whose home is the party record (FPartyMember::FieldSkill, saved);
//     a pawn with no record keeps it here. Double-tap sprint (IA_Field_MovementSkill) or wor.Field.Activate uses it.
//   - Enhanced Sprint: a faster sprint while on (by the source's element), EP per second while moving at sprint speed
//     (Q-214a, Crown), off at 0 EP or on a second double-tap. UHubMovementLibrary::ComputeHubSprintSpeed reads GetSprintSpeedMultiplier.
// A client's calls go to the server (it spends the EP and owns the record); the on / off state and the choice replicate.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Field/FieldSkillTypes.h"
#include "FieldMovementComponent.generated.h"

class UCharacterDataComponent;
struct FPartyMember;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnhancedSprintChanged, bool, bActive);

UCLASS(ClassGroup = (WoR), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UFieldMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFieldMovementComponent();

	/** The normal dash along the ground (velocity, else facing). Refused on cooldown or on a non-character pawn. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal Dash();

	/** Uses the active field skill: Dash dashes; Enhanced Sprint toggles. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal ActivateFieldSkill();

	/** Picks the active field skill (FieldSkillRules::CheckPick against the source's element now). Saved on the party
	 *  record when the pawn has one. Stops Enhanced Sprint when the choice changes. */
	UFUNCTION(BlueprintCallable, Category = "Field")
	EFieldSkillRefusal SetActiveFieldSkill(EFieldSkillSource Source, EFieldMovementType Type);

	UFUNCTION(BlueprintPure, Category = "Field")
	FFieldSkillChoice GetActiveFieldSkill() const;

	/** The element Source gives this pawn now (None when it has none). */
	UFUNCTION(BlueprintPure, Category = "Field")
	ESpellElement GetSourceElement(EFieldSkillSource Source) const;

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsEnhancedSprintActive() const { return bEnhancedSprintActive; }

	/** FieldSkillRules::NO_SPRINT_BOOST unless Enhanced Sprint is on. */
	UFUNCTION(BlueprintPure, Category = "Field")
	float GetSprintSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsDashReady() const;

	UFUNCTION(BlueprintCallable, Category = "Field")
	void StopEnhancedSprint();

	/** One Enhanced Sprint EP charge (the drain timer's step; public for the spec), charged only while the pawn moves at
	 *  sprint speed (Q-214a, Crown). Stops it below one charge. */
	void DrainEnhancedSprint();

	/** The pawn's real ground speed is a sprint (FieldSkillRules::IsMovingAtSprintSpeed). */
	UFUNCTION(BlueprintPure, Category = "Field")
	bool IsMovingAtSprintSpeed() const;

	/** Server: copies the party record's choice into the replicated mirror (after the leader is rebound). */
	void RefreshFromRecord();

	/** "Hero: skill Innate EnhancedSprint (Lightning) | sprint ON x1.50 | EP 7/10 | dash ready | record yes" */
	FString GetFieldStateString() const;

	UPROPERTY(BlueprintAssignable, Category = "Field")
	FOnEnhancedSprintChanged OnEnhancedSprintChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION(Server, Reliable)
	void ServerDash();

	UFUNCTION(Server, Reliable)
	void ServerActivateFieldSkill();

	UFUNCTION(Server, Reliable)
	void ServerSetActiveFieldSkill(EFieldSkillSource Source, EFieldMovementType Type);

	UFUNCTION()
	void OnRep_EnhancedSprintActive();

	EFieldSkillRefusal StartEnhancedSprint();
	void SetEnhancedSprintActive(bool bActive);

	/** Re-reads the sprint speed when the pawn is sprinting (the pawn Blueprint sets it on sprint press). */
	void ReapplySprintSpeed() const;

	bool HasAuthority() const;
	UCharacterDataComponent *GetCharacterData() const;
	const FPartyMember *FindRecord() const;

	/** The choice for a pawn with no party record, and the replicated mirror of the record's for the owning client. */
	UPROPERTY(Replicated)
	FFieldSkillChoice ActiveChoice;

	UPROPERTY(ReplicatedUsing = OnRep_EnhancedSprintActive)
	bool bEnhancedSprintActive = false;

	/** The element Enhanced Sprint started with (its speed); replicated so the client's sprint speed matches. */
	UPROPERTY(Replicated)
	ESpellElement EnhancedSprintElement = ESpellElement::None;

	double LastDashTime = -FieldSkillRules::DASH_COOLDOWN_SECONDS;
	FTimerHandle DrainTimer;
	mutable TWeakObjectPtr<UCharacterDataComponent> CachedCharacterData;
};
