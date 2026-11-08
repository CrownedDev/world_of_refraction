// StatusBarStateComponent.cpp

#include "Combat/StatusBarStateComponent.h"
#include "Combat/WoRFightSubsystem.h"
#include "Net/UnrealNetwork.h"

UStatusBarStateComponent::UStatusBarStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UStatusBarStateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UStatusBuildupManager *Manager = WoRFight::Get<UStatusBuildupManager>(this))
		{
			Manager->OnStatusBuildupChanged.AddUniqueDynamic(this, &UStatusBarStateComponent::HandleBuildupChanged);
			BoundManager = Manager;
		}
	}
}

void UStatusBarStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UStatusBuildupManager *Manager = BoundManager.Get())
	{
		Manager->OnStatusBuildupChanged.RemoveDynamic(this, &UStatusBarStateComponent::HandleBuildupChanged);
	}
	BoundManager.Reset();
	Super::EndPlay(EndPlayReason);
}

void UStatusBarStateComponent::HandleBuildupChanged(AActor *Target, float Current, float Max, ESpellElement PendingElement, AActor *Source)
{
	if (Target == GetOwner())
	{
		SetBar(Current, Max, PendingElement, Source);
	}
}

void UStatusBarStateComponent::SetBar(float Current, float Max, ESpellElement PendingElement, AActor *Source)
{
	Bar.Current = Current;
	Bar.Max = Max;
	Bar.PendingElement = PendingElement;
	Bar.Source = Source;
}

void UStatusBarStateComponent::OnRep_Bar()
{
	OnBarChanged.Broadcast(GetOwner(), Bar.Current, Bar.Max, Bar.PendingElement, Bar.Source);
}

FString UStatusBarStateComponent::GetDebugString() const
{
	return FString::Printf(TEXT("%s status bar %.0f / %.0f (%s, last hit by %s)"), *GetNameSafe(GetOwner()), Bar.Current, Bar.Max,
						   *UEnum::GetValueAsString(Bar.PendingElement), *GetNameSafe(Bar.Source));
}

void UStatusBarStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UStatusBarStateComponent, Bar);
}
