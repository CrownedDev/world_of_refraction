// CombatOrchestratorDebug.cpp
// B-033 (move-only): ACombatOrchestrator's debug buttons and commands, moved out of
// CombatOrchestrator.cpp unchanged. Declarations stay on the class (CallInEditor / BlueprintCallable).

#include "Combat/CombatOrchestrator.h"
#include "GameFramework/Pawn.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Infusion/InfusionChargeManager.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"   // WORLDSTAT_KILL_DIVISOR (§7 C3 earn faucet)
#include "EngineUtils.h"              // TActorIterator for the wor.* world-stat debug commands
#include "Kismet/GameplayStatics.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Combat/FleeRules.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Combat/Grid/CombatGridSubsystem.h"
#include "UI/Combat/CombatHUD.h"
#include "Combat/Camera/CombatCameraDirector.h"
#include "GameFramework/PlayerController.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Combat/Mechanics/BattleRandomSubsystem.h"
#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Skills/Effects/FSkillEffect.h"
#include "Skills/Effects/FGatheredEffect.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Durability/DurabilityConstants.h"
#include "Equipment/Durability/CrystalBreak.h"
#include "Inventory/ItemEffectType.h"
#include "Combat/Mechanics/WeatherStateManager.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "Equipment/Crystals/CrystalManager.h"
#include "TimerManager.h"
#include "Combat/CombatControl.h"
#include "Party/PartySessionSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace CombatDebugAssetFallback
{
	// OBSOLETE (B-118): the debug commands below used to load a hard-coded test asset when the
	// actor had no skill (one path is not in git). Off by default; remove this cvar and the
	// paths after Crown's PIE pass.
	TAutoConsoleVariable<bool> CVarAllowTestAssetFallback(
		TEXT("wor.Debug.AllowTestAssetFallback"), false,
		TEXT("OBSOLETE (B-118): let combat debug commands load a hard-coded test skill when the actor has none."));

	/** The test asset when the obsolete fallback is on; otherwise null (the command logs the error). */
	template <class TSkill>
	TSkill *LoadTestSkill(const TCHAR *Path)
	{
		return CVarAllowTestAssetFallback.GetValueOnGameThread() ? LoadObject<TSkill>(nullptr, Path) : nullptr;
	}
}

void ACombatOrchestrator::DebugApplyPendingWorldStats()
{
	// DEBUG-ONLY placeholder — dump the whole pending pool into Mind on the player team (LocalParty), to
	// prove the pool->grant pipe in PIE. The REAL draft is the deferred 5-pick-3 UI on
	// OnWorldStatDraftReady. Falls back to the first player pawn if LocalParty has emptied (post-combat).
	int32 Applied = 0;
	auto ApplyTo = [this, &Applied](AActor *Actor)
	{
		if (UCharacterDataComponent *Comp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
		{
			Comp->AddEarnedWorldStat(EWorldPillar::Mind, PendingWorldStatPool);
			++Applied;
		}
	};
	if (LocalPartyCombatants.Num() > 0)
	{
		for (AActor *Actor : LocalPartyCombatants)
		{
			ApplyTo(Actor);
		}
	}
	else if (UWorld *World = GetWorld())
	{
		if (APlayerController *PC = World->GetFirstPlayerController())
		{
			ApplyTo(PC->GetPawn());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("[CombatOrchestrator] DEBUG: applied pending pool %d to Mind on %d actor(s)"),
		   PendingWorldStatPool, Applied);
	PendingWorldStatPool = 0; // consumed
}

void ACombatOrchestrator::DebugPrintWorldStats() const
{
	UE_LOG(LogTemp, Display, TEXT("[CombatOrchestrator] Pending WSP pool: %d"), PendingWorldStatPool);
	for (AActor *Actor : LocalPartyCombatants)
	{
		if (const UCharacterDataComponent *Comp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr)
		{
			UE_LOG(LogTemp, Display, TEXT("  %s: Mind=%d Body=%d Spirit=%d"),
				   *Actor->GetName(), Comp->GetWorldMind(), Comp->GetWorldBody(), Comp->GetWorldSpirit());
		}
	}
}

// ---- DEBUG console commands (PIE) — resolve the first ACombatOrchestrator in the world ----
#if !UE_BUILD_SHIPPING
namespace
{
	ACombatOrchestrator *FindCombatOrchestrator(UWorld *World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ACombatOrchestrator> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	FAutoConsoleCommandWithWorld GGrantPendingWorldStatsCmd(
		TEXT("wor.GrantPendingWorldStats"),
		TEXT("DEBUG: apply the combat's pending World Stat pool into the player team's Mind (placeholder for the deferred draft UI)."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				if (ACombatOrchestrator *Orch = FindCombatOrchestrator(World))
				{
					Orch->DebugApplyPendingWorldStats();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] wor.GrantPendingWorldStats: no orchestrator in world."));
				}
			}));

	FAutoConsoleCommandWithWorld GSwitchControlCmd(
		TEXT("wor.Combat.SwitchControl"),
		TEXT("Switch the party member whose turn it is between you and the AI (D-026). Placeholder for taking over an AI member until the new HUD."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				if (ACombatOrchestrator *Orch = FindCombatOrchestrator(World))
				{
					Orch->SwitchMemberControl(Orch->GetCurrentActor());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] wor.Combat.SwitchControl: no orchestrator in world."));
				}
			}));

	FAutoConsoleCommandWithWorld GPrintCombatStateCmd(
		TEXT("wor.Combat.PrintState"),
		TEXT("DEBUG: print the fight's state: turn, current actor, both teams' HP/EP and who plays each of your members."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				if (ACombatOrchestrator *Orch = FindCombatOrchestrator(World))
				{
					Orch->DebugPrintCombatState();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] wor.Combat.PrintState: no orchestrator in world."));
				}
			}));

	FAutoConsoleCommandWithWorld GPrintWorldStatsCmd(
		TEXT("wor.PrintWorldStats"),
		TEXT("DEBUG: print the pending World Stat pool + each player-team member's live Mind/Body/Spirit."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				if (ACombatOrchestrator *Orch = FindCombatOrchestrator(World))
				{
					Orch->DebugPrintWorldStats();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] wor.PrintWorldStats: no orchestrator in world."));
				}
			}));
}
#endif // !UE_BUILD_SHIPPING

