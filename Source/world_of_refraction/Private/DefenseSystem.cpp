// DefenseSystem.cpp
// Real-time defense system implementation

#include "DefenseSystem.h"
#include "CharacterDataComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CombatAnimInstance.h"
#include "AIDecisionManager.h"
#include "CharacterData.h"
#include "LoadoutComponent.h"

void UDefenseSystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Initialized"));
}

void UDefenseSystem::Deinitialize()
{
	// Clear all timers
	if (UWorld *World = GetWorld())
	{
		for (auto &Pair : WindowTimerHandles)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}

	ActiveDefenseStates.Empty();
	WindowTimerHandles.Empty();
	Super::Deinitialize();
}

// ========================================
// DEFENSE WINDOW MANAGEMENT
// ========================================

void UDefenseSystem::OpenDefenseWindow(
	AActor *Attacker,
	AActor *Defender,
	float AttackSize,
	int32 BaseDamage,
	float WindowDuration,
	bool bIsElemental)
{
	if (!Defender)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] Cannot open window - null defender"));
		return;
	}

	// Close existing window if any
	if (ActiveDefenseStates.Contains(Defender))
	{
		CloseDefenseWindow(Defender);
	}

	// Create new defense state
	FDefenseState State;
	State.Defender = Defender;
	State.Attacker = Attacker;
	State.bWindowOpen = true;
	State.AttackSize = AttackSize;
	State.BaseDamage = BaseDamage;
	State.bIsElemental = bIsElemental;
	State.DefenseChosen = EDefenseType::None;
	State.DodgeDirection = EDefenseDirection::None;
	State.WindowOpenTime = FPlatformTime::Seconds();
	State.WindowDuration = WindowDuration > 0.0f ? WindowDuration : DefaultWindowDuration;
	State.bInputReceived = false;

	ActiveDefenseStates.Add(Defender, State);

	// Set timer to auto-close window
	FTimerHandle TimerHandle;
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &UDefenseSystem::OnWindowTimerExpired, Defender);

	if (UWorld *World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TimerHandle,
			TimerDelegate,
			State.WindowDuration,
			false);

		WindowTimerHandles.Add(Defender, TimerHandle);
	}

	// Broadcast event for UI
	OnDefenseWindowOpened.Broadcast(Defender, AttackSize, State.WindowDuration);

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense window opened for %s (Size: %.1f, Damage: %d, Duration: %.2fs)"),
		   *Defender->GetName(), AttackSize, BaseDamage, State.WindowDuration);

	// Check if AI-controlled and schedule defense
	UCharacterDataComponent *CharComp = Defender->FindComponentByClass<UCharacterDataComponent>();
	if (!CharComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No CharacterDataComponent on %s"), *Defender->GetName());
	}
	else if (!CharComp->CharacterData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] CharacterData is null on %s"), *Defender->GetName());
	}
	else if (!CharComp->CharacterData->ShouldUseAI())
	{
		UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] %s is not AI-controlled, skipping AI defense"), *Defender->GetName());
	}
	else
	{
		UAIDecisionManager *AIManager = GetGameInstance()->GetSubsystem<UAIDecisionManager>();
		if (!AIManager)
		{
			UE_LOG(LogTemp, Error, TEXT("[DefenseSystem] Failed to get AIDecisionManager!"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Scheduling AI defense for %s"), *Defender->GetName());
			AIManager->ScheduleDefenseDecision(Defender, AttackSize, BaseDamage, State.WindowDuration);
		}
	}
}

FDefenseResult UDefenseSystem::CloseDefenseWindow(AActor *Defender)
{
	FDefenseResult Result;

	if (!Defender)
	{
		return Result;
	}

	// Get and remove state
	FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	if (!StatePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No active defense window for %s"),
			   *Defender->GetName());
		return Result;
	}

	FDefenseState State = *StatePtr;
	ActiveDefenseStates.Remove(Defender);

	// Clear timer
	if (FTimerHandle *TimerHandle = WindowTimerHandles.Find(Defender))
	{
		if (UWorld *World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
		WindowTimerHandles.Remove(Defender);
	}

	// Calculate result
	float DodgeThreshold = GetDodgeThreshold(Defender);
	Result = CalculateDefenseResult(
		State.BaseDamage,
		State.DefenseChosen,
		State.bInputReceived,
		State.AttackSize,
		DodgeThreshold);

	Result.bWasInWindow = State.bInputReceived;

	// Handle parry reflect
	if (Result.bSuccess && State.DefenseChosen == EDefenseType::Parry && Result.ReflectedDamage > 0)
	{
		if (State.Attacker.IsValid())
		{
			ApplyReflectedDamage(State.Attacker.Get(), Result.ReflectedDamage);
			OnParryReflect.Broadcast(Defender, State.Attacker.Get(), Result.ReflectedDamage);
		}
	}

	// Broadcast event
	OnDefenseWindowClosed.Broadcast(Defender, Result);

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense window closed for %s - Type: %d, Success: %s, Damage: %d → %d"),
		   *Defender->GetName(),
		   static_cast<int32>(State.DefenseChosen),
		   Result.bSuccess ? TEXT("Yes") : TEXT("No"),
		   State.BaseDamage,
		   Result.FinalDamage);

	return Result;
}

