// CombatPlayerController.cpp

#include "Combat/CombatPlayerController.h"
#include "Player/RunStateComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EDefenseType.h"
#include "Combat/Defense/EDefenseDirection.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Combat/CombatControl.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "Combat/WoRFightSubsystem.h"
#include "Player/TurnStateComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "WoRAuthority.h"
#include "WoRLog.h"

namespace CombatMenuKeys
{
	/** Above the editor-authored combat context (priority 0), so a shared key goes to the menu. */
	constexpr int32 CONTEXT_PRIORITY = 1;

	struct FBinding
	{
		FName ActionName;
		EPieMenuCategory Category;
		FKey Keyboard;
		FKey Gamepad;
	};

	/** Q-093: fixed keys for now (D-063); each action name unique so rebinding can come later. */
	TArray<FBinding> GetBindings()
	{
		return {
			{TEXT("IA_Combat_MenuDefend"), EPieMenuCategory::Defend, EKeys::G, EKeys::Gamepad_FaceButton_Top},
			{TEXT("IA_Combat_MenuInfusionSource"), EPieMenuCategory::CycleSource, EKeys::R, EKeys::Gamepad_LeftShoulder},
			{TEXT("IA_Combat_MenuInfusionLevel"), EPieMenuCategory::CycleLevel, EKeys::T, EKeys::Gamepad_RightShoulder},
		};
	}

	/** B-406b (Q-406b): skips a boss cinematic. Its own action name (D-063); keys from InputMap.md. Functions, not
	 *  globals: an EKeys value is only safe to copy after its module's static init. */
	constexpr const TCHAR *SKIP_CINEMATIC_ACTION_NAME = TEXT("IA_Combat_SkipCinematic");
	FKey GetSkipCinematicKeyboard() { return EKeys::Tab; } // not Space: the likely dodge key and Slate's Accept
	FKey GetSkipCinematicGamepad() { return EKeys::Gamepad_Special_Right; }

	FAutoConsoleCommand GPrintMenuKeysCmd(
		TEXT("wor.Input.MenuKeys"),
		TEXT("DEBUG: the combat menu keys (B-093): action, keyboard key, gamepad button."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogTemp, Display, TEXT("%s"), *ACombatPlayerController::GetMenuKeysString());
		}));
}

namespace CombatInputRouting
{
	/** How far before the server's "now" a client's press may be stamped: covers the trip to the server.
	 *  An older stamp is clamped to it, so a client can't claim an arbitrarily early press. */
	constexpr double MAX_DEFENSE_INPUT_LATENCY_SECONDS = 0.5; // TODO: tune in 2-player PIE over a real connection
	/** B-343 (D-135): one Dodge button; the direction only picks the dodge montage. */
	constexpr EDefenseDirection DODGE_MONTAGE_DIRECTION = EDefenseDirection::Left;

	/** Inputs kept per controller for `wor.Net.PrintInputs`. */
	constexpr int32 MAX_RECENT_INPUTS = 8;

	/** A press stamped 0 is "now" to UDefenseSystem::SubmitDefenseInput. */
	constexpr double STAMP_NOW = 0.0;