// ========================================
// DEBUG TOOLS
// ========================================

void ACombatOrchestrator::DebugPrintCombatState()
{
	UE_LOG(LogTemp, Display, TEXT("=== COMBAT ORCHESTRATOR STATE ==="));
	UE_LOG(LogTemp, Display, TEXT("Combat State: %d"), (int32)CombatState);
	UE_LOG(LogTemp, Display, TEXT("Current Turn: %d"), CurrentTurnNumber);
	UE_LOG(LogTemp, Display, TEXT("Current Actor: %s"), CurrentActor ? *CurrentActor->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Display, TEXT("Row move used this turn: %s"), bRowMoveUsedThisTurn ? TEXT("yes") : TEXT("no"));
	UE_LOG(LogTemp, Display, TEXT("Auto-Advance: %s (%.1fs delay)"),
		   bAutoAdvanceTurns ? TEXT("ON") : TEXT("OFF"), AutoAdvanceDelay);
	UE_LOG(LogTemp, Display, TEXT("Waiting for Async: %s"), bWaitingForAsyncAction ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogTemp, Display, TEXT("Encounter Rank: %s (flee %s)"), *UEnum::GetValueAsString(EncounterRank),
		   FleeRules::CanFleeRank(EncounterRank) ? TEXT("allowed") : TEXT("refused"));

	UE_LOG(LogTemp, Display, TEXT("\nTeam 0 (%d members):"), LocalPartyCombatants.Num());
	for (AActor *Actor : LocalPartyCombatants)
	{
		UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - HP: %d/%d, EP: %d/%d, Alive: %s, Played by: %s"),
				   *Actor->GetName(),
				   Comp->CurrentHP, Comp->MaxHP,
				   Comp->CurrentEP, Comp->MaxEP,
				   Comp->bIsAlive ? TEXT("Yes") : TEXT("No"),
				   SkillEffectManagerRef && SkillEffectManagerRef->HasEffectOfType(Actor, ESkillEffectType::RandomSkill)
					   ? TEXT("Void break (random)")
				   : IsActorAIControlled(Actor) ? TEXT("AI")
												: TEXT("you"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - (no CharacterDataComponent)"), *Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("\nTeam 1 (%d members):"), OpposingPartyCombatants.Num());
	for (AActor *Actor : OpposingPartyCombatants)
	{
		UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - HP: %d/%d, EP: %d/%d, Alive: %s"),
				   *Actor->GetName(),
				   Comp->CurrentHP, Comp->MaxHP,
				   Comp->CurrentEP, Comp->MaxEP,
				   Comp->bIsAlive ? TEXT("Yes") : TEXT("No"));
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("  %s - (no CharacterDataComponent)"), *Actor->GetName());
		}
	}

	UE_LOG(LogTemp, Display, TEXT("================================="));
}
void ACombatOrchestrator::DebugDamageLocalParty()
{
	for (AActor *Actor : LocalPartyCombatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerTakeDamage(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugDamageOpposingParty()
{
	for (AActor *Actor : OpposingPartyCombatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerTakeDamage(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugSpendEPLocalParty()
{
	for (AActor *Actor : LocalPartyCombatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerSpendEnergy(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugSpendEPOpposingParty()
{
	for (AActor *Actor : OpposingPartyCombatants)
	{
		if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
			Comp->ServerSpendEnergy(DebugDamageAmount);
	}
}

void ACombatOrchestrator::DebugApplyStatusBuildup()
{
	UStatusBuildupManager *BuildupManager = GetGameInstance()->GetSubsystem<UStatusBuildupManager>();
	if (!BuildupManager)
		return;

	// Session Y: manager resolves trigger from (Element, PhysicalType).
	// Fire + None → DOT (same trigger this debug menu used pre-Y).
	for (AActor *Actor : LocalPartyCombatants)
		BuildupManager->AddStatusBuildup(nullptr, Actor, DebugStatusBuildupAmount, ESpellElement::Fire, EPhysicalDamageType::None);

	for (AActor *Actor : OpposingPartyCombatants)
		BuildupManager->AddStatusBuildup(nullptr, Actor, DebugStatusBuildupAmount, ESpellElement::Fire, EPhysicalDamageType::None);
}

void ACombatOrchestrator::DebugKillActor(AActor *Actor)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugKillActor: null actor"));
		return;
	}

	UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (Comp)
	{
		// ServerTakeDamage -> CheckDeath -> OnDied; TurnManager's per-combat bind
		// hears the broadcast — no explicit OnActorDied call (was a double-notify).
		Comp->ServerTakeDamage(9999);
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Killed %s"), *Actor->GetName());

		// Update facing for remaining actors
		UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>();
		if (Grid)
		{
			Grid->UpdateAllActorFacing(GetActorLocation());
		}
	}
}

void ACombatOrchestrator::DebugHealAllTeam(int32 TeamIndex)
{
	TArray<AActor *> &Team = (TeamIndex == 0) ? LocalPartyCombatants : OpposingPartyCombatants;

	for (AActor *Actor : Team)
	{
		UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
		if (Comp)
		{
			if (!Comp->bIsAlive)
			{
				// ServerResurrect broadcasts OnResurrected; TurnManager's bind hears it.
				Comp->ServerResurrect(100);
			}
			else
			{
				Comp->ServerHeal(Comp->MaxHP);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Healed all Team %d members"), TeamIndex);
}

void ACombatOrchestrator::DebugExecuteTestAction()
{
	if (CombatState != ECombatState::InProgress || !CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteTestAction: No active turn"));
		return;
	}

	// Find a living enemy target
	TArray<AActor *> Enemies = GetEnemyTeam(CurrentActor);
	AActor *Target = nullptr;
	for (AActor *Enemy : Enemies)
	{
		if (IsActorAlive(Enemy))
		{
			Target = Enemy;
			break;
		}
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteTestAction: No living enemy targets"));
		OnActionCompleted();
		return;
	}

	// Create a Defend action (simplest - no data asset needed)
	FAction TestAction;
	TestAction.ActionType = EActionType::Defend;

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: %s executing Defend action"),
		   *CurrentActor->GetName());

	SubmitActionAsync(TestAction);
}

void ACombatOrchestrator::DebugTestAttackMovement()
{
	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAttackMovement: No active turn"));
		return;
	}

	// Find target from opposing team (B-256: GetDebugOpponent)
	AActor *Target = GetDebugOpponent(Actor);

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAttackMovement: No valid target"));
		return;
	}

	// Build attack action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Ability; // attack/ability merge: attacks dispatch as Ability (SkillData + IsAttack)
	AttackAction.Targets.Add(Target);

	// Get weapon attack data via GetActiveWeapon (respects bShowPrimary)
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		UWeaponData *ActiveWeapon = Loadout->GetActiveWeapon();
		if (ActiveWeapon && ActiveWeapon->WeaponAttack)
		{
			AttackAction.SkillData = ActiveWeapon->WeaponAttack;
			UE_LOG(LogTemp, Log, TEXT("[DebugTestAttackMovement] Using attack: %s from weapon: %s"),
				   *ActiveWeapon->WeaponAttack->Name, *ActiveWeapon->Name);
		}
	}

	if (!AttackAction.SkillData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestAttackMovement] No weapon attack data found on %s"),
			   *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestAttackMovement] %s attacking %s"),
		   *Actor->GetName(), *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		ActionExecutorRef->ExecuteActionAsync(Actor, AttackAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}
void ACombatOrchestrator::DebugTestAbilityMovement()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAbilityMovement: No actor available"));
		return;
	}

	// Find target from opposing team (B-256: GetDebugOpponent)
	AActor *Target = GetDebugOpponent(Actor);

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestAbilityMovement: No valid target"));
		return;
	}

	// Build ability action
	FAction AbilityAction;
	AbilityAction.ActionType = EActionType::Ability;
	AbilityAction.Targets.Add(Target);

	// Get ability from LoadoutComponent
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<UAbilityData *> AvailableAbilities = Loadout->GetAvailableAbilities();
		if (AvailableAbilities.Num() > 0)
		{
			AbilityAction.SkillData = AvailableAbilities[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestAbilityMovement] Using ability: %s"),
				   *AbilityAction.SkillData->Name);
		}
	}

	// OBSOLETE (B-118): test-asset fallback, only with wor.Debug.AllowTestAssetFallback 1
	if (!AbilityAction.SkillData)
	{
		AbilityAction.SkillData = CombatDebugAssetFallback::LoadTestSkill<UAbilityData>(
			TEXT("/Game/Testing/Weapons/Abilities/DA_Test_Ability.DA_Test_Ability"));
	}

	if (!AbilityAction.SkillData)
	{
		UE_LOG(LogWoRCombat, Error, TEXT("[DebugTestAbilityMovement] No ability available on %s - give it one in its loadout (B-118)"),
			   *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestAbilityMovement] %s using %s on %s"),
		   *Actor->GetName(), *AbilityAction.SkillData->Name, *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, AbilityAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestSpellMovement()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestSpellMovement: No actor available"));
		return;
	}

	// Find target from opposing team (B-256: GetDebugOpponent)
	AActor *Target = GetDebugOpponent(Actor);

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugTestSpellMovement: No valid target"));
		return;
	}

	// Build spell action
	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.Targets.Add(Target);

	// Get spell from active slot (respects bShowPrimary)
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> ActiveSpells = Loadout->GetActiveSlotSpells();
		if (ActiveSpells.Num() > 0)
		{
			SpellAction.SpellData = ActiveSpells[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestSpellMovement] Using spell: %s (Element: %s)"),
				   *SpellAction.SpellData->Name,
				   *UEnum::GetValueAsString(SpellAction.SpellData->Element));
		}
		else
		{
			// Fallback to all available spells
			TArray<USpellData *> AllSpells = Loadout->GetAvailableSpells();
			if (AllSpells.Num() > 0)
			{
				SpellAction.SpellData = AllSpells[0];
				UE_LOG(LogTemp, Log, TEXT("[DebugTestSpellMovement] Fallback spell: %s"),
					   *SpellAction.SpellData->Name);
			}
		}
	}

	// OBSOLETE (B-118): test-asset fallback, only with wor.Debug.AllowTestAssetFallback 1
	if (!SpellAction.SpellData)
	{
		SpellAction.SpellData = CombatDebugAssetFallback::LoadTestSkill<USpellData>(
			TEXT("/Game/Data/Spells/Fire/DA_Spell_Fireball.DA_Spell_Fireball"));
	}

	if (!SpellAction.SpellData)
	{
		UE_LOG(LogWoRCombat, Error, TEXT("[DebugTestSpellMovement] No spell available on %s - give it one in its loadout (B-118)"),
			   *Actor->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DebugTestSpellMovement] %s casting %s on %s"),
		   *Actor->GetName(), *SpellAction.SpellData->Name, *Target->GetName());

	// Execute through ActionExecutor with callback
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestItemOnEnemy()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No actor available"));
		return;
	}

	// Find enemy target
	AActor *Target = nullptr;
	bool bActorInLocalParty = LocalPartyCombatants.Contains(Actor);

	if (bActorInLocalParty && OpposingPartyCombatants.Num() > 0)
	{
		Target = OpposingPartyCombatants[0];
	}
	else if (!bActorInLocalParty && LocalPartyCombatants.Num() > 0)
	{
		Target = LocalPartyCombatants[0];
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No valid target"));
		return;
	}

	// Get loadout and find damage item (Garnet)
	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No LoadoutComponent on %s"), *Actor->GetName());
		return;
	}

	TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
	FCrystalId DamageId;
	bool bFoundDamage = false;

	for (const FItemLoadoutSlot &Slot : UsableItems)
	{
		if (!Slot.IsEmpty() && ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::Damage)
		{
			DamageId = Slot.CrystalId;
			bFoundDamage = true;
			break;
		}
	}

	// Fallback: a literal Garnet F when the loadout has no damage item.
	if (!bFoundDamage)
	{
		DamageId = FCrystalId{ECrystalType::Garnet, EItemTier::F_Tier};
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnEnemy] No damage item in loadout, using fallback Garnet F"));
	}

	// Build item action
	FAction ItemAction;
	ItemAction.ActionType = EActionType::Item;
	ItemAction.ItemData = DamageId;
	ItemAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugTestItemOnEnemy] %s using %s on enemy %s"),
		   *Actor->GetName(), *ItemIdentity::GetDisplayName(DamageId), *Target->GetName());

	// Execute
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, ItemAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestItemOnSelf()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnSelf] No actor available"));
		return;
	}

	// Get loadout and find healing item (Sapphire)
	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnSelf] No LoadoutComponent on %s"), *Actor->GetName());
		return;
	}

	TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
	FCrystalId HealId;
	bool bFoundHeal = false;

	for (const FItemLoadoutSlot &Slot : UsableItems)
	{
		// AI-1: the heal is the Healing Stone (RestoreHealth); Sapphire moved to DefyDeath.
		if (!Slot.IsEmpty() && ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::RestoreHealth)
		{
			HealId = Slot.CrystalId;
			bFoundHeal = true;
			break;
		}
	}

	// Fallback: a literal Healing Stone F when the loadout has no healing item (AI-1: the heal is
	// the Healing Stone now; Sapphire is defy-death).
	if (!bFoundHeal)
	{
		HealId = FCrystalId{ECrystalType::HealingStone, EItemTier::F_Tier};
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnSelf] No healing item in loadout, using fallback Healing Stone F"));
	}

	// Build item action - target is self
	FAction ItemAction;
	ItemAction.ActionType = EActionType::Item;
	ItemAction.ItemData = HealId;
	ItemAction.Targets.Add(Actor); // Self-target

	UE_LOG(LogTemp, Log, TEXT("[DebugTestItemOnSelf] %s using %s on SELF"),
		   *Actor->GetName(), *ItemIdentity::GetDisplayName(HealId));

	// Execute
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, ItemAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestItemOnAlly()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No actor available"));
		return;
	}

	// Find ally target (same team, different actor)
	AActor *Ally = nullptr;
	bool bActorInLocalParty = LocalPartyCombatants.Contains(Actor);
	TArray<AActor *> &TeamArray = bActorInLocalParty ? LocalPartyCombatants : OpposingPartyCombatants;

	for (AActor *Teammate : TeamArray)
	{
		if (Teammate != Actor && IsActorAlive(Teammate))
		{
			Ally = Teammate;
			break;
		}
	}

	if (!Ally)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No ally available (need 2+ team members)"));
		return;
	}

	// Get loadout and find healing item (Sapphire)
	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No LoadoutComponent on %s"), *Actor->GetName());
		return;
	}

	TArray<FItemLoadoutSlot> UsableItems = Loadout->GetUsableItems();
	FCrystalId HealId;
	bool bFoundHeal = false;

	for (const FItemLoadoutSlot &Slot : UsableItems)
	{
		// AI-1: the heal is the Healing Stone (RestoreHealth); Sapphire moved to DefyDeath.
		if (!Slot.IsEmpty() && ItemIdentity::GetItemEffectType(Slot.CrystalId) == EItemEffectType::RestoreHealth)
		{
			HealId = Slot.CrystalId;
			bFoundHeal = true;
			break;
		}
	}

	// Fallback: a literal Healing Stone F when the loadout has no healing item (AI-1: the heal is
	// the Healing Stone now; Sapphire is defy-death).
	if (!bFoundHeal)
	{
		HealId = FCrystalId{ECrystalType::HealingStone, EItemTier::F_Tier};
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestItemOnAlly] No healing item in loadout, using fallback Healing Stone F"));
	}

	// Build item action - target is ally
	FAction ItemAction;
	ItemAction.ActionType = EActionType::Item;
	ItemAction.ItemData = HealId;
	ItemAction.Targets.Add(Ally);

	UE_LOG(LogTemp, Log, TEXT("[DebugTestItemOnAlly] %s using %s on ally %s"),
		   *Actor->GetName(), *ItemIdentity::GetDisplayName(HealId), *Ally->GetName());

	// Execute
	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, ItemAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugExecuteAsyncAttack()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAttack: No actor available"));
		return;
	}

	// Find target from opposing team (B-256: GetDebugOpponent)
	AActor *Target = GetDebugOpponent(Actor);

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAttack: No valid target"));
		return;
	}

	// Get weapon attack data via GetActiveWeapon (respects bShowPrimary)
	USkillDataBase *AttackData = nullptr;
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		UWeaponData *ActiveWeapon = Loadout->GetActiveWeapon();
		if (ActiveWeapon && ActiveWeapon->WeaponAttack)
		{
			AttackData = ActiveWeapon->WeaponAttack;
		}
	}
	if (!AttackData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugExecuteAsyncAttack] No weapon attack data on %s"),
			   *Actor->GetName());
		return;
	}

	// Build action
	FAction AttackAction;
	AttackAction.ActionType = EActionType::Ability; // attack/ability merge: attacks dispatch as Ability (SkillData + IsAttack)
	AttackAction.SkillData = AttackData;
	AttackAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteAsyncAttack] %s attacking %s with %s"),
		   *Actor->GetName(), *Target->GetName(), *AttackData->Name);

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, AttackAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugExecuteAsyncSpell()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncSpell: No actor available"));
		return;
	}

	// Find target from opposing team (B-256: GetDebugOpponent)
	AActor *Target = GetDebugOpponent(Actor);

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncSpell: No valid target"));
		return;
	}

	// Try to get a spell from LoadoutComponent
	USpellData *SpellData = nullptr;
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> AvailableSpells = Loadout->GetAvailableSpells();
		if (AvailableSpells.Num() > 0)
		{
			SpellData = AvailableSpells[0];
		}
	}

	// OBSOLETE (B-118): test-asset fallback, only with wor.Debug.AllowTestAssetFallback 1
	if (!SpellData)
	{
		SpellData = CombatDebugAssetFallback::LoadTestSkill<USpellData>(
			TEXT("/Game/Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall.DA_Spells_Fire_FireBall"));
	}

	if (!SpellData)
	{
		UE_LOG(LogWoRCombat, Error, TEXT("[DebugExecuteAsyncSpell] No spell available - give the actor one in its loadout (B-118)"));
		return;
	}

	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.SpellData = SpellData;
	SpellAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteAsyncSpell] %s casting %s on %s"),
		   *Actor->GetName(), *SpellData->Name, *Target->GetName());

	UE_LOG(LogTemp, Warning, TEXT("[DEBUG] Spell: %s | TargetType: %d | DeliveryType: %d | CastAnim: %s"),
		   *SpellData->Name,
		   (int32)SpellData->TargetType,
		   (int32)SpellData->DeliveryType,
		   SpellData->CastAnimation ? *SpellData->CastAnimation->GetName() : TEXT("NONE"));

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestPrimarySpell()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestPrimarySpell] No actor available"));
		return;
	}

	AActor *Target = nullptr;
	bool bActorInLocalParty = LocalPartyCombatants.Contains(Actor);
	if (bActorInLocalParty && OpposingPartyCombatants.Num() > 0)
		Target = OpposingPartyCombatants[0];
	else if (!bActorInLocalParty && LocalPartyCombatants.Num() > 0)
		Target = LocalPartyCombatants[0];

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestPrimarySpell] No valid target"));
		return;
	}

	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.Targets.Add(Target);

	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> PrimarySpells = Loadout->GetPrimarySlotSpells();
		if (PrimarySpells.Num() > 0)
		{
			SpellAction.SpellData = PrimarySpells[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestPrimarySpell] Using PRIMARY spell: %s (Element: %s)"),
				   *SpellAction.SpellData->Name,
				   *UEnum::GetValueAsString(SpellAction.SpellData->Element));
		}
	}

	if (!SpellAction.SpellData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestPrimarySpell] No PRIMARY slot spells on %s"), *Actor->GetName());
		return;
	}

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugTestSecondarySpell()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestSecondarySpell] No actor available"));
		return;
	}

	AActor *Target = nullptr;
	bool bActorInLocalParty = LocalPartyCombatants.Contains(Actor);
	if (bActorInLocalParty && OpposingPartyCombatants.Num() > 0)
		Target = OpposingPartyCombatants[0];
	else if (!bActorInLocalParty && LocalPartyCombatants.Num() > 0)
		Target = LocalPartyCombatants[0];

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestSecondarySpell] No valid target"));
		return;
	}

	FAction SpellAction;
	SpellAction.ActionType = EActionType::Spell;
	SpellAction.Targets.Add(Target);

	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<USpellData *> SecondarySpells = Loadout->GetSecondarySlotSpells();
		if (SecondarySpells.Num() > 0)
		{
			SpellAction.SpellData = SecondarySpells[0];
			UE_LOG(LogTemp, Log, TEXT("[DebugTestSecondarySpell] Using SECONDARY spell: %s (Element: %s)"),
				   *SpellAction.SpellData->Name,
				   *UEnum::GetValueAsString(SpellAction.SpellData->Element));
		}
	}

	if (!SpellAction.SpellData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DebugTestSecondarySpell] No SECONDARY slot spells on %s (Generic with evolved weapon crystal only)"), *Actor->GetName());
		return;
	}

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, SpellAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugExecuteAsyncAbility()
{
	AActor *Actor = GetDebugActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAbility: No actor available"));
		return;
	}

	// Find target from opposing team (B-256: GetDebugOpponent)
	AActor *Target = GetDebugOpponent(Actor);

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugExecuteAsyncAbility: No valid target"));
		return;
	}

	// Try to get an ability from LoadoutComponent
	UAbilityData *AbilityData = nullptr;
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		TArray<UAbilityData *> AvailableAbilities = Loadout->GetAvailableAbilities();
		if (AvailableAbilities.Num() > 0)
		{
			AbilityData = AvailableAbilities[0];
		}
	}

	// OBSOLETE (B-118): test-asset fallback, only with wor.Debug.AllowTestAssetFallback 1
	if (!AbilityData)
	{
		AbilityData = CombatDebugAssetFallback::LoadTestSkill<UAbilityData>(
			TEXT("/Game/Testing/Weapons/Abilities/DA_Test_Ability.DA_Test_Ability"));
	}

	if (!AbilityData)
	{
		UE_LOG(LogWoRCombat, Error, TEXT("[DebugExecuteAsyncAbility] No ability available - give the actor one in its loadout (B-118)"));
		return;
	}

	FAction AbilityAction;
	AbilityAction.ActionType = EActionType::Ability;
	AbilityAction.SkillData = AbilityData;
	AbilityAction.Targets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[DebugExecuteAsyncAbility] %s using %s on %s"),
		   *Actor->GetName(), *AbilityData->Name, *Target->GetName());

	if (ActionExecutorRef)
	{
		bWaitingForAsyncAction = true;
		ActionExecutorRef->ExecuteActionAsync(Actor, AbilityAction,
											  FOnActionComplete::CreateUObject(this, &ACombatOrchestrator::HandleAsyncActionCompleted));
	}
}

