#include "WeatherStateManager.h"
#include "CharacterDataComponent.h"
#include "CharacterData.h"
#include "CosmeticsData.h"

void UWeatherStateManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    Team0Hierarchy.Empty();
    Team1Hierarchy.Empty();
    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Initialized"));
}

void UWeatherStateManager::Deinitialize()
{
    EndCombat();
    Super::Deinitialize();
}

// ========================================
// COMBAT SETUP
// ========================================

void UWeatherStateManager::InitialiseLeaders(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1)
{
    EndCombat();

    Team0Hierarchy = BuildHierarchy(Team0);
    Team1Hierarchy = BuildHierarchy(Team1);

    BindToLeader(Team0Hierarchy, true);
    BindToLeader(Team1Hierarchy, false);

    RecalculateWeather();

    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Leaders initialised - Team0: %s, Team1: %s"),
           Team0Hierarchy.Num() > 0 ? *Team0Hierarchy[0].Actor->GetName() : TEXT("None"),
           Team1Hierarchy.Num() > 0 ? *Team1Hierarchy[0].Actor->GetName() : TEXT("None"));
}

void UWeatherStateManager::EndCombat()
{
    // Unbind Team0 leader
    if (Team0Hierarchy.Num() > 0 && Team0Hierarchy[0].Actor)
    {
        if (UCharacterDataComponent *Comp = Team0Hierarchy[0].Actor->FindComponentByClass<UCharacterDataComponent>())
        {
            Comp->OnDied.RemoveDynamic(this, &UWeatherStateManager::OnTeam0LeaderDied);
        }
    }

    // Unbind Team1 leader
    if (Team1Hierarchy.Num() > 0 && Team1Hierarchy[0].Actor)
    {
        if (UCharacterDataComponent *Comp = Team1Hierarchy[0].Actor->FindComponentByClass<UCharacterDataComponent>())
        {
            Comp->OnDied.RemoveDynamic(this, &UWeatherStateManager::OnTeam1LeaderDied);
        }
    }

    Team0Hierarchy.Empty();
    Team1Hierarchy.Empty();

    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Combat ended - hierarchies cleared"));
}

// ========================================
// INTERNAL
// ========================================

TArray<FLeadershipEntry> UWeatherStateManager::BuildHierarchy(const TArray<AActor *> &Team)
{
    TArray<FLeadershipEntry> Hierarchy;

    for (AActor *Actor : Team)
    {
        if (!Actor)
            continue;

        FLeadershipEntry Entry;
        Entry.Actor = Actor;
        Entry.TotalWorldStats = GetTotalWorldStats(Actor);
        Entry.Element = ESpellElement::Generic;

        if (UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>())
        {
            if (Comp->CharacterData)
            {
                Entry.Element = Comp->CharacterData->InnateElement;
            }
        }

        Hierarchy.Add(Entry);
    }

    // Sort by world stats descending (highest = leader)
    Hierarchy.Sort([](const FLeadershipEntry &A, const FLeadershipEntry &B)
                   { return A.TotalWorldStats > B.TotalWorldStats; });

    return Hierarchy;
}

void UWeatherStateManager::BindToLeader(TArray<FLeadershipEntry> &Hierarchy, bool bIsTeam0)
{
    if (Hierarchy.Num() == 0)
        return;

    AActor *Leader = Hierarchy[0].Actor;
    if (!Leader)
        return;

    UCharacterDataComponent *Comp = Leader->FindComponentByClass<UCharacterDataComponent>();
    if (!Comp)
        return;

    // Bind to OnHPChanged for weather updates
    Comp->OnHPChanged.AddDynamic(this, &UWeatherStateManager::OnLeaderHPChanged);

    // Bind to OnDied for leadership transfer
    if (bIsTeam0)
        Comp->OnDied.AddDynamic(this, &UWeatherStateManager::OnTeam0LeaderDied);
    else
        Comp->OnDied.AddDynamic(this, &UWeatherStateManager::OnTeam1LeaderDied);

    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Bound to %s leader: %s (WorldStats: %d, Element: %s)"),
           bIsTeam0 ? TEXT("Team0") : TEXT("Team1"),
           *Leader->GetName(),
           Hierarchy[0].TotalWorldStats,
           *UEnum::GetValueAsString(Hierarchy[0].Element));
}