void UDefenseSystem::SubmitDefenseInput(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction)
{
	if (!Defender)
	{
		return;
	}

	FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	if (!StatePtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No active defense window for %s - input ignored"),
			   *Defender->GetName());
		return;
	}

	if (!StatePtr->bWindowOpen)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] Defense window not open for %s - input ignored"),
			   *Defender->GetName());
		return;
	}

	if (StatePtr->bInputReceived)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] Defense input already received for %s"),
			   *Defender->GetName());
		return;
	}

	// Validate dodge direction
	if (DefenseType == EDefenseType::Dodge)
	{
		if (Direction != EDefenseDirection::Left && Direction != EDefenseDirection::Right)
		{
			UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] Dodge requires Left or Right direction"));
			Direction = EDefenseDirection::Left; // Default to left
		}
	}

	// Record input
	StatePtr->DefenseChosen = DefenseType;
	StatePtr->DodgeDirection = Direction;
	StatePtr->bInputReceived = true;

	// Play defense animation
	PlayDefenseAnimation(Defender, DefenseType, Direction);

	// Broadcast event
	OnDefenseInputReceived.Broadcast(Defender, DefenseType, Direction);

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense input received: %s chose %d (Direction: %d)"),
		   *Defender->GetName(),
		   static_cast<int32>(DefenseType),
		   static_cast<int32>(Direction));
}

bool UDefenseSystem::IsDefenseWindowOpen(AActor *Defender) const
{
	if (!Defender)
	{
		return false;
	}

	const FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	return StatePtr && StatePtr->bWindowOpen;
}

FDefenseState UDefenseSystem::GetDefenseState(AActor *Defender) const
{
	if (!Defender)
	{
		return FDefenseState();
	}

	const FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	return StatePtr ? *StatePtr : FDefenseState();
}

float UDefenseSystem::GetRemainingWindowTime(AActor *Defender) const
{
	if (!Defender)
	{
		return 0.0f;
	}

	const FDefenseState *StatePtr = ActiveDefenseStates.Find(Defender);
	if (!StatePtr || !StatePtr->bWindowOpen)
	{
		return 0.0f;
	}

	double Elapsed = FPlatformTime::Seconds() - StatePtr->WindowOpenTime;
	float Remaining = StatePtr->WindowDuration - static_cast<float>(Elapsed);
	return FMath::Max(0.0f, Remaining);
}

// ========================================
// DODGE CALCULATIONS
// ========================================

bool UDefenseSystem::CanDodgeAttack(AActor *Defender, float AttackSize) const
{
	float Threshold = GetDodgeThreshold(Defender);
	return AttackSize < Threshold;
}

float UDefenseSystem::GetDodgeThreshold(AActor *Defender) const
{
	// Base threshold
	float Threshold = BaseDodgeThreshold;

	// TODO: Could be modified by:
	// - Character stats (agility/speed)
	// - Equipment bonuses
	// - Status effects (slowed reduces threshold)
	// - Character size

	return Threshold;
}

// ========================================
// DAMAGE CALCULATION
// ========================================