void ACombatOrchestrator::DebugDrawCombatGrid(float Duration)
{
	UCombatGridSubsystem *Grid = GetGameInstance()->GetSubsystem<UCombatGridSubsystem>();
	if (!Grid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] CombatGridSubsystem not available"));
		return;
	}

	// Use world origin or this actor's location as arena center
	FVector ArenaCenter = GetActorLocation();

	// Draw the grid layout
	Grid->DebugDrawGrid(ArenaCenter, Duration);

	// Draw actor positions
	Grid->DebugDrawActorPositions(ArenaCenter, Duration);

	// Also log to output
	Grid->DebugLogAllPositions();
	Grid->DebugLogModifiers();
}

void ACombatOrchestrator::DebugStartCombatWithLevelActors()
{
	UWorld *World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] No world available"));
		return;
	}

	// Find actors by tag
	TArray<AActor *> LocalParty;
	TArray<AActor *> OpposingParty;

	TArray<AActor *> AllActors;
	UGameplayStatics::GetAllActorsWithTag(World, FName("LocalParty"), AllActors);
	for (AActor *Actor : AllActors)
	{
		if (Actor->FindComponentByClass<UCharacterDataComponent>())
		{
			LocalParty.Add(Actor);
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Found LocalParty: %s"), *Actor->GetName());
		}
	}

	AllActors.Empty();
	UGameplayStatics::GetAllActorsWithTag(World, FName("OpposingParty"), AllActors);
	for (AActor *Actor : AllActors)
	{
		if (Actor->FindComponentByClass<UCharacterDataComponent>())
		{
			OpposingParty.Add(Actor);
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Found OpposingParty: %s"), *Actor->GetName());
		}
	}

	// Validate
	if (LocalParty.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] No actors with 'LocalParty' tag found. Add tag to player characters."));
		return;
	}
	if (OpposingParty.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatOrchestrator] No actors with 'OpposingParty' tag found. Add tag to enemy characters."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Starting combat: %d vs %d"), LocalParty.Num(), OpposingParty.Num());

	// Start combat (handles grid assignment and actor placement)
	StartCombat(LocalParty, OpposingParty);

	// Draw debug visualization
	DebugDrawCombatGrid(10.0f);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Combat started and actors positioned!"));
}