	FAutoConsoleCommandWithWorld GPrintInputsCmd(
		TEXT("wor.Net.PrintInputs"),
		TEXT("DEBUG (B-229): each combat player controller's last inputs handled on this machine (menu picks, defense presses), with verdict and origin. Run on the host to see a client's."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			int32 Count = 0;
			for (TActorIterator<ACombatPlayerController> It(World); It; ++It)
			{
				UE_LOG(LogWoR, Log, TEXT("%s"), *It->GetInputDebugString());
				++Count;
			}
			if (Count == 0)
			{
				UE_LOG(LogWoR, Log, TEXT("[CombatInput] no combat player controller in this world"));
			}
		}));

	// Until a client builds its own command menu (B-228b), this is how a client makes a pick in 2-player PIE.
	FAutoConsoleCommandWithWorld GSendDefendPickCmd(
		TEXT("wor.Net.SendDefendPick"),
		TEXT("DEBUG (B-229): pick Defend for the replicated current actor through this machine's combat controller (a client sends it to the server)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			ACombatPlayerController *Controller = ACombatPlayerController::FindLocal(World);
			const UTurnStateComponent *TurnState = UTurnStateComponent::Find(World);
			AActor *CurrentActor = TurnState ? TurnState->GetCurrentActor() : nullptr;
			if (!Controller || !CurrentActor)
			{
				UE_LOG(LogWoR, Warning, TEXT("[CombatInput] wor.Net.SendDefendPick: needs a combat controller (%s) and a current actor in the turn state (%s)"),
					   Controller ? TEXT("ok") : TEXT("none"), *GetNameSafe(CurrentActor));
				return;
			}
			FAction Defend;
			Defend.ActionType = EActionType::Defend;
			Controller->RequestMenuPick(FCombatPickRequest::FromAction(CurrentActor, Defend));
		}));
}

void ACombatPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add the combat input mapping context at priority 0.
	if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (CombatMappingContext)
		{
			Subsystem->AddMappingContext(CombatMappingContext, 0);
			UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Added Combat Mapping Context"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[CombatPlayerController] CombatMappingContext not assigned!"));
		}
		BuildMenuKeys();
		Subsystem->AddMappingContext(MenuKeysContext, CombatMenuKeys::CONTEXT_PRIORITY);
	}
}

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Hygiene: the combat mapping context is added in BeginPlay onto the persistent
	// LocalPlayer subsystem; remove it here so it does not linger when leaving combat
	// (e.g. switching to the hub controller). Removes only its own context — additive.
	if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (CombatMappingContext)
		{
			Subsystem->RemoveMappingContext(CombatMappingContext);
		}
		if (MenuKeysContext)
		{
			Subsystem->RemoveMappingContext(MenuKeysContext);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ACombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent *EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	// Defense input — B-343 (D-135): two buttons, Guard and Dodge, each on press (Started). No gating here: the
	// DefenseSystem ignores input when no defense window is open. Parry and the right dodge are no longer bound
	// (their .uasset actions are removed in the editor later).
	if (BlockAction)
	{
		EnhancedInput->BindAction(BlockAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnBlock);
	}
	if (DodgeLeftAction)
	{
		EnhancedInput->BindAction(DodgeLeftAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnDodge);
	}

	BuildMenuKeys();
	EnhancedInput->BindAction(DefendAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnDefend);
	EnhancedInput->BindAction(InfusionAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnInfusion);
	EnhancedInput->BindAction(InfusionLevelAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnInfusionLevel);
	EnhancedInput->BindAction(SkipCinematicAction, ETriggerEvent::Started, this, &ACombatPlayerController::OnSkipCinematic);

	UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] Defense input bindings complete"));
}

// ========================================
// DEFENSE INPUT HANDLERS
// ========================================

// Defense input resolves the defender from the DefenseSystem (the player's open-window
// character), NOT GetPawn() — the PC possesses nothing in this combat model. Null defender
// (no window open) is a harmless no-op. SINGLE-TARGET seed; multi-target is Stage 6 (§13).
// B-229: each press goes through RequestDefenseInput, so a client's press reaches the server.
void ACombatPlayerController::OnBlock(const FInputActionValue &Value)
{
	UE_LOG(LogTemp, Log, TEXT("[CombatPC] Guard pressed"));
	RequestDefenseInput(EDefenseType::Block, EDefenseDirection::None);
}

void ACombatPlayerController::OnDodge(const FInputActionValue &Value)
{
	UE_LOG(LogTemp, Log, TEXT("[CombatPC] Dodge pressed"));
	// D-135: one Dodge. The direction only picks the dodge montage (DefenseSystem::PlayDefenseMontage).
	RequestDefenseInput(EDefenseType::Dodge, CombatInputRouting::DODGE_MONTAGE_DIRECTION);
}

// ========================================
// CO-OP INPUT ROUTING (B-229)
// ========================================