void UWeatherStateManager::RecalculateWeather()
{
    AActor *Team0Leader = GetCurrentLeader(Team0Hierarchy);
    AActor *Team1Leader = GetCurrentLeader(Team1Hierarchy);

    float Team0Dominance = 0.5f;
    float Team1Dominance = 0.5f;

    if (Team0Leader)
    {
        if (UCharacterDataComponent *Comp = Team0Leader->FindComponentByClass<UCharacterDataComponent>())
        {
            Team0Dominance = Comp->MaxHP > 0 ? (float)Comp->CurrentHP / (float)Comp->MaxHP : 0.0f;
        }
    }

    if (Team1Leader)
    {
        if (UCharacterDataComponent *Comp = Team1Leader->FindComponentByClass<UCharacterDataComponent>())
        {
            Team1Dominance = Comp->MaxHP > 0 ? (float)Comp->CurrentHP / (float)Comp->MaxHP : 0.0f;
        }
    }

    float BlendValue = Team0Dominance - Team1Dominance;

    UPrimaryDataAsset *Team0DA = ResolveWeatherDA(Team0Hierarchy);
    UPrimaryDataAsset *Team1DA = ResolveWeatherDA(Team1Hierarchy);

    OnWeatherChanged.Broadcast(Team0DA, Team1DA, BlendValue);

    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Weather updated - T0: %s, T1: %s, Blend: %.2f"),
           Team0DA ? *Team0DA->GetName() : TEXT("None"),
           Team1DA ? *Team1DA->GetName() : TEXT("None"),
           BlendValue);
}

void UWeatherStateManager::OnLeaderHPChanged(int32 CurrentHP, int32 MaxHP)
{
    RecalculateWeather();
}

void UWeatherStateManager::OnTeam0LeaderDied(AActor *DeadActor)
{
    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Team0 leader died: %s - transferring leadership"), *DeadActor->GetName());

    // Remove dead leader from hierarchy
    Team0Hierarchy.RemoveAll([DeadActor](const FLeadershipEntry &Entry)
                             { return Entry.Actor == DeadActor; });

    // Bind to new leader if one exists
    BindToLeader(Team0Hierarchy, true);
    RecalculateWeather();
}

void UWeatherStateManager::OnTeam1LeaderDied(AActor *DeadActor)
{
    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Team1 leader died: %s - transferring leadership"), *DeadActor->GetName());

    Team1Hierarchy.RemoveAll([DeadActor](const FLeadershipEntry &Entry)
                             { return Entry.Actor == DeadActor; });

    BindToLeader(Team1Hierarchy, false);
    RecalculateWeather();
}

int32 UWeatherStateManager::GetTotalWorldStats(AActor *Actor) const
{
    if (!Actor)
        return 0;

    UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!Comp || !Comp->CharacterData)
        return 0;

    return Comp->CharacterData->WorldMindLevel +
           Comp->CharacterData->WorldBodyLevel +
           Comp->CharacterData->WorldSpiritLevel;
}

AActor *UWeatherStateManager::GetCurrentLeader(const TArray<FLeadershipEntry> &Hierarchy) const
{
    if (Hierarchy.Num() == 0)
        return nullptr;
    return Hierarchy[0].Actor;
}

UPrimaryDataAsset *UWeatherStateManager::ResolveWeatherDA(const TArray<FLeadershipEntry> &Hierarchy) const
{
    AActor *Leader = GetCurrentLeader(Hierarchy);
    if (!Leader)
        return nullptr;

    UCharacterDataComponent *Comp = Leader->FindComponentByClass<UCharacterDataComponent>();
    if (!Comp || !Comp->CharacterData)
        return nullptr;

    UCharacterData *Data = Comp->CharacterData;

    // Generic and Resonator have no weather influence
    if (Data->CharacterClass == ECharacterClass::Generic ||
        Data->CharacterClass == ECharacterClass::Resonator)
        return nullptr;

    // Return the equipped variant from the Cosmetics asset, if any.
    if (Data->Cosmetics && Data->Cosmetics->EquippedWeatherVariant)
        return Data->Cosmetics->EquippedWeatherVariant;

    // No equipped variant — return null, sky stays as level default
    return nullptr;
}
