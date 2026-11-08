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

/** B-232c: a free command-menu step (no turn spent): the server runs it for the member it is about. */
UENUM()
enum class ECombatFreeCommand : uint8
{
	SwitchWeapon,
	SwitchAdapter,
	MoveRowForward,
	MoveRowBack,
	/** D-026: the member goes to the AI for the rest of the fight. */
	HandToAI,
	MAX UMETA(Hidden)
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

	/** The Guard input (D-084, D-135). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> BlockAction;

	/** OBSOLETE (B-343, D-135): no longer bound; kept so the Blueprint default isn't lost before the editor step
	 *  removes IA_Parry. Delete in the D-124 sweep. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> ParryAction;

	/** The Dodge input (D-135: one Dodge; the property keeps its old name so the Blueprint default stays).
	 *  TODO: rename DodgeLeftAction to DodgeAction in the D-124 sweep (an editor step: the Blueprint default moves with it). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Defense")
	TObjectPtr<UInputAction> DodgeLeftAction;

	/** OBSOLETE (B-343, D-135): no longer bound (no left / right split). Delete in the D-124 sweep. */
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
	void OnDodge(const FInputActionValue &Value);

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

	/** B-232c: the one entry for a free menu step (weapon / adapter switch, a row step, hand to the AI).
	 *  Authority: HandleFreeCommand here; client: Server_SubmitFreeCommand, and on acceptance the client's own copy
	 *  of the member follows (Client_FreeCommandApplied) so its menu redraws from the same loadout. */
	void RequestFreeCommand(AActor *Actor, ECombatFreeCommand Command);

	/** B-232c server side (also the authority path): the same checks as a pick (the sender controls Actor, a fight,
	 *  Actor's turn, nothing running), then the step itself; NotOffered when the step is refused (no second weapon,
	 *  the switch used, no row to step to). */
	ECombatInputVerdict HandleFreeCommand(AActor *Actor, ECombatFreeCommand Command, bool bFromClient = false);

	/** Server side of a pick (also the authority path): the sender must control Pick.Actor and it must have the
	 *  turn, then the orchestrator runs it. */
	ECombatInputVerdict HandleMenuPick(const FCombatPickRequest &Pick, bool bFromClient = false);

	/** Server side of a defense press: the open-window player defender, if the sender controls it. PressServerTime
	 *  0 stamps now (the local path); a client's time is clamped to [now - MAX latency, now]. */
	ECombatInputVerdict HandleDefenseInput(EDefenseType DefenseType, EDefenseDirection Direction, double PressServerTime,
										   bool bFromClient = false);

	/** Does Sender command Actor? Its own pawn, an actor it owns, or, for a member of the side's roster, the seat rule
	 *  (B-232, Q-21-audit, URunStateComponent::IsCommandedBy): its own characters, an unclaimed member when the
	 *  rotation is on Sender, never an AI seat. Off the roster (no GameState run state, a test actor) the old rule
	 *  stays: any player-driven combatant, and a non-pawn combatant only for a pick made on this machine (bFromClient
	 *  false, B-229). A bot-driven pawn (enemy) is never controlled. */
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

	/** B-232b (review): the server refused this client's menu pick; the client reopens its menu for the same turn. */
	UFUNCTION(Client, Reliable)
	void Client_MenuPickRefused(ECombatInputVerdict Verdict);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SubmitDefenseInput(EDefenseType DefenseType, EDefenseDirection Direction, double PressServerTime);

	/** B-232c */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SubmitFreeCommand(AActor *Actor, ECombatFreeCommand Command);

	/** B-232c: the server ran the step; the client's copy of Actor does the same (its loadout is not replicated) and
	 *  its menu redraws. */
	UFUNCTION(Client, Reliable)
	void Client_FreeCommandApplied(AActor *Actor, ECombatFreeCommand Command);

private:
	/** The fight in progress in this world, or null. */
	ACombatOrchestrator *FindActiveOrchestrator() const;

	void RecordInput(const FString &Description, ECombatInputVerdict Verdict, bool bFromClient);

	/** B-232c: runs Command on Actor on this machine (the server's step, or a client following it). */
	static bool ApplyFreeCommand(ACombatOrchestrator *Orchestrator, AActor *Actor, ECombatFreeCommand Command);

	TArray<FCombatInputRecord> RecentInputs;
};
