// InfusionChargeManager.cpp

#include "InfusionChargeManager.h"
#include "ActionStructs.h"
#include "Engine/World.h"
#include "TimerManager.h"

UInfusionChargeManager::UInfusionChargeManager()
{
}

void UInfusionChargeManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Initialized"));
}

void UInfusionChargeManager::Deinitialize()
{
	// Clear any active timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	ResetState();
	Super::Deinitialize();
}

// ========================================
// CHARGING API
// ========================================

bool UInfusionChargeManager::BeginCharge(AActor* Actor, EInfusionType Type)
{
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InfusionChargeManager] BeginCharge failed - null actor"));
		return false;
	}

	if (Type == EInfusionType::None)
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
	ChargingType = Type;
	ChargeTime = 0.0f;
	CurrentLevel = 0;
	CurrentState = EChargeState::Charging;

	// Start auto-update timer if enabled
	if (bAutoUpdate)
	{
		if (UWorld* World = GetWorld())
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

	OnChargeStarted.Broadcast(Actor, Type, 0);

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge started - Actor: %s, Type: %s"),
		*Actor->GetName(),
		*InfusionTypeHelpers::GetInfusionName(Type));

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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	// Capture final state
	int32 FinalLevel = CurrentLevel;
	AActor* Actor = ChargingActor.Get();
	EInfusionType Type = ChargingType;

	CurrentState = EChargeState::Ready;

	// Broadcast completion
	if (Actor)
	{
		OnChargeComplete.Broadcast(Actor, Type, FinalLevel);
	}

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge complete - Level: %d, Time: %.2fs"),
		FinalLevel, ChargeTime);

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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	// Capture state for broadcast
	AActor* Actor = ChargingActor.Get();
	int32 LevelAtCancel = CurrentLevel;

	CurrentState = EChargeState::Cancelled;

	// Broadcast cancellation
	if (Actor)
	{
		OnChargeCancelled.Broadcast(Actor, LevelAtCancel);
	}

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge cancelled at Level %d"), LevelAtCancel);

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

		// Broadcast level change
		if (ChargingActor.IsValid())
		{
			OnChargeLevelChanged.Broadcast(ChargingActor.Get(), OldLevel, NewLevel);
		}

		UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Charge level changed: %d → %d (Time: %.2fs)"),
			OldLevel, NewLevel, ChargeTime);
	}
}

// ========================================
// QUERY
// ========================================

FChargeStatus UInfusionChargeManager::GetChargeStatus() const
{
	FChargeStatus Status;
	Status.State = CurrentState;
	Status.InfusionType = ChargingType;
	Status.ChargeLevel = CurrentLevel;
	Status.ChargeTime = ChargeTime;
	Status.ChargingActor = ChargingActor.Get();
	Status.ProgressToNextLevel = CalculateProgressToNextLevel();

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

EInfusionType UInfusionChargeManager::GetChargingInfusionType() const
{
	return ChargingType;
}

AActor* UInfusionChargeManager::GetChargingActor() const
{
	return ChargingActor.Get();
}

// ========================================
// AI / DIRECT SET
// ========================================

void UInfusionChargeManager::SetChargeLevel(AActor* Actor, EInfusionType Type, int32 Level)
{
	// Clamp level
	Level = FMath::Clamp(Level, 0, 2);

	// Set directly without timing
	ChargingActor = Actor;
	ChargingType = Type;
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

	UE_LOG(LogTemp, Log, TEXT("[InfusionChargeManager] Direct set - Actor: %s, Type: %s, Level: %d"),
		Actor ? *Actor->GetName() : TEXT("None"),
		*InfusionTypeHelpers::GetInfusionName(Type),
		Level);
}

void UInfusionChargeManager::ApplyChargeToAction(FAction& Action, EInfusionType Type, int32 Level)
{
	Action.InfusionType = Type;
	Action.InfusionLevel = FMath::Clamp(Level, 0, 2);

	UE_LOG(LogTemp, Verbose, TEXT("[InfusionChargeManager] Applied charge to action - Type: %s, Level: %d"),
		*InfusionTypeHelpers::GetInfusionName(Type),
		Action.InfusionLevel);
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
	UE_LOG(LogTemp, Warning, TEXT("Type: %s"), *InfusionTypeHelpers::GetInfusionName(Status.InfusionType));
	UE_LOG(LogTemp, Warning, TEXT("Level: %d"), Status.ChargeLevel);
	UE_LOG(LogTemp, Warning, TEXT("Time: %.2fs"), Status.ChargeTime);
	UE_LOG(LogTemp, Warning, TEXT("Progress: %.1f%%"), Status.ProgressToNextLevel * 100.0f);
	UE_LOG(LogTemp, Warning, TEXT("Time to next: %.2fs"), Status.TimeToNextLevel);
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
	ChargingType = EInfusionType::None;
	CurrentLevel = 0;
	ChargeTime = 0.0f;
}

void UInfusionChargeManager::OnUpdateTimer()
{
	// Fixed timestep update (1/60th second)
	UpdateCharge(1.0f / 60.0f);
}
