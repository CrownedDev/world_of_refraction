#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Skills/Definitions/ESpellElement.h"
#include "WeatherStateManager.generated.h"

USTRUCT(BlueprintType)
struct FLeadershipEntry
{
    GENERATED_BODY()

    UPROPERTY()
    AActor *Actor = nullptr;

    UPROPERTY()

    int32 TotalWorldStats = 0;

    UPROPERTY()
    ESpellElement Element = ESpellElement::None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWeatherChanged,
                                               UPrimaryDataAsset *, Team0WeatherDA,
                                               UPrimaryDataAsset *, Team1WeatherDA,
                                               float, BlendValue);

UCLASS()
class WORLD_OF_REFRACTION_API UWeatherStateManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;
    virtual void Deinitialize() override;

    // ========================================
    // COMBAT SETUP
    // ========================================

    UFUNCTION(BlueprintCallable, Category = "Weather")
    void InitialiseLeaders(const TArray<AActor *> &Team0, const TArray<AActor *> &Team1);

    UFUNCTION(BlueprintCallable, Category = "Weather")
    void EndCombat();

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Weather")
    FOnWeatherChanged OnWeatherChanged;

    // ========================================
    // DEBUG
    // ========================================

    /** Snapshot of weather state — logs both teams' leaders, classes, HP %,
     *  resolved DA, current BlendValue, last-broadcast BlendValue, and the
     *  listener count on OnWeatherChanged. The listener count is the critical
     *  diagnostic: if it reports 0 during combat, no consumer is bound and the
     *  broadcast goes nowhere. */
    /** Console trigger: wor.PrintWeather (in the .cpp). No CallInEditor — a
     *  UGameInstanceSubsystem has no Details panel to host a button. */
    UFUNCTION(BlueprintCallable, Category = "Weather|Debug")
    void PrintWeatherState() const;

    /** Same content as PrintWeatherState, returned as FString for UI / inspection
     *  use. PrintWeatherState delegates to this then UE_LOGs. */
    UFUNCTION(BlueprintPure, Category = "Weather|Debug")
    FString GetWeatherStateString() const;

private:
    // Leadership hierarchies (sorted by world stats, highest first)
    TArray<FLeadershipEntry> Team0Hierarchy;
    TArray<FLeadershipEntry> Team1Hierarchy;

    /** Last BlendValue passed to OnWeatherChanged.Broadcast. Diagnostic only —
     *  written by RecalculateWeather, read by PrintWeatherState. Defaults to
     *  0.0f and is not reset across combats so a between-encounter snapshot
     *  still reflects the last value broadcast. */
    float LastBroadcastBlendValue = 0.0f;

    // ========================================
    // INTERNAL
    // ========================================

    TArray<FLeadershipEntry> BuildHierarchy(const TArray<AActor *> &Team);

    /** Subscribe to OnHPChanged and OnDied on every member of the team. The
     *  team-HP signal needs damage on any member to trigger a recompute, so
     *  the binding is whole-team rather than leader-only. EndCombat mirrors
     *  this with a matching iterate-and-unbind. */
    void BindToTeam(const TArray<FLeadershipEntry> &Hierarchy, bool bIsTeam0);

    void RecalculateWeather();

    /** Sum of CurrentHP / sum of MaxHP across alive members only (bIsAlive
     *  filter). Returns 0.0f if no member is alive. Reused by both
     *  RecalculateWeather (broadcast path) and GetWeatherStateString
     *  (diagnostic snapshot) so they cannot diverge. */
    float ComputeTeamHPPercent(const TArray<FLeadershipEntry> &Hierarchy) const;

    /** Map two team-HP percents to the broadcast BlendValue: signed gap →
     *  deadzone → linear ramp → ±1.0 clamp. See WEATHER_DEADZONE_GAP /
     *  WEATHER_RAMP_END_GAP in WeatherStateManager.cpp for the tuning. */
    float ComputeBlendValue(float Team0Percent, float Team1Percent) const;

    UFUNCTION()
    void OnTeam0MemberDied(AActor *DeadActor);

    UFUNCTION()
    void OnTeam1MemberDied(AActor *DeadActor);

    UFUNCTION()
    void OnTeamMemberHPChanged(int32 NewHP, int32 MaxHP);

    UPrimaryDataAsset *ResolveWeatherDA(const TArray<FLeadershipEntry> &Hierarchy) const;

    int32 GetTotalWorldStats(AActor *Actor) const;

    /** First ALIVE entry in the sorted hierarchy. Hierarchy is sorted descending
     *  by world stats and is NOT pruned on death (skip-the-dead) — iteration
     *  finds the highest-statted alive actor, which lets a resurrected leader
     *  reclaim the role naturally. Returns nullptr if no entry is alive. */
    AActor *GetCurrentLeader(const TArray<FLeadershipEntry> &Hierarchy) const;
};