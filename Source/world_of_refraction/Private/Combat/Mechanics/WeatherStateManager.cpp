#include "Combat/Mechanics/WeatherStateManager.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Character/CosmeticsData.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"

namespace
{
    /** Absolute team-HP gap below which the broadcast BlendValue is exactly
     *  0.0f. BP_WeatherController treats this as neutral / default sky. */
    static constexpr float WEATHER_DEADZONE_GAP = 0.05f;

    /** Absolute team-HP gap at or above which the broadcast BlendValue
     *  saturates at ±1.0f (full intensity). Between the deadzone and this
     *  value the magnitude ramps linearly. */
    static constexpr float WEATHER_RAMP_END_GAP = 0.20f;
}

void UWeatherStateManager::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    LocalPartyHierarchy.Empty();
    OpposingPartyHierarchy.Empty();
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

void UWeatherStateManager::InitialiseLeaders(const TArray<AActor *> &LocalParty, const TArray<AActor *> &OpposingParty)
{
    EndCombat();

    LocalPartyHierarchy = BuildHierarchy(LocalParty);
    OpposingPartyHierarchy = BuildHierarchy(OpposingParty);

    BindToTeam(LocalPartyHierarchy, true);
    BindToTeam(OpposingPartyHierarchy, false);

    RecalculateWeather();

    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Leaders initialised - LocalParty: %s, OpposingParty: %s"),
           LocalPartyHierarchy.Num() > 0 ? *LocalPartyHierarchy[0].Actor->GetName() : TEXT("None"),
           OpposingPartyHierarchy.Num() > 0 ? *OpposingPartyHierarchy[0].Actor->GetName() : TEXT("None"));
}

void UWeatherStateManager::EndCombat()
{
    // Iterate every entry on both teams and remove the OnHPChanged + OnDied
    // subscriptions added by BindToTeam. Defensive null-guards on actor and
    // component for the (rare) case an actor was destroyed mid-combat without
    // notifying us.
    for (const FLeadershipEntry &Entry : LocalPartyHierarchy)
    {
        if (!Entry.Actor)
            continue;
        if (UCharacterDataComponent *Comp = Entry.Actor->FindComponentByClass<UCharacterDataComponent>())
        {
            Comp->OnHPChanged.RemoveDynamic(this, &UWeatherStateManager::OnTeamMemberHPChanged);
            Comp->OnDied.RemoveDynamic(this, &UWeatherStateManager::OnLocalPartyMemberDied);
        }
    }

    for (const FLeadershipEntry &Entry : OpposingPartyHierarchy)
    {
        if (!Entry.Actor)
            continue;
        if (UCharacterDataComponent *Comp = Entry.Actor->FindComponentByClass<UCharacterDataComponent>())
        {
            Comp->OnHPChanged.RemoveDynamic(this, &UWeatherStateManager::OnTeamMemberHPChanged);
            Comp->OnDied.RemoveDynamic(this, &UWeatherStateManager::OnOpposingPartyMemberDied);
        }
    }

    LocalPartyHierarchy.Empty();
    OpposingPartyHierarchy.Empty();

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
        Entry.Element = ESpellElement::None;

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

void UWeatherStateManager::BindToTeam(const TArray<FLeadershipEntry> &Hierarchy, bool bIsLocalParty)
{
    int32 BoundCount = 0;
    for (const FLeadershipEntry &Entry : Hierarchy)
    {
        if (!Entry.Actor)
            continue;
        UCharacterDataComponent *Comp = Entry.Actor->FindComponentByClass<UCharacterDataComponent>();
        if (!Comp)
            continue;

        Comp->OnHPChanged.AddDynamic(this, &UWeatherStateManager::OnTeamMemberHPChanged);
        if (bIsLocalParty)
            Comp->OnDied.AddDynamic(this, &UWeatherStateManager::OnLocalPartyMemberDied);
        else
            Comp->OnDied.AddDynamic(this, &UWeatherStateManager::OnOpposingPartyMemberDied);
        ++BoundCount;
    }

    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] Bound to %s (%d/%d members)"),
           bIsLocalParty ? TEXT("LocalParty") : TEXT("OpposingParty"),
           BoundCount,
           Hierarchy.Num());
}

