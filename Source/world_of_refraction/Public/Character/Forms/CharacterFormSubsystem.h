// CharacterFormSubsystem.h
// B-279: the player's loadout-match meter per character form (Design/CharacterForms.md §1). Each
// fight won adds what the best-matching party member wore (CharacterFormMeter::GetFightFill); at 100%
// the form unlocks. Account state like the pool (saved through USaveSubsystem, D-059); it moves to
// the player state with the co-op plumbing.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Character/Forms/CharacterFormData.h"
#include "CharacterFormSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterFormUnlocked, UCharacterFormData *, Form);

UCLASS()
class WORLD_OF_REFRACTION_API UCharacterFormSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase &Collection) override;

    /** Adds a form to the known set (the asset manager's CharacterForm assets are added at start). */
    void RegisterForm(UCharacterFormData *Form);

    /** A fight won by Fighters (the local party): each locked form gains the best fill among them. */
    void RecordFightWon(const TArray<AActor *> &Fighters);

    /** RecordFightWon on pieces already gathered, one list per fighter. */
    void RecordFightWonWith(const TArray<TArray<FCharacterFormPiece>> &FighterPieces);

    /** What Fighter's active loadout wears: its gear at instance tier, the gear's spells, the slots' spell
     *  overrides and assigned abilities. Evolutions aren't on slots yet. // TODO: evolutions */
    static TArray<FCharacterFormPiece> GatherEquippedPieces(const AActor &Fighter);

    UFUNCTION(BlueprintPure, Category = "Forms")
    float GetProgress(const UCharacterFormData *Form) const;

    UFUNCTION(BlueprintPure, Category = "Forms")
    bool IsUnlocked(const UCharacterFormData *Form) const;

    /** Save / load (USaveSubsystem): progress by form asset name. */
    const TMap<FName, float> &GetProgressByName() const { return Progress; }
    void RestoreProgress(const TMap<FName, float> &InProgress);

    UPROPERTY(BlueprintAssignable, Category = "Forms")
    FOnCharacterFormUnlocked OnFormUnlocked;

    /** Debug: every known form with its meter. */
    FString GetFormsString() const;

private:
    UPROPERTY()
    TArray<TObjectPtr<UCharacterFormData>> Forms;

    TMap<FName, float> Progress;
};
