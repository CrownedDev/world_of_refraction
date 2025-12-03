// InfusionChargeManager.cpp
// Updated for EChargeInfusionType + HP Cost system

#include "InfusionChargeManager.h"
#include "ActionStructs.h"
#include "CharacterDataComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UInfusionChargeManager::UInfusionChargeManager()
{
}

void UInfusionChargeManager::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Initialized"));
}

void UInfusionChargeManager::Deinitialize()
{
	// Clear any active timer
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	ResetState();
	Super::Deinitialize();
}

// ========================================
// CHARGING API
// ========================================

bool UInfusionChargeManager::BeginCharge(AActor *Actor, EChargeInfusionType ChargeType, EInfusionSourceOption SelectedSource)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InfusionChargeManager] BeginCharge failed - null actor"));
		return false;
	}

	if (ChargeType == EChargeInfusionType::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InfusionChargeManager] BeginCharge failed - cannot charge None type"));
		return false;
	}

	// If already charging, cancel previous
	if (CurrentState == EChargeState::Charging)
	{
		CancelCharge();
	}

	// Start new charge
	ChargingActor = Actor;
	ChargingType = ChargeType;
	ChargingSource = SelectedSource;
	ChargeTime = 0.0f;
	CurrentLevel = 0;
	HPCostPaid = 0.0f;
	CurrentState = EChargeState::Charging;

	// Deduct L1 HP cost immediately (committed cost)
	float InitialHPCost = DeductHPCost(Actor, 1);
	HPCostPaid = InitialHPCost;

	// Start auto-update timer if enabled
	if (bAutoUpdate)
	{
		if (UWorld *World = GetWorld())
		{
			// Update at 60fps for smooth progress
			World->GetTimerManager().SetTimer(
				UpdateTimerHandle,
				this,
				&UInfusionChargeManager::OnUpdateTimer,
				1.0f / 60.0f,
				true);
		}
	}

	OnChargeStarted.Broadcast(Actor, ChargeType, SelectedSource, 0);

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge started - Actor: %s, Type: %s, Source: %d, HP Cost: %.1f"),
		   *Actor->GetName(),
		   *ChargeInfusionTypeHelpers::GetTypeName(ChargeType),
		   static_cast<int32>(SelectedSource),
		   InitialHPCost);

	return true;
}

int32 UInfusionChargeManager::CompleteCharge()
{
	if (CurrentState != EChargeState::Charging)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InfusionChargeManager] CompleteCharge called but not charging"));
		return 0;
	}

	// Stop timer
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	// Capture final state
	int32 FinalLevel = CurrentLevel;
	AActor *Actor = ChargingActor.Get();
	EChargeInfusionType Type = ChargingType;
	EInfusionSourceOption Source = ChargingSource;

	CurrentState = EChargeState::Ready;

	// Broadcast completion
	if (Actor)
	{
		OnChargeComplete.Broadcast(Actor, Type, Source, FinalLevel);
	}

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge complete - Level: %d, Time: %.2fs, Total HP Cost: %.1f"),
		   FinalLevel, ChargeTime, HPCostPaid);

	// Reset state
	ResetState();

	return FinalLevel;
}

void UInfusionChargeManager::CancelCharge()
{
	if (CurrentState != EChargeState::Charging)
	{
		return;
	}

	// Stop timer
	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	// Capture state for broadcast
	AActor *Actor = ChargingActor.Get();
	int32 LevelAtCancel = CurrentLevel;
	float TotalHPCost = HPCostPaid;

	CurrentState = EChargeState::Cancelled;

	// Broadcast cancellation (HP cost is NOT refunded)
	if (Actor)
	{
		OnChargeCancelled.Broadcast(Actor, LevelAtCancel, TotalHPCost);
	}

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge cancelled at Level %d (HP spent: %.1f - NOT refunded)"),
		   LevelAtCancel, TotalHPCost);

	ResetState();
}