void ACombatPlayerController::RequestDefenseInput(EDefenseType DefenseType, EDefenseDirection Direction)
{
	if (WoRAuthority::HasAuthority(this))
	{
		HandleDefenseInput(DefenseType, Direction, CombatInputRouting::STAMP_NOW);
		return;
	}
	// The client's own DefenseSystem has no windows (the server opens them), so the server picks the defender.
	const UWorld *World = GetWorld();
	const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
	const double PressServerTime = GameState ? GameState->GetServerWorldTimeSeconds() : CombatInputRouting::STAMP_NOW;
	Server_SubmitDefenseInput(DefenseType, Direction, PressServerTime);
}

ECombatInputVerdict ACombatPlayerController::HandleDefenseInput(EDefenseType DefenseType, EDefenseDirection Direction,
																double PressServerTime, bool bFromClient)
{
	const FString Description = FString::Printf(TEXT("Defense %s %s"), *UEnum::GetValueAsString(DefenseType),
												*UEnum::GetValueAsString(Direction));
	UDefenseSystem *DefenseSys = GetDefenseSystem();
	// TODO(B-232): the defender this sender controls, not the first open-window player defender.
	AActor *Defender = DefenseSys ? DefenseSys->GetActiveDefenderForLocalPlayer() : nullptr;
	if (!Defender)
	{
		RecordInput(Description, ECombatInputVerdict::NoActor, bFromClient);
		return ECombatInputVerdict::NoActor;
	}
	const FString ForDefender = FString::Printf(TEXT("%s for %s"), *Description, *Defender->GetName());
	if (!ControlsActor(this, Defender))
	{
		RecordInput(ForDefender, ECombatInputVerdict::NotControlled, bFromClient);
		return ECombatInputVerdict::NotControlled;
	}

	double InputTime = CombatInputRouting::STAMP_NOW;
	if (PressServerTime != CombatInputRouting::STAMP_NOW)
	{
		const double Now = UDefenseSystem::GetDefenseClockSeconds(this);
		// A modified client could stamp up to the latency cap early for extra Perfect Guard time: accepted for shared
		// co-op (TODO: B-231 judges remote presses on the server clock).
		InputTime = FMath::Clamp(PressServerTime, Now - CombatInputRouting::MAX_DEFENSE_INPUT_LATENCY_SECONDS, Now);
	}
	DefenseSys->SubmitDefenseInput(Defender, DefenseType, Direction, InputTime);
	RecordInput(ForDefender, ECombatInputVerdict::Accepted, bFromClient);
	return ECombatInputVerdict::Accepted;
}

bool ACombatPlayerController::Server_SubmitDefenseInput_Validate(EDefenseType DefenseType, EDefenseDirection Direction,
																 double PressServerTime)
{
	const UEnum *TypeEnum = StaticEnum<EDefenseType>();
	const UEnum *DirectionEnum = StaticEnum<EDefenseDirection>();
	// IsValidEnumValue also accepts the generated _MAX entry; refuse it.
	return TypeEnum && TypeEnum->IsValidEnumValue(static_cast<int64>(DefenseType)) &&
		   static_cast<int64>(DefenseType) < TypeEnum->GetMaxEnumValue() && DirectionEnum &&
		   DirectionEnum->IsValidEnumValue(static_cast<int64>(Direction)) &&
		   static_cast<int64>(Direction) < DirectionEnum->GetMaxEnumValue() && FMath::IsFinite(PressServerTime);
}

void ACombatPlayerController::Server_SubmitDefenseInput_Implementation(EDefenseType DefenseType,
																	   EDefenseDirection Direction,
																	   double PressServerTime)
{
	HandleDefenseInput(DefenseType, Direction, PressServerTime, true);
}

void ACombatPlayerController::RequestMenuPick(const FCombatPickRequest &Pick)
{
	if (WoRAuthority::HasAuthority(this))
	{
		HandleMenuPick(Pick);
		return;
	}
	UE_LOG(LogWoR, Verbose, TEXT("[CombatInput] sending pick to the server: %s"), *Pick.GetDebugString());
	Server_SubmitMenuPick(Pick);
}

