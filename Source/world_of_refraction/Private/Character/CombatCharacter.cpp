// CombatCharacter.cpp

#include "Character/CombatCharacter.h"

#include "Character/BattleConfigComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Currency/CurrencyComponent.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "Infusion/InfusionVFXComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"

ACombatCharacter::ACombatCharacter()
{
    // CharacterDataComponent stays first: native components run BeginPlay in
    // constructor-declaration order, and its cascade seeds what the rest read.
    CharacterDataComponent = CreateDefaultSubobject<UCharacterDataComponent>(TEXT("CharacterDataComponent"));
    WeaponMeshComponent = CreateDefaultSubobject<UWeaponMeshComponent>(TEXT("WeaponMeshComponent"));
    CurrencyComponent = CreateDefaultSubobject<UCurrencyComponent>(TEXT("CurrencyComponent"));
    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));
    CrystalInventoryComponent = CreateDefaultSubobject<UCrystalInventoryComponent>(TEXT("CrystalInventoryComponent"));
    EvolutionInventoryComponent = CreateDefaultSubobject<UEvolutionInventoryComponent>(TEXT("EvolutionInventoryComponent"));
    InfusionVFXComponent = CreateDefaultSubobject<UInfusionVFXComponent>(TEXT("InfusionVFXComponent"));
    LoadoutComponent = CreateDefaultSubobject<ULoadoutComponent>(TEXT("LoadoutComponent"));
    BrokenDarknessComponent = CreateDefaultSubobject<UBrokenDarknessManager>(TEXT("BrokenDarknessComponent"));
    BattleConfigComponent = CreateDefaultSubobject<UBattleConfigComponent>(TEXT("BattleConfigComponent"));
}