void UInfusionChargeManager::UpdateCharge(float DeltaTime)
{
	if (CurrentState != EChargeState::Charging)
	{
		return;
	}

	// Accumulate time
	ChargeTime += DeltaTime;

	// Check for level change
	int32 NewLevel = CalculateLevelFromTime(ChargeTime);

	if (NewLevel != CurrentLevel)
	{
		int32 OldLevel = CurrentLevel;
		CurrentLevel = NewLevel;

		// Deduct additional HP cost when reaching L2
		float AdditionalHPCost = 0.0f;
		if (NewLevel == 2 && OldLevel == 1)
		{
			// Deduct the difference between L2 and L1 cost
			float L2CostPercent = GetHPCostPercent(2);
			float L1CostPercent = GetHPCostPercent(1);
			float DifferencePercent = L2CostPercent - L1CostPercent;

			AActor *Actor = ChargingActor.Get();
			if (Actor)
			{
				UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
				if (CharComp)
				{
					AdditionalHPCost = CharComp->CurrentHP * DifferencePercent;
					CharComp->CurrentHP = FMath::Max(1, CharComp->CurrentHP - FMath::RoundToInt(AdditionalHPCost));
					HPCostPaid += AdditionalHPCost;
				}
			}
		}

		// Broadcast level change with HP cost info
		if (ChargingActor.IsValid())
		{
			OnChargeLevelChanged.Broadcast(ChargingActor.Get(), OldLevel, NewLevel, AdditionalHPCost);
		}

		UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge level changed: %d -> %d (Time: %.2fs, Additional HP: %.1f)"),
			   OldLevel, NewLevel, ChargeTime, AdditionalHPCost);
	}
}

// ========================================
// QUERY
// ========================================

FChargeStatus UInfusionChargeManager::GetChargeStatus() const
{
	FChargeStatus Status;
	Status.State = CurrentState;
	Status.ChargeType = ChargingType;
	Status.SelectedSource = ChargingSource;
	Status.ChargeLevel = CurrentLevel;
	Status.ChargeTime = ChargeTime;
	Status.ChargingActor = ChargingActor.Get();
	Status.ProgressToNextLevel = CalculateProgressToNextLevel();
	Status.HPCostPaid = HPCostPaid;

	// Calculate time to next level
	if (CurrentLevel == 0)
	{
		Status.TimeToNextLevel = Level1Time - ChargeTime;
	}
	else if (CurrentLevel == 1)
	{
		Status.TimeToNextLevel = Level2Time - ChargeTime;
	}
	else
	{
		Status.TimeToNextLevel = -1.0f; // At max
	}

	return Status;
}

bool UInfusionChargeManager::IsCharging() const
{
	return CurrentState == EChargeState::Charging;
}

int32 UInfusionChargeManager::GetCurrentChargeLevel() const
{
	return CurrentLevel;
}

EChargeInfusionType UInfusionChargeManager::GetChargingType() const
{
	return ChargingType;
}

EInfusionSourceOption UInfusionChargeManager::GetSelectedSource() const
{
	return ChargingSource;
}

float UInfusionChargeManager::GetHPCostPaid() const
{
	return HPCostPaid;
}

AActor *UInfusionChargeManager::GetChargingActor() const
{
	return ChargingActor.Get();
}

// ========================================
// AI / DIRECT SET
// ========================================

void UInfusionChargeManager::SetChargeLevel(AActor *Actor, EChargeInfusionType ChargeType, EInfusionSourceOption Source, int32 Level)
{
	// Clamp level
	Level = FMath::Clamp(Level, 0, 2);

	// Set directly without timing
	ChargingActor = Actor;
	ChargingType = ChargeType;
	ChargingSource = Source;
	CurrentLevel = Level;
	CurrentState = EChargeState::Ready;

	// Calculate equivalent time for logging
	if (Level == 0)
	{
		ChargeTime = 0.0f;
	}
	else if (Level == 1)
	{
		ChargeTime = Level1Time;
	}
	else
	{
		ChargeTime = Level2Time;
	}

	// Deduct HP cost for the target level
	HPCostPaid = DeductHPCost(Actor, Level);

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Direct set - Actor: %s, Type: %s, Source: %d, Level: %d, HP Cost: %.1f"),
		   Actor ? *Actor->GetName() : TEXT("None"),
		   *ChargeInfusionTypeHelpers::GetTypeName(ChargeType),
		   static_cast<int32>(Source),
		   Level,
		   HPCostPaid);
}

void UInfusionChargeManager::ApplyChargeToAction(FAction &Action, EChargeInfusionType ChargeType, EInfusionSourceOption Source, int32 Level)
{
	Level = FMath::Clamp(Level, 0, 2);

	// Set the appropriate field based on charge type
	if (ChargeType == EChargeInfusionType::Spell)
	{
		Action.SpellInfusionLevel = Level;
	}
	else if (ChargeType == EChargeInfusionType::Ability)
	{
		Action.AbilityInfusionLevel = Level;
		Action.SelectedSource = Source;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[InfusionChargeManager] Applied charge to action - Type: %s, Source: %d, Level: %d"),
		   *ChargeInfusionTypeHelpers::GetTypeName(ChargeType),
		   static_cast<int32>(Source),
		   Level);
}