namespace
{
	/** A client's pick may only use a spell or ability its character's loadout offers (any loaded asset otherwise,
	 *  boss skills included). No loadout (headless harness): nothing to check against. */
	bool OffersPickedSkill(const FCombatPickRequest &Pick)
	{
		const ULoadoutComponent *Loadout = Pick.Actor ? Pick.Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
		if (!Loadout)
		{
			return true;
		}
		if (Pick.ActionType == EActionType::Spell)
		{
			return Pick.SpellData && Loadout->GetAvailableSpells().Contains(Pick.SpellData.Get());
		}
		if (Pick.ActionType == EActionType::Ability && Pick.SkillData)
		{
			const USkillDataBase *Skill = Pick.SkillData.Get();
			return Loadout->GetAvailableAbilities().ContainsByPredicate([Skill](const UAbilityData *Each) { return Each == Skill; }) ||
				   Loadout->GetAllWeaponAttacks().Contains(const_cast<USkillDataBase *>(Skill));
		}
		return true; // Defend, items (the executor checks the pouch), a basic attack with no skill
	}
}

ECombatInputVerdict ACombatPlayerController::HandleMenuPick(const FCombatPickRequest &Pick, bool bFromClient)
{
	ECombatInputVerdict Verdict = ECombatInputVerdict::Accepted;
	ACombatOrchestrator *Orchestrator = nullptr;
	if (!Pick.Actor)
	{
		Verdict = ECombatInputVerdict::NoActor;
	}
	else if (!ControlsActor(this, Pick.Actor, bFromClient))
	{
		Verdict = ECombatInputVerdict::NotControlled;
	}
	else
	{
		Orchestrator = FindActiveOrchestrator();
		if (!Orchestrator)
		{
			Verdict = ECombatInputVerdict::NoCombat;
		}
		else if (!Orchestrator->IsActorsTurn(Pick.Actor))
		{
			Verdict = ECombatInputVerdict::NotTheirTurn;
		}
		else if (Orchestrator->IsWaitingForAsyncAction())
		{
			Verdict = ECombatInputVerdict::Busy;
		}
		else if (bFromClient && !OffersPickedSkill(Pick))
		{
			Verdict = ECombatInputVerdict::NotOffered;
		}
	}

	RecordInput(TEXT("Pick ") + Pick.GetDebugString(), Verdict, bFromClient);
	if (Verdict == ECombatInputVerdict::Accepted)
	{
		Orchestrator->SubmitActionAsync(Pick.ToAction());
	}
	return Verdict;
}

void ACombatPlayerController::RequestFreeCommand(AActor *Actor, ECombatFreeCommand Command)
{
	if (WoRAuthority::HasAuthority(this))
	{
		HandleFreeCommand(Actor, Command);
		return;
	}
	UE_LOG(LogWoR, Verbose, TEXT("[CombatInput] sending %s for %s to the server"), *UEnum::GetValueAsString(Command), *GetNameSafe(Actor));
	Server_SubmitFreeCommand(Actor, Command);
}

ECombatInputVerdict ACombatPlayerController::HandleFreeCommand(AActor *Actor, ECombatFreeCommand Command, bool bFromClient)
{
	ECombatInputVerdict Verdict = ECombatInputVerdict::Accepted;
	ACombatOrchestrator *Orchestrator = nullptr;
	if (!Actor)
	{
		Verdict = ECombatInputVerdict::NoActor;
	}
	else if (!ControlsActor(this, Actor, bFromClient))
	{
		Verdict = ECombatInputVerdict::NotControlled;
	}
	else
	{
		Orchestrator = FindActiveOrchestrator();
		if (!Orchestrator)
		{
			Verdict = ECombatInputVerdict::NoCombat;
		}
		else if (!Orchestrator->IsActorsTurn(Actor))
		{
			Verdict = ECombatInputVerdict::NotTheirTurn;
		}
		else if (Orchestrator->IsWaitingForAsyncAction())
		{
			Verdict = ECombatInputVerdict::Busy;
		}
		else if (!ApplyFreeCommand(Orchestrator, Actor, Command))
		{
			Verdict = ECombatInputVerdict::NotOffered;
		}
	}
	RecordInput(FString::Printf(TEXT("%s for %s"), *UEnum::GetValueAsString(Command), *GetNameSafe(Actor)), Verdict, bFromClient);
	return Verdict;
}