AActor *ACombatOrchestrator::GetDebugOpponent(AActor *Actor) const
{
	const bool bActorInLocalParty = LocalPartyCombatants.Contains(Actor);
	const TArray<AActor *> &Opponents = bActorInLocalParty ? OpposingPartyCombatants : LocalPartyCombatants;
	return Opponents.Num() > 0 ? Opponents[0] : nullptr;
}

AActor *ACombatOrchestrator::GetDebugActor() const
{
	// Use override if set, otherwise fall back to current turn actor
	if (DebugOverrideActor)
	{
		return DebugOverrideActor;
	}
	return CurrentActor;
}

// SPIKE RETIRED (Stage 12 SC8): the throwaway fused-montage warp test
// (DebugWarpSpike + the Spike* arrays/handlers) is deleted — the production
// runner (ActionExecutor: BeginSkillExecution / OnCombatNotifyReceived /
// DispatchSpellCast) reproduces its validated behavior. Spike branch:
// spike/fused-montage-warp.

void ACombatOrchestrator::DebugManualAdvanceTurn()
{
	if (!TurnManagerRef || CombatState != ECombatState::InProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Cannot advance - combat not in progress"));
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Manual turn advance"));
	TurnManagerRef->AdvanceToNextTurn();
}

void ACombatOrchestrator::DebugToggleAutoAdvance()
{
	bAutoAdvanceTurns = !bAutoAdvanceTurns;

	if (!bAutoAdvanceTurns)
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoAdvanceTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Auto-advance: %s"),
		   bAutoAdvanceTurns ? TEXT("ON") : TEXT("OFF"));
}

