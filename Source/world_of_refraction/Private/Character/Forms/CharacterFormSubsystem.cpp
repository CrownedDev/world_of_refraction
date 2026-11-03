// CharacterFormSubsystem.cpp

#include "Character/Forms/CharacterFormSubsystem.h"

#include "Engine/AssetManager.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "WoRLog.h"

void UCharacterFormSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
    Super::Initialize(Collection);
    if (!UAssetManager::IsInitialized())
    {
        return;
    }
    TArray<FPrimaryAssetId> FormIds;
    UAssetManager::Get().GetPrimaryAssetIdList(UCharacterFormData::FormAssetType, FormIds);
    for (const FPrimaryAssetId &Id : FormIds)
    {
        const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
        RegisterForm(Cast<UCharacterFormData>(Path.TryLoad()));
    }
    UE_LOG(LogWoR, Log, TEXT("[CharacterForms] %d form(s) known"), Forms.Num());
}

void UCharacterFormSubsystem::RegisterForm(UCharacterFormData *Form)
{
    if (Form)
    {
        Forms.AddUnique(Form);
    }
}

TArray<FCharacterFormPiece> UCharacterFormSubsystem::GatherEquippedPieces(const AActor &Fighter)
{
    TArray<FCharacterFormPiece> Pieces;
    const ULoadoutComponent *Loadout = Fighter.FindComponentByClass<ULoadoutComponent>();
    if (!Loadout)
    {
        return Pieces;
    }
    for (const FLoadoutSlot &Slot : Loadout->GetActiveLoadout().Slots)
    {
        if (Slot.Gear.Asset)
        {
            Pieces.Add({Slot.Gear.Asset, Slot.Gear.Tier, true});
        }
        for (const USpellData *Spell : Slot.Gear.AssignedSpells)
        {
            Pieces.Add({Spell, EItemTier::E_Tier, false});
        }
        for (const USpellData *Spell : Slot.SpellOverrides)
        {
            Pieces.Add({Spell, EItemTier::E_Tier, false});
        }
        for (const UAbilityData *Ability : Slot.AssignedAbilities)
        {
            Pieces.Add({Ability, EItemTier::E_Tier, false});
        }
    }
    return Pieces;
}

void UCharacterFormSubsystem::RecordFightWon(const TArray<AActor *> &Fighters)
{
    TArray<TArray<FCharacterFormPiece>> FighterPieces;
    for (const AActor *Fighter : Fighters)
    {
        if (Fighter)
        {
            FighterPieces.Add(GatherEquippedPieces(*Fighter));
        }
    }
    RecordFightWonWith(FighterPieces);
}

void UCharacterFormSubsystem::RecordFightWonWith(const TArray<TArray<FCharacterFormPiece>> &FighterPieces)
{
    for (UCharacterFormData *Form : Forms)
    {
        if (!Form || IsUnlocked(Form))
        {
            continue;
        }
        const TArray<FCharacterFormPiece> Reference = Form->GetReferencePieces();
        float Fill = 0.0f;
        for (const TArray<FCharacterFormPiece> &Worn : FighterPieces)
        {
            Fill = FMath::Max(Fill, CharacterFormMeter::GetFightFill(Reference, Worn));
        }
        if (Fill <= 0.0f)
        {
            continue;
        }
        float &Meter = Progress.FindOrAdd(Form->GetFName());
        Meter = FMath::Min(Meter + Fill, CharacterFormMeter::FULL);
        UE_LOG(LogWoR, Verbose, TEXT("[CharacterForms] %s +%.2f -> %.2f"), *Form->GetName(), Fill, Meter);
        if (Meter >= CharacterFormMeter::FULL)
        {
            UE_LOG(LogWoR, Log, TEXT("[CharacterForms] %s unlocked"), *Form->GetName());
            OnFormUnlocked.Broadcast(Form);
        }
    }
}

float UCharacterFormSubsystem::GetProgress(const UCharacterFormData *Form) const
{
    return Form ? Progress.FindRef(Form->GetFName()) : 0.0f;
}

bool UCharacterFormSubsystem::IsUnlocked(const UCharacterFormData *Form) const
{
    return GetProgress(Form) >= CharacterFormMeter::FULL;
}

void UCharacterFormSubsystem::RestoreProgress(const TMap<FName, float> &InProgress)
{
    Progress = InProgress;
}

FString UCharacterFormSubsystem::GetFormsString() const
{
    FString Out = FString::Printf(TEXT("[CharacterForms] %d form(s)"), Forms.Num());
    for (const UCharacterFormData *Form : Forms)
    {
        if (Form)
        {
            Out += FString::Printf(TEXT("\n  %s: %.0f%%%s"), *Form->GetName(), GetProgress(Form) * 100.0f,
                                   IsUnlocked(Form) ? TEXT(" (unlocked)") : TEXT(""));
        }
    }
    return Out;
}