bool ACombatPlayerController::ApplyFreeCommand(ACombatOrchestrator *Orchestrator, AActor *Actor, ECombatFreeCommand Command)
{
	FText Refusal;
	ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	switch (Command)
	{
	case ECombatFreeCommand::SwitchWeapon:
		return Loadout && Loadout->TrySwitchActiveSlot(EGearKind::Weapon, Refusal);
	case ECombatFreeCommand::SwitchAdapter:
		return Loadout && Loadout->TrySwitchActiveSlot(EGearKind::Adapter, Refusal);
	case ECombatFreeCommand::MoveRowForward:
	case ECombatFreeCommand::MoveRowBack:
		return Orchestrator && Orchestrator->TryMoveRow(Actor, Command == ECombatFreeCommand::MoveRowForward, Refusal);
	case ECombatFreeCommand::HandToAI:
		return Orchestrator && Orchestrator->SwitchMemberControl(Actor);
	default:
		return false;
	}
}

bool ACombatPlayerController::Server_SubmitFreeCommand_Validate(AActor *Actor, ECombatFreeCommand Command)
{
	return Command < ECombatFreeCommand::MAX;
}

void ACombatPlayerController::Server_SubmitFreeCommand_Implementation(AActor *Actor, ECombatFreeCommand Command)
{
	const ECombatInputVerdict Verdict = HandleFreeCommand(Actor, Command, true);
	if (Verdict == ECombatInputVerdict::Accepted)
	{
		Client_FreeCommandApplied(Actor, Command); // a refused step changes nothing; the client's menu stays as it is
	}
	else if (Command == ECombatFreeCommand::HandToAI)
	{
		Client_MenuPickRefused(Verdict); // review fix: the client closed its menu for the hand-off; it reopens
	}
}

void ACombatPlayerController::Client_FreeCommandApplied_Implementation(AActor *Actor, ECombatFreeCommand Command)
{
	// The loadout's active slot is not replicated: the client's copy takes the same step, then the menu redraws.
	// The row and the AI hand-off live on the server (the orchestrator); the menu reads them from the turn state.
	if (Command == ECombatFreeCommand::SwitchWeapon || Command == ECombatFreeCommand::SwitchAdapter)
	{
		// Review fix: the server approved it, so the client's copy takes it even if its own turn flag lags.
		if (ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr)
		{
			Loadout->ResetTurnSwitch();
		}
		if (!ApplyFreeCommand(nullptr, Actor, Command))
		{
			UE_LOG(LogWoRCombat, Warning, TEXT("[Net] Client copy refused a switch the server applied for %s: the loadouts differ"),
				   *GetNameSafe(Actor));
		}
	}
	if (UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(this))
	{
		Menu->HandleFreeCommandApplied(Actor, Command);
	}
}

bool ACombatPlayerController::Server_SubmitMenuPick_Validate(const FCombatPickRequest &Pick)
{
	return Pick.HasValidEnums();
}

void ACombatPlayerController::Server_SubmitMenuPick_Implementation(const FCombatPickRequest &Pick)
{
	const ECombatInputVerdict Verdict = HandleMenuPick(Pick, true);
	if (Verdict != ECombatInputVerdict::Accepted)
	{
		Client_MenuPickRefused(Verdict); // B-232b: the client's menu closed on sending; it reopens for this turn
	}
}