FDefenseResult UDefenseSystem::CalculateDefenseResult(
	int32 BaseDamage,
	EDefenseType DefenseType,
	bool bDefenseSuccessful,
	float AttackSize,
	float DodgeThreshold)
{
	FDefenseResult Result;
	Result.DefenseType = DefenseType;
	Result.FinalDamage = BaseDamage;
	Result.ReflectedDamage = 0;
	Result.bSuccess = false;

	// No defense = full damage
	if (DefenseType == EDefenseType::None || !bDefenseSuccessful)
	{
		Result.FailureReason = TEXT("No defense or missed timing");
		return Result;
	}

	switch (DefenseType)
	{
	case EDefenseType::Block:
		// Block always works, 50% reduction
		Result.bSuccess = true;
		Result.FinalDamage = FMath::RoundToInt(BaseDamage * 0.5f);
		break;

	case EDefenseType::Parry:
		// Parry: 70% reduction + 30% reflect
		Result.bSuccess = true;
		Result.FinalDamage = FMath::RoundToInt(BaseDamage * 0.3f);
		Result.ReflectedDamage = FMath::RoundToInt(BaseDamage * 0.3f);
		break;

	case EDefenseType::Dodge:
		// Dodge: 100% avoidance IF attack is small enough
		if (AttackSize < DodgeThreshold)
		{
			Result.bSuccess = true;
			Result.FinalDamage = 0;
		}
		else
		{
			// Attack too big, dodge fails completely
			Result.bSuccess = false;
			Result.FinalDamage = BaseDamage;
			Result.FailureReason = FString::Printf(
				TEXT("Attack too large (%.1f >= %.1f threshold)"),
				AttackSize, DodgeThreshold);
		}
		break;

	default:
		break;
	}

	return Result;
}

void UDefenseSystem::PlayDefenseAnimation(AActor *Defender, EDefenseType DefenseType, EDefenseDirection Direction)
{
	if (!Defender)
	{
		return;
	}

	// Get LoadoutComponent for animation references
	ULoadoutComponent *LoadoutComp = Defender->FindComponentByClass<ULoadoutComponent>();
	if (!LoadoutComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No LoadoutComponent for %s - cannot play defense animation"),
			   *Defender->GetName());
		return;
	}

	UAnimMontage *MontageToPlay = nullptr;
	float PlayRate = 1.0f;

	switch (DefenseType)
	{
	case EDefenseType::Block:
		MontageToPlay = LoadoutComp->GetBlockMontage();
		break;

	case EDefenseType::Parry:
		MontageToPlay = LoadoutComp->GetParryMontage();
		break;

	case EDefenseType::Dodge:
		if (Direction == EDefenseDirection::Left)
		{
			MontageToPlay = LoadoutComp->GetDodgeLeftMontage();
		}
		else
		{
			MontageToPlay = LoadoutComp->GetDodgeRightMontage();
		}
		break;

	default:
		return;
	}

	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DefenseSystem] No montage for %s defense type %d"),
			   *Defender->GetName(), static_cast<int32>(DefenseType));
		return;
	}

	// Get CombatAnimInstance and play
	ACharacter *Character = Cast<ACharacter>(Defender);
	if (!Character)
	{
		return;
	}

	UCombatAnimInstance *CombatAnim = Cast<UCombatAnimInstance>(Character->GetMesh()->GetAnimInstance());
	if (CombatAnim)
	{
		// Use PlayRate for direction (negative = reversed)
		CombatAnim->PlayActionMontage(MontageToPlay, PlayRate);
		UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Playing %s for %s (Rate: %.1f)"),
			   *MontageToPlay->GetName(), *Defender->GetName(), PlayRate);
	}
}

// ========================================
// INTERNAL METHODS
// ========================================

void UDefenseSystem::OnWindowTimerExpired(AActor *Defender)
{
	if (!Defender)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Defense window timer expired for %s"),
		   *Defender->GetName());

	// Close window and apply result
	CloseDefenseWindow(Defender);
}

void UDefenseSystem::ApplyReflectedDamage(AActor *Attacker, int32 Damage)
{
	if (!Attacker || Damage <= 0)
	{
		return;
	}

	UCharacterDataComponent *Comp = GetCharacterDataComponent(Attacker);
	if (Comp)
	{
		Comp->ServerTakeDamage(Damage);
		UE_LOG(LogTemp, Log, TEXT("[DefenseSystem] Applied %d reflected damage to %s"),
			   Damage, *Attacker->GetName());
	}
}

UCharacterDataComponent *UDefenseSystem::GetCharacterDataComponent(AActor *Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	return Actor->FindComponentByClass<UCharacterDataComponent>();
}
