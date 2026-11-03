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
/** B-279b: the local player's skin changed (Form null: back to the base form, or the base body changed). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWornFormChanged, UCharacterFormData *, Form);

class UCharacterData;

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

    // ==================== SKINS (B-279b) ====================

    /** Wear Form's skin (null: back to the base form). Refused when locked, in a fight, or away from the
     *  hub / a rest spot (CharacterFormSwitch::CanSwitch); None when it was worn. */
    EFormSwitchRefusal TryWearForm(UCharacterFormData *Form);

    UCharacterFormData *GetWornForm() const { return WornForm; }

    /** Manny or Quinn: the body under no skin. The player's pick; changes where skins do (Q-279b). */
    EPlayerBaseForm GetBaseForm() const { return BaseForm; }
    bool TrySetBaseForm(EPlayerBaseForm InBaseForm);

    /** B-187's rest spot sets this while the player is at one (with the hub, a place to switch). */
    void SetAtRestSpot(bool bInAtRestSpot) { bAtRestSpot = bInAtRestSpot; }

    /** D-033 through a skin: the character whose bound gear Wearer gets the bonus of. The worn skin's
     *  character when Wearer is the player's own character (the local party's first permanent member),
     *  else null. // TODO: co-op reads each player's form from their PlayerState (B-228+). */
    const UCharacterData *GetSkinCharacterFor(const UCharacterData *Wearer) const;

    const TArray<TObjectPtr<UCharacterFormData>> &GetForms() const { return Forms; }
    UCharacterFormData *FindFormByName(FName FormName) const;

    /** Save / load (USaveSubsystem): the worn form by asset name (None: no skin) and the base body. A form
     *  no longer known or unlocked loads as no skin. */
    FName GetWornFormName() const { return WornForm ? WornForm->GetFName() : NAME_None; }
    void RestoreWorn(FName FormName, EPlayerBaseForm InBaseForm);

    UPROPERTY(BlueprintAssignable, Category = "Forms")
    FOnWornFormChanged OnWornFormChanged;

    /** Debug: every known form with its meter. */
    FString GetFormsString() const;

private:
    bool IsInFight() const;
    bool IsAtSwitchSpot() const;

    UPROPERTY()
    TArray<TObjectPtr<UCharacterFormData>> Forms;

    TMap<FName, float> Progress;

    UPROPERTY()
    TObjectPtr<UCharacterFormData> WornForm;

    EPlayerBaseForm BaseForm = EPlayerBaseForm::Manny;
    bool bAtRestSpot = false;
};
