#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ESpellElement.h"
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
    ESpellElement Element = ESpellElement::Generic;
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

private:
    // Leadership hierarchies (sorted by world stats, highest first)
    TArray<FLeadershipEntry> Team0Hierarchy;
    TArray<FLeadershipEntry> Team1Hierarchy;

    // ========================================
    // INTERNAL
    // ========================================

    TArray<FLeadershipEntry> BuildHierarchy(const TArray<AActor *> &Team);
    void BindToLeader(TArray<FLeadershipEntry> &Hierarchy, bool bIsTeam0);
    void RecalculateWeather();

    UFUNCTION()
    void OnTeam0LeaderDied(AActor *DeadActor);

    UFUNCTION()
    void OnTeam1LeaderDied(AActor *DeadActor);

    UFUNCTION()
    void OnLeaderHPChanged(int32 CurrentHP, int32 MaxHP);

    UPrimaryDataAsset *ResolveWeatherDA(const TArray<FLeadershipEntry> &Hierarchy) const;

    int32 GetTotalWorldStats(AActor *Actor) const;
    AActor *GetCurrentLeader(const TArray<FLeadershipEntry> &Hierarchy) const;
};