void ACombatPlayerController::Client_MenuPickRefused_Implementation(ECombatInputVerdict Verdict)
{
	UE_LOG(LogTemp, Log, TEXT("[CombatPlayerController] The server refused the menu pick (%s); reopening the menu"),
		   *UEnum::GetValueAsString(Verdict));
	if (UTurnStateComponent *TurnState = UTurnStateComponent::Find(this))
	{
		TurnState->ReopenClientCommandMenu();
	}
}

bool ACombatPlayerController::ControlsActor(const AController *Sender, const AActor *Actor, bool bFromClient)
{
	if (!Sender || !Actor)
	{
		return false;
	}
	if (Sender->GetPawn() == Actor || Actor->GetOwner() == Sender)
	{
		return true;
	}
	// B-232 (Q-21-audit): a roster member follows the seat rule; Q-229b's shared control ends for the roster.
	if (const URunStateComponent *Run = URunStateComponent::Find(Sender); Run && Run->FindRosterEntry(Actor))
	{
		return Run->IsCommandedBy(Actor, Sender);
	}
	// Off the roster (no GameState run state, a test actor): any player-driven combatant. A non-pawn combatant (the test
	// harness's) counts as player-driven, as in the orchestrator, but only for a pick made here: a remote client may
	// only name a pawn (B-229).
	const APawn *Pawn = Cast<APawn>(Actor);
	return Pawn ? CombatControl::IsPlayerDriven(Pawn) : !bFromClient;
}

ACombatPlayerController *ACombatPlayerController::FindLocal(const UObject *WorldContext)
{
	UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	return GI ? Cast<ACombatPlayerController>(GI->GetFirstLocalPlayerController(World)) : nullptr;
}

ACombatOrchestrator *ACombatPlayerController::FindActiveOrchestrator() const
{
	for (TActorIterator<ACombatOrchestrator> It(GetWorld()); It; ++It)
	{
		if (It->GetCombatState() == ECombatState::InProgress)
		{
			return *It;
		}
	}
	return nullptr;
}

void ACombatPlayerController::RecordInput(const FString &Description, ECombatInputVerdict Verdict, bool bFromClient)
{
	FCombatInputRecord Record;
	Record.Description = Description;
	Record.Verdict = Verdict;
	Record.bFromClient = bFromClient;
	Record.ServerTime = UDefenseSystem::GetDefenseClockSeconds(this);
	if (RecentInputs.Num() >= CombatInputRouting::MAX_RECENT_INPUTS)
	{
		RecentInputs.RemoveAt(0);
	}
	RecentInputs.Add(Record);
	UE_LOG(LogWoR, Log, TEXT("[CombatInput] %s %s %s: %s"), *GetName(),
		   bFromClient ? TEXT("client RPC") : TEXT("local"), *Description, *UEnum::GetValueAsString(Verdict));
}

FString ACombatPlayerController::GetInputDebugString() const
{
	FString Out = FString::Printf(TEXT("[CombatInput] %s (%s), %d recent input(s)"), *GetName(),
								  IsLocalController() ? TEXT("local") : TEXT("remote"), RecentInputs.Num());
	for (const FCombatInputRecord &Record : RecentInputs)
	{
		Out += FString::Printf(TEXT("\n  %.2fs %s %s -> %s"), Record.ServerTime,
							   Record.bFromClient ? TEXT("[client]") : TEXT("[local]"), *Record.Description,
							   *UEnum::GetValueAsString(Record.Verdict));
	}
	return Out;
}

// ========================================
// MENU KEYS (B-093)
// ========================================

