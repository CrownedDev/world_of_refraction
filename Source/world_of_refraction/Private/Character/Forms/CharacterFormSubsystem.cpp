// CharacterFormSubsystem.cpp

#include "Character/Forms/CharacterFormSubsystem.h"

#include "Combat/TurnManager.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Loadout/FCombatLoadout.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/TrialRunSubsystem.h"
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

bool UCharacterFormSubsystem::IsInFight() const
{
    const UTurnManager *TurnManager = GetGameInstance()->GetSubsystem<UTurnManager>();
    return TurnManager && TurnManager->IsCombatActive();
}

bool UCharacterFormSubsystem::IsAtSwitchSpot() const
{
    if (bAtRestSpot)
    {
        return true;
    }
    const UTrialRunSubsystem *TrialRun = GetGameInstance()->GetSubsystem<UTrialRunSubsystem>();
    return TrialRun && TrialRun->IsHubWorld(GetGameInstance()->GetWorld());
}

EFormSwitchRefusal UCharacterFormSubsystem::TryWearForm(UCharacterFormData *Form)
{
    const bool bUnlocked = !Form || IsUnlocked(Form);
    const EFormSwitchRefusal Refusal = CharacterFormSwitch::CanSwitch(bUnlocked, IsInFight(), IsAtSwitchSpot());
    if (Refusal != EFormSwitchRefusal::None)
    {
        UE_LOG(LogWoR, Log, TEXT("[CharacterForms] wear %s refused: %s"), *GetNameSafe(Form), *UEnum::GetValueAsString(Refusal));
        return Refusal;
    }
    if (WornForm != Form)
    {
        WornForm = Form;
        UE_LOG(LogWoR, Log, TEXT("[CharacterForms] now wearing %s"), Form ? *Form->GetName() : TEXT("the base form"));
        // B-279c: a skin comes with its authored outfit (CharacterCustomisation.md §3); taking it off keeps the outfit.
        // The outfit is announced after the skin, so the look never pairs the new outfit with the old skin.
        const bool bOutfitChanges = Form && !Form->AuthoredOutfit.IsEmpty();
        if (bOutfitChanges)
        {
            CurrentOutfit = Form->AuthoredOutfit;
        }
        OnWornFormChanged.Broadcast(WornForm);
        if (bOutfitChanges)
        {
            // TODO: the player state's ApplyFormLook already dresses the new outfit on the skin event; this second
            // pass redresses it (six loads). Fold into one event when the look moves to the PlayerState (D-064).
            OnOutfitChanged.Broadcast();
        }
    }
    return EFormSwitchRefusal::None;
}

bool UCharacterFormSubsystem::TryWearOutfitPiece(UOutfitPieceData *Piece)
{
    // Outfits change where skins do (the hub, a rest spot), never in a fight.
    if (!Piece || CharacterFormSwitch::CanSwitch(true, IsInFight(), IsAtSwitchSpot()) != EFormSwitchRefusal::None)
    {
        UE_LOG(LogWoR, Log, TEXT("[Outfit] wear %s refused"), *GetNameSafe(Piece));
        return false;
    }
    CurrentOutfit.SetPiece(Piece);
    UE_LOG(LogWoR, Log, TEXT("[Outfit] now wearing %s as %s"), *Piece->GetName(), *UEnum::GetValueAsString(Piece->Slot));
    OnOutfitChanged.Broadcast();
    return true;
}

bool UCharacterFormSubsystem::TryClearOutfitSlot(EOutfitSlot Slot)
{
    if (CharacterFormSwitch::CanSwitch(true, IsInFight(), IsAtSwitchSpot()) != EFormSwitchRefusal::None)
    {
        return false;
    }
    CurrentOutfit.ClearSlot(Slot);
    OnOutfitChanged.Broadcast();
    return true;
}

void UCharacterFormSubsystem::RestoreOutfit(const FOutfitSet &Outfit)
{
    CurrentOutfit = Outfit;
    OnOutfitChanged.Broadcast();
}

bool UCharacterFormSubsystem::TrySetBaseForm(EPlayerBaseForm InBaseForm)
{
    // Q-279b: the base body changes where skins do (Elden Ring's mirror sits in the hub).
    if (CharacterFormSwitch::CanSwitch(true, IsInFight(), IsAtSwitchSpot()) != EFormSwitchRefusal::None)
    {
        return false;
    }
    if (BaseForm != InBaseForm)
    {
        BaseForm = InBaseForm;
        UE_LOG(LogWoR, Log, TEXT("[CharacterForms] base form %s"), *UEnum::GetValueAsString(BaseForm));
        OnWornFormChanged.Broadcast(WornForm);
    }
    return true;
}

const UCharacterData *UCharacterFormSubsystem::GetSkinCharacterFor(const UCharacterData *Wearer) const
{
    if (!WornForm || !Wearer)
    {
        return nullptr;
    }
    const UPartySessionSubsystem *PartySession = GetGameInstance()->GetSubsystem<UPartySessionSubsystem>();
    const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr;
    if (!Party)
    {
        return nullptr;
    }
    const FPartyMember *Player = Party->Members.FindByPredicate([](const FPartyMember &Member) { return !Member.bRunOnly && Member.CharacterData; });
    return Player && Player->CharacterData == Wearer ? WornForm->Character.Get() : nullptr;
}

UCharacterFormData *UCharacterFormSubsystem::FindFormByName(FName FormName) const
{
    const TObjectPtr<UCharacterFormData> *Found = Forms.FindByPredicate([FormName](const UCharacterFormData *Form) { return Form && Form->GetFName() == FormName; });
    return Found ? Found->Get() : nullptr;
}

UCharacterFormData *UCharacterFormSubsystem::FindFormForCharacter(const UCharacterData *Character) const
{
    if (!Character)
    {
        return nullptr;
    }
    const TObjectPtr<UCharacterFormData> *Found = Forms.FindByPredicate([Character](const UCharacterFormData *Form) { return Form && Form->Character == Character; });
    return Found ? Found->Get() : nullptr;
}

void UCharacterFormSubsystem::RestoreWorn(FName FormName, EPlayerBaseForm InBaseForm)
{
    BaseForm = InBaseForm;
    UCharacterFormData *Form = FormName.IsNone() ? nullptr : FindFormByName(FormName);
    if (Form && !IsUnlocked(Form))
    {
        Form = nullptr;
    }
    if (!FormName.IsNone() && !Form)
    {
        UE_LOG(LogWoR, Warning, TEXT("[CharacterForms] saved skin %s is unknown or locked; wearing the base form"), *FormName.ToString());
    }
    WornForm = Form;
    OnWornFormChanged.Broadcast(WornForm);
}

FString UCharacterFormSubsystem::GetFormsString() const
{
    FString Out = FString::Printf(TEXT("[CharacterForms] %d form(s); wearing %s, base %s"), Forms.Num(),
                                  WornForm ? *WornForm->GetName() : TEXT("no skin"), *UEnum::GetValueAsString(BaseForm));
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
