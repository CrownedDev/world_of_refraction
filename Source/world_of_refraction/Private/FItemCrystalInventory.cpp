// FItemCrystalInventory.cpp
// Item crystal inventory implementation

#include "FItemCrystalInventory.h"
#include "EvolutionItemData.h"

bool FItemCrystalInventory::CanAddCrystal(UEvolutionItemData *Crystal) const
{
    if (!Crystal)
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    return CanAddToTier(Tier);
}

bool FItemCrystalInventory::AddCrystal(UEvolutionItemData *Crystal)
{
    if (!CanAddCrystal(Crystal))
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    GetMutableArrayForTier(Tier).Add(Crystal);
    return true;
}

bool FItemCrystalInventory::RemoveCrystal(UEvolutionItemData *Crystal)
{
    if (!Crystal)
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    return GetMutableArrayForTier(Tier).Remove(Crystal) > 0;
}

bool FItemCrystalInventory::HasCrystal(UEvolutionItemData *Crystal) const
{
    if (!Crystal)
    {
        return false;
    }

    EItemTier Tier = Crystal->Tier;
    return GetArrayForTier(Tier).Contains(Crystal);
}

TArray<UEvolutionItemData *> FItemCrystalInventory::GetCrystalsOfType(ECrystalType Type) const
{
    TArray<UEvolutionItemData *> Result;

    // Check all tier arrays
    auto CheckArray = [&](const TArray<UEvolutionItemData *> &Arr)
    {
        for (UEvolutionItemData *Crystal : Arr)
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

TArray<UEvolutionItemData *> FItemCrystalInventory::GetEvolutionCrystals() const
{
    TArray<UEvolutionItemData *> Result;

    auto CheckArray = [&](const TArray<UEvolutionItemData *> &Arr)
    {
        for (UEvolutionItemData *Crystal : Arr)
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

TArray<UEvolutionItemData *> FItemCrystalInventory::GetItemCrystals() const
{
    TArray<UEvolutionItemData *> Result;

    auto CheckArray = [&](const TArray<UEvolutionItemData *> &Arr)
    {
        for (UEvolutionItemData *Crystal : Arr)
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