void ACombatPlayerController::BuildMenuKeys()
{
	if (MenuKeysContext)
	{
		return;
	}
	MenuKeysContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Combat_MenuKeys"));
	for (const CombatMenuKeys::FBinding &Binding : CombatMenuKeys::GetBindings())
	{
		UInputAction *Action = NewObject<UInputAction>(this, Binding.ActionName);
		Action->ValueType = EInputActionValueType::Boolean;
		// Don't eat the key: the context stays mapped all fight, and a defense key sharing it
		// (priority 0) must still reach the defense window on an enemy's turn.
		Action->bConsumeInput = false;
		MenuKeysContext->MapKey(Action, Binding.Keyboard);
		MenuKeysContext->MapKey(Action, Binding.Gamepad);
		switch (Binding.Category)
		{
		case EPieMenuCategory::Defend:
			DefendAction = Action;
			break;
		case EPieMenuCategory::CycleSource:
			InfusionAction = Action;
			break;
		case EPieMenuCategory::CycleLevel:
			InfusionLevelAction = Action;
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	// B-406b: the boss-cinematic skip, in the same always-on context (it does nothing when no cinematic plays).
	SkipCinematicAction = NewObject<UInputAction>(this, CombatMenuKeys::SKIP_CINEMATIC_ACTION_NAME);
	SkipCinematicAction->ValueType = EInputActionValueType::Boolean;
	SkipCinematicAction->bConsumeInput = false;
	MenuKeysContext->MapKey(SkipCinematicAction, CombatMenuKeys::GetSkipCinematicKeyboard());
	MenuKeysContext->MapKey(SkipCinematicAction, CombatMenuKeys::GetSkipCinematicGamepad());
}

void ACombatPlayerController::OnSkipCinematic(const FInputActionValue &Value)
{
	if (!IsLocalController())
	{
		return;
	}
	SkipBossCinematic(this);
}

bool ACombatPlayerController::SkipBossCinematic(const UObject *WorldContext)
{
	UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ACombatOrchestrator> It(World); It; ++It)
	{
		UCombatCameraDirector *Director = It->GetCameraDirector();
		if (It->GetCombatState() == ECombatState::InProgress && Director && Director->IsBossCinematicPlaying())
		{
			// Q-406b: this machine's view only. On a co-op client the server's hold ends on its own end or the cap.
			UE_LOG(LogWoRCombat, Log, TEXT("[CombatPC] Boss cinematic skipped (%s)"),
				   *UEnum::GetValueAsString(Director->GetBossCinematicKind()));
			Director->FinishBossCinematic();
			return true;
		}
	}
	return false;
}

void ACombatPlayerController::OnDefend(const FInputActionValue &Value) { PressMenuKey(EPieMenuCategory::Defend); }

void ACombatPlayerController::OnInfusion(const FInputActionValue &Value) { PressMenuKey(EPieMenuCategory::CycleSource); }

void ACombatPlayerController::OnInfusionLevel(const FInputActionValue &Value) { PressMenuKey(EPieMenuCategory::CycleLevel); }

void ACombatPlayerController::PressMenuKey(EPieMenuCategory Category)
{
	if (!IsLocalController())
	{
		return;
	}
	UGameInstance *GI = GetGameInstance();
	if (UCombatCommandMenuSubsystem *Menu = WoRFight::Get<UCombatCommandMenuSubsystem>(GI))
	{
		Menu->PressMenuKey(Category);
	}
}

FString ACombatPlayerController::GetMenuKeysString()
{
	FString Out = TEXT("[CombatPC] Menu keys");
	for (const CombatMenuKeys::FBinding &Binding : CombatMenuKeys::GetBindings())
	{
		Out += FString::Printf(TEXT("\n  %s (%s): %s / %s"), *Binding.ActionName.ToString(),
							   *UEnum::GetValueAsString(Binding.Category), *Binding.Keyboard.GetDisplayName().ToString(),
							   *Binding.Gamepad.GetDisplayName().ToString());
	}
	Out += FString::Printf(TEXT("\n  %s (skip boss cinematic, B-406b): %s / %s"), CombatMenuKeys::SKIP_CINEMATIC_ACTION_NAME,
						   *CombatMenuKeys::GetSkipCinematicKeyboard().GetDisplayName().ToString(),
						   *CombatMenuKeys::GetSkipCinematicGamepad().GetDisplayName().ToString());
	return Out;
}

// ========================================
// HELPERS
// ========================================

UDefenseSystem *ACombatPlayerController::GetDefenseSystem() const
{
	if (UGameInstance *GI = GetGameInstance())
	{
		return WoRFight::Get<UDefenseSystem>(GI);
	}
	return nullptr;
}
