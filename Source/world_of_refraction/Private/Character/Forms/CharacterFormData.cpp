// CharacterFormData.cpp

#include "Character/Forms/CharacterFormData.h"

#include "Character/CharacterData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

#define LOCTEXT_NAMESPACE "CharacterForms"

const FPrimaryAssetType UCharacterFormData::FormAssetType(TEXT("CharacterForm"));

EFormSwitchRefusal CharacterFormSwitch::CanSwitch(bool bUnlocked, bool bInFight, bool bAtSwitchSpot)
{
    if (!bUnlocked)
    {
        return EFormSwitchRefusal::Locked;
    }
    if (bInFight)
    {
        return EFormSwitchRefusal::InFight;
    }
    return bAtSwitchSpot ? EFormSwitchRefusal::None : EFormSwitchRefusal::NotAtSwitchSpot;
}

FText CharacterFormSwitch::GetRefusalText(EFormSwitchRefusal Refusal)
{
    switch (Refusal)
    {
    case EFormSwitchRefusal::Locked:
        return LOCTEXT("RefusedLocked", "That form isn't unlocked yet.");
    case EFormSwitchRefusal::InFight:
        return LOCTEXT("RefusedInFight", "You can't change form in a fight.");
    case EFormSwitchRefusal::NotAtSwitchSpot:
        return LOCTEXT("RefusedNotAtSpot", "Change form in the hub or at a rest spot.");
    default:
        return FText::GetEmpty();
    }
}

float CharacterFormMeter::GetFightFill(const TArray<FCharacterFormPiece> &Reference, const TArray<FCharacterFormPiece> &Equipped)
{
    if (Reference.IsEmpty())
    {
        return 0.0f;
    }
    float Credit = 0.0f;
    for (const FCharacterFormPiece &Piece : Reference)
    {
        float Best = 0.0f;
        for (const FCharacterFormPiece &Worn : Equipped)
        {
            if (Worn.Asset != Piece.Asset)
            {
                continue;
            }
            const bool bExact = !Piece.bHasTier || !Worn.bHasTier || Worn.Tier == Piece.Tier;
            Best = FMath::Max(Best, bExact ? FULL : OTHER_TIER_CREDIT);
        }
        Credit += Best;
    }
    return FULL_SET_FILL_PER_FIGHT * Credit / Reference.Num();
}

FPrimaryAssetId UCharacterFormData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(FormAssetType, GetFName());
}

TArray<FCharacterFormPiece> UCharacterFormData::GetReferencePieces() const
{
    TArray<FCharacterFormPiece> Pieces;
    const UInventoryData *Inventory = Character ? Character->Inventory : nullptr;
    if (!Inventory)
    {
        return Pieces;
    }
    const auto AddGear = [&Pieces](const UEquipmentDataBase *Gear)
    {
        if (Gear)
        {
            Pieces.Add({Gear, Gear->Tier, true});
        }
    };
    const auto AddByAsset = [&Pieces](const UObject *Skill)
    {
        if (Skill)
        {
            Pieces.Add({Skill, EItemTier::E_Tier, false}); // TODO: skill / evolution instance tiers once loadouts carry them
        }
    };
    for (const UWeaponData *Weapon : Inventory->Weapons)
    {
        AddGear(Weapon);
    }
    for (const URingData *Ring : Inventory->Rings)
    {
        AddGear(Ring);
    }
    // TODO: evolutions join the reference once loadout slots carry them (the worn side can't see them yet).
    for (const USpellData *Spell : Inventory->Spells)
    {
        AddByAsset(Spell);
    }
    for (const UAbilityData *Ability : Inventory->Abilities)
    {
        AddByAsset(Ability);
    }
    return Pieces;
}

FText UCharacterFormData::GetRevealedLoadoutText() const
{
    TArray<FText> Lines;
    for (const FCharacterFormPiece &Piece : GetReferencePieces())
    {
        // TODO: skills carry no player-facing FText name yet; the gear name is the authored FString (placeholder screen).
        const UEquipmentDataBase *Gear = Cast<UEquipmentDataBase>(Piece.Asset);
        const FText Name = FText::FromString(Gear ? Gear->GetDisplayName() : GetNameSafe(Piece.Asset));
        Lines.Add(Piece.bHasTier ? FText::Format(LOCTEXT("RevealGearRow", "- {0} ({1})"), Name, FText::FromString(TierHelpers::GetTierName(Piece.Tier)))
                                 : FText::Format(LOCTEXT("RevealSkillRow", "- {0}"), Name));
    }
    return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

FString UCharacterFormData::GetFormString() const
{
    const TArray<FCharacterFormPiece> Pieces = GetReferencePieces();
    FString Out = FString::Printf(TEXT("[Form] %s (%s): character %s, %d reference piece(s), %d lore id(s)"), *FormName.ToString(), *GetName(),
                                  Character ? *Character->Name : TEXT("<none>"), Pieces.Num(), LoreIds.Num());
    for (const FCharacterFormPiece &Piece : Pieces)
    {
        Out += FString::Printf(TEXT("\n  %s%s"), *GetNameSafe(Piece.Asset),
                               Piece.bHasTier ? *FString::Printf(TEXT(" (%s)"), *TierHelpers::GetTierName(Piece.Tier)) : TEXT(""));
    }
    return Out;
}

void UCharacterFormData::PrintForm() const
{
    UE_LOG(LogTemp, Log, TEXT("%s"), *GetFormString());
}

#undef LOCTEXT_NAMESPACE