void ACombatOrchestrator::DebugSelectTarget(int32 EnemyIndex)
{
	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] No current actor"));
		return;
	}

	int32 CurrentTeam = LocalPartyCombatants.Contains(CurrentActor) ? 0 : 1;
	TArray<AActor *> &EnemyTeam = (CurrentTeam == 0) ? OpposingPartyCombatants : LocalPartyCombatants;

	if (EnemyIndex < 0 || EnemyIndex >= EnemyTeam.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Invalid enemy index %d (team has %d)"),
			   EnemyIndex, EnemyTeam.Num());
		return;
	}

	DebugSelectedTargetIndex = EnemyIndex;
	AActor *Target = EnemyTeam[EnemyIndex];

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Selected target: %s (index %d)"),
		   *Target->GetName(), EnemyIndex);
}

void ACombatOrchestrator::DebugAttackSelectedTarget()
{
	if (!CurrentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] No current actor"));
		return;
	}

	int32 CurrentTeam = LocalPartyCombatants.Contains(CurrentActor) ? 0 : 1;
	TArray<AActor *> &EnemyTeam = (CurrentTeam == 0) ? OpposingPartyCombatants : LocalPartyCombatants;

	if (DebugSelectedTargetIndex >= EnemyTeam.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] Invalid target index"));
		return;
	}

	AActor *Target = EnemyTeam[DebugSelectedTargetIndex];

	FAction AttackAction;
	AttackAction.ActionType = EActionType::Ability; // attack/ability merge: attacks dispatch as Ability (SkillData + IsAttack)
	AttackAction.Targets.Add(Target);

	// Get weapon attack from loadout
	ULoadoutComponent *Loadout = CurrentActor->FindComponentByClass<ULoadoutComponent>();
	if (Loadout)
	{
		TArray<USkillDataBase *> Attacks = Loadout->GetAllWeaponAttacks();
		if (Attacks.Num() > 0)
		{
			AttackAction.SkillData = Attacks[0];
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] DEBUG: Attacking %s"), *Target->GetName());

	SubmitActionAsync(AttackAction);
}

