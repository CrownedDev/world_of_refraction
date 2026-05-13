// EquipmentDataBase.cpp

#include "EquipmentDataBase.h"
#include "ItemData.h"

bool UEquipmentDataBase::IsEvolved() const
{
    return SlottedCrystal && SlottedCrystal->bIsEvolutionCrystal;
}

ESpellElement UEquipmentDataBase::GetCrystalElement() const
{
    if (!SlottedCrystal)
    {
        return ESpellElement::Generic;
    }
    return SlottedCrystal->GetAssociatedElement();
}

#if WITH_EDITOR
EDataValidationResult UEquipmentDataBase::IsDataValid(FDataValidationContext &Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Name.IsEmpty())
    {
        Context.AddWarning(FText::FromString(TEXT("Equipment must have a unique name")));
    }

    const int32 MaxSpells = GetMaxSpells();
    if (DefaultSpells.Num() > MaxSpells)
    {
        Context.AddWarning(FText::FromString(FString::Printf(
            TEXT("DefaultSpells (%d) exceeds max (%d)"),
            DefaultSpells.Num(), MaxSpells)));
    }

    return Result;
}
#endif
