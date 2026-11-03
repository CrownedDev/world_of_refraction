// CharacterFormData.cpp

#include "Character/Forms/CharacterFormData.h"

#include "Character/CharacterData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

const FPrimaryAssetType UCharacterFormData::FormAssetType(TEXT("CharacterForm"));

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
    for (const UEvolutionItemData *Evolution : Inventory->EvolutionEquipment)
    {
        AddByAsset(Evolution); // matched by asset: an evolution's tier lives on its instance
    }
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