void UWeatherStateManager::RecalculateWeather()
{
    const float LocalPartyPercent = ComputeTeamHPPercent(LocalPartyHierarchy);
    const float OpposingPartyPercent = ComputeTeamHPPercent(OpposingPartyHierarchy);
    const float BlendValue = ComputeBlendValue(LocalPartyPercent, OpposingPartyPercent);

    UPrimaryDataAsset *LocalPartyDA = ResolveWeatherDA(LocalPartyHierarchy);
    UPrimaryDataAsset *OpposingPartyDA = ResolveWeatherDA(OpposingPartyHierarchy);

    LastBroadcastBlendValue = BlendValue;
    OnWeatherChanged.Broadcast(LocalPartyDA, OpposingPartyDA, BlendValue);

    UE_LOG(LogTemp, Log,
           TEXT("[WeatherStateManager] Weather updated - T0: %s (%.0f%%), T1: %s (%.0f%%), Blend: %.2f"),
           LocalPartyDA ? *LocalPartyDA->GetName() : TEXT("None"),
           LocalPartyPercent * 100.0f,
           OpposingPartyDA ? *OpposingPartyDA->GetName() : TEXT("None"),
           OpposingPartyPercent * 100.0f,
           BlendValue);
}

void UWeatherStateManager::OnTeamMemberHPChanged(int32 NewHP, int32 MaxHP)
{
    RecalculateWeather();
}

void UWeatherStateManager::OnLocalPartyMemberDied(AActor *DeadActor)
{
    // Hierarchy is NOT pruned — skip-the-dead lets resurrection re-enter the
    // team-HP sum naturally (ComputeTeamHPPercent filters by bIsAlive). The
    // dead member's binding stays in place; their OnHPChanged simply won't
    // fire again until they're revived.
    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] LocalParty member died: %s"),
           DeadActor ? *DeadActor->GetName() : TEXT("(null)"));
    RecalculateWeather();
}

void UWeatherStateManager::OnOpposingPartyMemberDied(AActor *DeadActor)
{
    UE_LOG(LogTemp, Log, TEXT("[WeatherStateManager] OpposingParty member died: %s"),
           DeadActor ? *DeadActor->GetName() : TEXT("(null)"));
    RecalculateWeather();
}

int32 UWeatherStateManager::GetTotalWorldStats(AActor *Actor) const
{
    if (!Actor)
        return 0;

    UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
    if (!Comp || !Comp->CharacterData)
        return 0;

    // §7 C2: weather priority reads the LIVE world stats (asset + head-start + earned).
    return Comp->GetWorldMind() + Comp->GetWorldBody() + Comp->GetWorldSpirit();
}

AActor *UWeatherStateManager::GetCurrentLeader(const TArray<FLeadershipEntry> &Hierarchy) const
{
    // Hierarchy is sorted descending by world stats and is NOT pruned on death
    // (skip-the-dead lets a resurrected character re-enter the team-HP sum
    // naturally). The "current" leader is the highest-statted ALIVE entry, so
    // iterate to find the first one whose UCharacterDataComponent reports
    // bIsAlive. Returns nullptr when the whole team is dead.
    for (const FLeadershipEntry &Entry : Hierarchy)
    {
        if (!Entry.Actor)
            continue;
        UCharacterDataComponent *Comp = Entry.Actor->FindComponentByClass<UCharacterDataComponent>();
        if (Comp && Comp->bIsAlive)
        {
            return Entry.Actor;
        }
    }
    return nullptr;
}

