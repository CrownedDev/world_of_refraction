// FItemCrystalInventory.cpp
// Item crystal inventory implementation

#include "FItemCrystalInventory.h"
#include "ItemData.h"

bool FItemCrystalInventory::CanAddCrystal(UItemData *Crystal) const
{
    if (!Crystal)
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    return CanAddToTier(Tier);
}

bool FItemCrystalInventory::AddCrystal(UItemData *Crystal)
{
    if (!CanAddCrystal(Crystal))
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    GetMutableArrayForTier(Tier).Add(Crystal);
    return true;
}

bool FItemCrystalInventory::RemoveCrystal(UItemData *Crystal)
{
    if (!Crystal)
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    return GetMutableArrayForTier(Tier).Remove(Crystal) > 0;
}

bool FItemCrystalInventory::HasCrystal(UItemData *Crystal) const
{
    if (!Crystal)
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    return GetArrayForTier(Tier).Contains(Crystal);
}

TArray<UItemData *> FItemCrystalInventory::GetCrystalsOfType(ECrystalType Type) const
{
    TArray<UItemData *> Result;

    // Check all tier arrays
    auto CheckArray = [&](const TArray<UItemData *> &Arr)
    {
        for (UItemData *Crystal : Arr)
        {
            if (Crystal && Crystal->CrystalType == Type)
            {
                Result.Add(Crystal);
            }
        }
    };

    CheckArray(CrystalsF);
    CheckArray(CrystalsE);
    CheckArray(CrystalsD);
    CheckArray(CrystalsC);
    CheckArray(CrystalsB);
    CheckArray(CrystalsA);
    CheckArray(CrystalsS);

    return Result;
}

TArray<UItemData *> FItemCrystalInventory::GetEvolutionCrystals() const
{
    TArray<UItemData *> Result;

    auto CheckArray = [&](const TArray<UItemData *> &Arr)
    {
        for (UItemData *Crystal : Arr)
        {
            if (Crystal && Crystal->bIsEvolutionCrystal)
            {
                Result.Add(Crystal);
            }
        }
    };

    CheckArray(CrystalsF);
    CheckArray(CrystalsE);
    CheckArray(CrystalsD);
    CheckArray(CrystalsC);
    CheckArray(CrystalsB);
    CheckArray(CrystalsA);
    CheckArray(CrystalsS);

    return Result;
}

TArray<UItemData *> FItemCrystalInventory::GetItemCrystals() const
{
    TArray<UItemData *> Result;

    auto CheckArray = [&](const TArray<UItemData *> &Arr)
    {
        for (UItemData *Crystal : Arr)
        {
            if (Crystal && !Crystal->bIsEvolutionCrystal)
            {
                Result.Add(Crystal);
            }
        }
    };

    CheckArray(CrystalsF);
    CheckArray(CrystalsE);
    CheckArray(CrystalsD);
    CheckArray(CrystalsC);
    CheckArray(CrystalsB);
    CheckArray(CrystalsA);
    CheckArray(CrystalsS);

    return Result;
}