// ========================================
// CRYSTAL WEAR DEBUG BUTTONS
// ========================================

void ACombatOrchestrator::DebugCrystalState()
{
	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_CrystalState();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugCrystalState: No CrystalManager subsystem."));
	}
}

void ACombatOrchestrator::DebugWearTable()
{
	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_WearTable();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugWearTable: No CrystalManager subsystem."));
	}
}

void ACombatOrchestrator::DebugSimCast_S_L2()
{
	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_SimCast(static_cast<int32>(EItemTier::S_Tier), 2);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_S_L2: No CrystalManager subsystem."));
	}
}

void ACombatOrchestrator::DebugSimCast_Matched_L1()
{
	if (!TurnManagerRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No TurnManager."));
		return;
	}

	AActor *Actor = TurnManagerRef->GetCurrentActor();
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No active combatant (combat not active?)."));
		return;
	}

	ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
	if (!Loadout)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No LoadoutComponent on %s."), *Actor->GetName());
		return;
	}

	UWeaponData *Weapon = Loadout->GetPrimaryWeapon();
	if (!Weapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: %s has no primary weapon equipped."), *Actor->GetName());
		return;
	}

	FRuntimeAttachedItem *Attachment = Loadout->FindAttachedItemByHolder(Weapon);
	if (!Attachment || Attachment->IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: %s's primary weapon has no attached crystal."), *Actor->GetName());
		return;
	}

	if (!Attachment->IsCrystal())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: %s's crystal is not refined (no tier to match)."), *Actor->GetName());
		return;
	}

	const int32 CrystalTier = static_cast<int32>(Attachment->Crystal.Id.Tier);

	if (UCrystalManager *CM = GetGameInstance() ? GetGameInstance()->GetSubsystem<UCrystalManager>() : nullptr)
	{
		CM->WOR_SimCast(CrystalTier, 1);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CombatOrchestrator] DebugSimCast_Matched_L1: No CrystalManager subsystem."));
	}
}