float UWeatherStateManager::ComputeTeamHPPercent(const TArray<FLeadershipEntry> &Hierarchy) const
{
    // Dead members contribute 0 HP to the numerator but their MaxHP stays in the
    // denominator: losing a teammate is a persistent weather penalty, not a
    // momentary one (a 2/3 team at full survivor HP reads as ~67%, not 100%).
    // Null actor / null component entries are not valid team members and are
    // skipped entirely. Resurrection flips bIsAlive back and the numerator
    // recovers naturally. Whole-team wipe returns 0.0f via the MaxHP > 0 guard.
    int32 SumCurrentHP = 0;
    int32 SumMaxHP = 0;
    for (const FLeadershipEntry &Entry : Hierarchy)
    {
        if (!Entry.Actor)
            continue;
        UCharacterDataComponent *Comp = Entry.Actor->FindComponentByClass<UCharacterDataComponent>();
        if (!Comp)
            continue;
        if (Comp->bIsAlive)
            SumCurrentHP += Comp->CurrentHP;
        SumMaxHP += Comp->MaxHP;
    }
    return SumMaxHP > 0 ? static_cast<float>(SumCurrentHP) / static_cast<float>(SumMaxHP) : 0.0f;
}

float UWeatherStateManager::ComputeBlendValue(float LocalPartyPercent, float OpposingPartyPercent) const
{
    const float SignedGap = LocalPartyPercent - OpposingPartyPercent;
    const float AbsGap = FMath::Abs(SignedGap);

    if (AbsGap < WEATHER_DEADZONE_GAP)
    {
        return 0.0f;
    }

    const float Intensity = FMath::Clamp(
        (AbsGap - WEATHER_DEADZONE_GAP) / (WEATHER_RAMP_END_GAP - WEATHER_DEADZONE_GAP),
        0.0f,
        1.0f);

    // FMath::Sign returns -1 / 0 / +1. The deadzone guard above rules out 0
    // here, so this is always ±1 × Intensity.
    return FMath::Sign(SignedGap) * Intensity;
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

// ========================================
// DEBUG
// ========================================

FString UWeatherStateManager::GetWeatherStateString() const
{
    const bool bCombatActive = (LocalPartyHierarchy.Num() > 0 || OpposingPartyHierarchy.Num() > 0);
    const int32 ListenerCount = OnWeatherChanged.GetAllObjects().Num();

    FString Output;
    Output += TEXT("=== [WeatherStateManager] STATE SNAPSHOT ===\n");
    Output += FString::Printf(TEXT("Combat active: %s\n"), bCombatActive ? TEXT("yes") : TEXT("no"));

    // Listener count is the critical diagnostic — flag it loudly when zero
    // during combat (BP_WeatherController or any other consumer is not bound).
    if (ListenerCount == 0)
    {
        Output += FString::Printf(
            TEXT("Listener count on OnWeatherChanged: 0  *** NO SUBSCRIBERS - broadcasts go nowhere ***\n"));
    }
    else
    {
        Output += FString::Printf(TEXT("Listener count on OnWeatherChanged: %d\n"), ListenerCount);
    }

    // Per-team description: leader line (still useful — shows which DA the
    // resolver picked) plus a team-summary line showing the alive/total
    // members and the HP sums that actually drive ComputeTeamHPPercent.
    auto DescribeTeam = [this, &Output](const TCHAR *Label, const TArray<FLeadershipEntry> &Hierarchy)
    {
        // Leader line — uses GetCurrentLeader (first ALIVE entry post-refactor).
        AActor *Leader = GetCurrentLeader(Hierarchy);
        if (!Leader)
        {
            Output += FString::Printf(TEXT("%s leader: (none alive)\n"), Label);
        }
        else
        {
            UCharacterDataComponent *Comp = Leader->FindComponentByClass<UCharacterDataComponent>();
            if (!Comp || !Comp->CharacterData)
            {
                Output += FString::Printf(TEXT("%s leader: %s (no CharacterDataComponent / CharacterData)\n"),
                                          Label, *Leader->GetName());
            }
            else
            {
                const float LeaderHPPercent = Comp->MaxHP > 0
                                                  ? 100.0f * static_cast<float>(Comp->CurrentHP) / static_cast<float>(Comp->MaxHP)
                                                  : 0.0f;
                const FString ClassName = UEnum::GetValueAsString(Comp->CharacterData->CharacterClass);
                UPrimaryDataAsset *DA = ResolveWeatherDA(Hierarchy);
                const FString DAName = DA ? DA->GetName() : FString(TEXT("nullptr - no weather contribution"));

                Output += FString::Printf(TEXT("%s leader: %s (%s) HP %d/%d (%.1f%%) -> %s\n"),
                                          Label,
                                          *Leader->GetName(),
                                          *ClassName,
                                          Comp->CurrentHP,
                                          Comp->MaxHP,
                                          LeaderHPPercent,
                                          *DAName);
            }
        }

        // Team-summary line — iterates the same way ComputeTeamHPPercent does
        // so the snapshot matches the actual broadcast input.
        int32 AliveCount = 0;
        int32 SumCur = 0;
        int32 SumMax = 0;
        for (const FLeadershipEntry &Entry : Hierarchy)
        {
            if (!Entry.Actor)
                continue;
            UCharacterDataComponent *Comp = Entry.Actor->FindComponentByClass<UCharacterDataComponent>();
            if (!Comp || !Comp->bIsAlive)
                continue;
            ++AliveCount;
            SumCur += Comp->CurrentHP;
            SumMax += Comp->MaxHP;
        }
        const float SumPercent = SumMax > 0
                                     ? 100.0f * static_cast<float>(SumCur) / static_cast<float>(SumMax)
                                     : 0.0f;
        Output += FString::Printf(TEXT("%s totals: %d/%d alive, HP %d/%d (%.1f%%)\n"),
                                  Label, AliveCount, Hierarchy.Num(), SumCur, SumMax, SumPercent);
    };

    DescribeTeam(TEXT("LocalParty"), LocalPartyHierarchy);
    DescribeTeam(TEXT("OpposingParty"), OpposingPartyHierarchy);

    // Recompute the broadcast inputs via the same helpers RecalculateWeather
    // uses, so the snapshot can never disagree with what listeners last saw.
    const float LocalPartyPercent = ComputeTeamHPPercent(LocalPartyHierarchy);
    const float OpposingPartyPercent = ComputeTeamHPPercent(OpposingPartyHierarchy);
    const float CurrentBlend = ComputeBlendValue(LocalPartyPercent, OpposingPartyPercent);
    const float SignedGap = LocalPartyPercent - OpposingPartyPercent;

    Output += FString::Printf(TEXT("Gap: %+.2f -> BlendValue (recomputed now): %+.2f\n"),
                              SignedGap, CurrentBlend);
    Output += FString::Printf(TEXT("Last broadcast BlendValue:                 %+.2f\n"),
                              LastBroadcastBlendValue);
    Output += TEXT("=============================================");

    return Output;
}

// FAutoConsoleCommandWithWorld — NOT UFUNCTION(CallInEditor). A UGameInstanceSubsystem has no
// Details panel, so a CallInEditor button never renders. Console commands resolve from any class.
// "wor." prefix groups them. State is PIE-only, so the chain is null-checked.
namespace
{
    static FAutoConsoleCommandWithWorld GPrintWeatherCmd(
        TEXT("wor.PrintWeather"),
        TEXT("Snapshot weather state: leaders, HP %, resolved DA, blend values, listener count (PIE debug)."),
        FConsoleCommandWithWorldDelegate::CreateLambda(
            [](UWorld *World)
            {
                UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
                UWeatherStateManager *Mgr = GI ? GI->GetSubsystem<UWeatherStateManager>() : nullptr;
                if (Mgr)
                {
                    Mgr->PrintWeatherState();
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Weather] wor.PrintWeather: no subsystem (no PIE world?)."));
                }
            }));
}

void UWeatherStateManager::PrintWeatherState() const
{
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *GetWeatherStateString());

    // Emphasise the zero-subscriber case at Warning level so it stands out in
    // the Output Log even if the snapshot scrolls past. Only escalates when
    // combat is genuinely active — out-of-combat zero is expected.
    const bool bCombatActive = (LocalPartyHierarchy.Num() > 0 || OpposingPartyHierarchy.Num() > 0);
    if (bCombatActive && OnWeatherChanged.GetAllObjects().Num() == 0)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[WeatherStateManager] OnWeatherChanged has NO subscribers while combat is active - BP_WeatherController binding may be missing"));
    }
}