// ========================================
// DEBUG
// ========================================

void UInfusionChargeManager::DebugPrintStatus() const
{
	FChargeStatus Status = GetChargeStatus();

	FString StateStr;
	switch (Status.State)
	{
	case EChargeState::Idle:
		StateStr = TEXT("Idle");
		break;
	case EChargeState::Charging:
		StateStr = TEXT("Charging");
		break;
	case EChargeState::Ready:
		StateStr = TEXT("Ready");
		break;
	case EChargeState::Cancelled:
		StateStr = TEXT("Cancelled");
		break;
	}

	UE_LOG(LogTemp, Warning, TEXT("=== InfusionChargeManager Status ==="));
	UE_LOG(LogTemp, Warning, TEXT("State: %s"), *StateStr);
	UE_LOG(LogTemp, Warning, TEXT("Type: %s"), *ChargeInfusionTypeHelpers::GetTypeName(Status.ChargeType));
	UE_LOG(LogTemp, Warning, TEXT("Source: %d"), static_cast<int32>(Status.SelectedSource));
	UE_LOG(LogTemp, Warning, TEXT("Level: %d"), Status.ChargeLevel);
	UE_LOG(LogTemp, Warning, TEXT("Time: %.2fs"), Status.ChargeTime);
	UE_LOG(LogTemp, Warning, TEXT("Progress: %.1f%%"), Status.ProgressToNextLevel * 100.0f);
	UE_LOG(LogTemp, Warning, TEXT("Time to next: %.2fs"), Status.TimeToNextLevel);
	UE_LOG(LogTemp, Warning, TEXT("HP Cost Paid: %.1f"), Status.HPCostPaid);
	UE_LOG(LogTemp, Warning, TEXT("Actor: %s"), Status.ChargingActor ? *Status.ChargingActor->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Warning, TEXT("================================"));
}

// ========================================
// INTERNAL METHODS
// ========================================

int32 UInfusionChargeManager::CalculateLevelFromTime(float Time) const
{
	if (Time >= Level2Time)
	{
		return 2;
	}
	else if (Time >= Level1Time)
	{
		return 1;
	}
	return 0;
}

float UInfusionChargeManager::CalculateProgressToNextLevel() const
{
	if (CurrentLevel >= 2)
	{
		return 1.0f; // At max
	}

	float TargetTime = (CurrentLevel == 0) ? Level1Time : Level2Time;
	float StartTime = (CurrentLevel == 0) ? 0.0f : Level1Time;

	float Progress = (ChargeTime - StartTime) / (TargetTime - StartTime);
	return FMath::Clamp(Progress, 0.0f, 1.0f);
}

void UInfusionChargeManager::ResetState()
{
	CurrentState = EChargeState::Idle;
	ChargingActor.Reset();
	ChargingType = EChargeInfusionType::None;
	ChargingSource = EInfusionSourceOption::None;
	CurrentLevel = 0;
	ChargeTime = 0.0f;
	HPCostPaid = 0.0f;
}

void UInfusionChargeManager::OnUpdateTimer()
{
	// Fixed timestep update (1/60th second)
	UpdateCharge(1.0f / 60.0f);
}

float UInfusionChargeManager::DeductHPCost(AActor *Actor, int32 TargetLevel)
{
	if (!Actor || TargetLevel <= 0)
	{
		return 0.0f;
	}

	UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InfusionChargeManager] DeductHPCost - No CharacterDataComponent on actor"));
		return 0.0f;
	}

	float CostPercent = GetHPCostPercent(TargetLevel);
	float HPCost = CharComp->CurrentHP * CostPercent;

	// Deduct but never go below 1 HP (can't kill self with infusion)
	int32 OldHP = CharComp->CurrentHP;
	int32 NewHP = FMath::Max(1, CharComp->CurrentHP - FMath::RoundToInt(HPCost));
	float ActualCost = static_cast<float>(OldHP - NewHP);
	CharComp->CurrentHP = NewHP;

	UE_LOG(LogTemp, Verbose, TEXT("[InfusionChargeManager] Deducted %.1f HP (%.0f%% of %d) for L%d, new HP: %d"),
		   ActualCost, CostPercent * 100.0f, OldHP, TargetLevel, NewHP);

	return ActualCost;
}

float UInfusionChargeManager::GetHPCostPercent(int32 TargetLevel) const
{
	switch (TargetLevel)
	{
	case 1:
		return InfusionConstants::CHARGE_L1_HP_COST_PERCENT;
	case 2:
		return InfusionConstants::CHARGE_L2_HP_COST_PERCENT;
	default:
		return 0.0f;
	}
}