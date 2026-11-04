// CombatPlayerController.h
// Player controller for turn-based combat. Owns the REAL-TIME defense input
// (block / parry / dodge) via Enhanced Input. Turn-based action selection is
// handled by UCombatCommandMenuSubsystem + the orchestrator.
//
// B-229 (co-op): the controller is where a player's inputs reach the server. A defense press
// and a confirmed menu pick (with its infusion commit) go through RequestDefenseInput /
// RequestMenuPick: on authority (host, standalone) they run HandleDefenseInput / HandleMenuPick
// directly, as before; on a client they are sent by Server RPC and the server runs the same
// Handle function, which checks the sender controls the character (ControlsActor).
// Debug: GetInputDebugString, `wor.Net.PrintInputs`.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "UI/Combat/PieMenuButtonData.h"
#include "Combat/Actions/CombatPickRequest.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "CombatPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UDefenseSystem;
class ACombatOrchestrator;

/** B-229: what the server did with one player input. */
UENUM()
enum class ECombatInputVerdict : uint8
{
	Accepted,
	/** The pick names no character, or no defense window is open for a player-driven character. */
	NoActor,
	/** The sender doesn't control that character (ACombatPlayerController::ControlsActor). */
	NotControlled,
	/** No fight in progress. */
	NoCombat,
	/** The character doesn't have the turn. */
	NotTheirTurn,
	/** An action is still running. */
	Busy,
	/** B-229 review: a client picked a spell or ability its character's loadout doesn't offer. */
	NotOffered
};

/** B-229: one input the server handled, for the debug log. */
struct FCombatInputRecord
{
	FString Description;
	ECombatInputVerdict Verdict = ECombatInputVerdict::Accepted;
	/** Arrived by Server RPC (a client's input), not from this machine. */
	bool bFromClient = false;
	double ServerTime = 0.0;
};

UCLASS()
class WORLD_OF_REFRACTION_API ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========================================
	// INPUT ASSETS (assign in Blueprint / editor)
	// ========================================

	/** Combat input mapping context — added at priority 0 in BeginPlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> CombatMappingContext;

	/** The Guard input (D-084: Block and Parry are one defense). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> BlockAction;

	/** Legacy: also a Guard press until its key is unbound in the mapping context (Q-168). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> ParryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> DodgeLeftAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> DodgeRightAction;

	// ========================================
	// MENU KEYS (B-093, Q-093) — built in code, fixed keys (D-063)
	// ========================================
	// Defend, the infusion source and the infusion level press the command menu's own
	// buttons (UCombatCommandMenuSubsystem::PressMenuKey), so they follow the menu's rules.

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuKeysContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DefendAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InfusionAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InfusionLevelAction;

	/** Creates the menu-key actions and their mapping context once. */
	void BuildMenuKeys();

	// ========================================
	// DEFENSE INPUT HANDLERS
	// ========================================

	void OnBlock(const FInputActionValue &Value);
	void OnParry(const FInputActionValue &Value);
	void OnDodgeLeft(const FInputActionValue &Value);
	void OnDodgeRight(const FInputActionValue &Value);

	void OnDefend(const FInputActionValue &Value);
	void OnInfusion(const FInputActionValue &Value);
	void OnInfusionLevel(const FInputActionValue &Value);

	/** Routes a menu key to the command menu; only the local player's keys count. */
	void PressMenuKey(EPieMenuCategory Category);

public:
	/** Debug: each menu key's action, keyboard key and gamepad button. */
	static FString GetMenuKeysString();

	// ========================================
	// CO-OP INPUT ROUTING (B-229)
	// ========================================

	/** The one entry for a confirmed command-menu pick. Authority: HandleMenuPick here; client: Server_SubmitMenuPick. */
	void RequestMenuPick(const FCombatPickRequest &Pick);

	/** The one entry for a Guard / Dodge press. Authority: HandleDefenseInput here (stamped now); client:
	 *  Server_SubmitDefenseInput, stamped with the replicated server clock so the server judges the real press time. */
	void RequestDefenseInput(EDefenseType DefenseType, EDefenseDirection Direction);

	/** Server side of a pick (also the authority path): the sender must control Pick.Actor and it must have the
	 *  turn, then the orchestrator runs it. */
	ECombatInputVerdict HandleMenuPick(const FCombatPickRequest &Pick, bool bFromClient = false);

	/** Server side of a defense press: the open-window player defender, if the sender controls it. PressServerTime
	 *  0 stamps now (the local path); a client's time is clamped to [now - MAX latency, now]. */
	ECombatInputVerdict HandleDefenseInput(EDefenseType DefenseType, EDefenseDirection Direction, double PressServerTime,
										   bool bFromClient = false);

	/** Does Sender command Actor? Its own pawn, an actor it owns, or (until B-232 assigns characters to players)
	 *  any player-driven combatant: shared party control. A bot-driven pawn (enemy, AI ally) is never controlled. */
	static bool ControlsActor(const AController *Sender, const AActor *Actor, bool bFromClient = false);

	/** This machine's combat player controller (the first local player's), or null (headless, other PC class). */
	static ACombatPlayerController *FindLocal(const UObject *WorldContext);

	/** The last inputs this controller handled on the server, oldest first (up to MAX_RECENT_INPUTS). */
	const TArray<FCombatInputRecord> &GetRecentInputs() const { return RecentInputs; }

	/** Debug: the recent inputs, one per line with verdict and origin. */
	FString GetInputDebugString() const;

protected:

	// ========================================
	// HELPERS
	// ========================================

	/** DefenseSystem subsystem (GameInstance). Mirrors the GI-subsystem getter pattern. */
	UDefenseSystem *GetDefenseSystem() const;

	// ========================================
	// SERVER RPCS (B-229)
	// ========================================

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SubmitMenuPick(const FCombatPickRequest &Pick);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SubmitDefenseInput(EDefenseType DefenseType, EDefenseDirection Direction, double PressServerTime);

private:
	/** The fight in progress in this world, or null. */
	ACombatOrchestrator *FindActiveOrchestrator() const;

	void RecordInput(const FString &Description, ECombatInputVerdict Verdict, bool bFromClient);

	TArray<FCombatInputRecord> RecentInputs;
};
