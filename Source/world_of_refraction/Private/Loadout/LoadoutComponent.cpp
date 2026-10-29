// LoadoutComponent.cpp
// Active combat loadout and runtime battle state implementation

#include "Loadout/LoadoutComponent.h"
#include "Loadout/SourceBreakRules.h"
#include "Loadout/LoadoutConstants.h"
#include "Inventory/InventoryConstants.h"
#include "WoRLog.h"
#include "Loadout/SpellPoolConstants.h"
#include "Inventory/InventoryComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/AbilityData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Combat/Damage/TierPowerConstants.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CosmeticsData.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Skills/Definitions/ElementHelpers.h"
#include "Character/StanceData.h"
#include "Combat/TurnManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/FBrokenCrystalPayload.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

#include "Skills/Definitions/SkillDataBase.h"

#define LOCTEXT_NAMESPACE "LoadoutComponent"

namespace
{
    /** Q-159 = A: utility crystals a loadout rule removes are not destroyed; they go back to the
     *  inventory, as many as fit. An authored loadout's crystals were never taken from the
     *  inventory, so this can add extra ones: a dev-time case Q-159 accepts while the inventory is
     *  the member's own (it refills at each spawn). B-203: while the member shares the party's
     *  carried items (saved), only crystals taken from them (Taken) go back, so none are made up. */
    void LoadoutReturnDroppedCrystals(UInventoryComponent *Inv, const FCrystalId &Id, int32 Count, TMap<FCrystalId, int32> &Taken)
    {
        const UCrystalInventoryComponent *CrystalInv = Inv->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>();
        if (!CrystalInv)
        {
            // An enemy's authored loadout: no crystal inventory to return to.
            UE_LOG(LogWoRLoadout, Verbose, TEXT("No crystal inventory: %d dropped %s not returned"), Count, *ItemIdentity::GetDisplayName(Id));
            return;
        }
        if (CrystalInv->IsSharingItems())
        {
            int32 &TakenOfType = Taken.FindOrAdd(Id);
            Count = FMath::Min(Count, TakenOfType);
            TakenOfType -= FMath::Max(Count, 0);
        }
        if (Count <= 0)
        {
            return;
        }
        int32 Returned = 0;
        while (Returned < Count && Inv->AddCrystal(Id))
        {
            ++Returned;
        }
        if (Returned < Count)
        {
            // Only an unshared inventory refuses (its tier cap); a shared one stores what doesn't fit (D-097).
            UE_LOG(LogWoRLoadout, Warning, TEXT("Inventory full: %d dropped %s lost (the member's own inventory is at its cap)"),
                   Count - Returned, *ItemIdentity::GetDisplayName(Id));
        }
    }

    /** B-138: announces an active-weapon change when the mutator it guards returns, on every path. */
    struct FActiveWeaponChangeScope
    {
        explicit FActiveWeaponChangeScope(ULoadoutComponent &InLoadout) : Loadout(InLoadout) {}
        ~FActiveWeaponChangeScope() { Loadout.NotifyActiveWeaponChanged(); }

        ULoadoutComponent &Loadout;
    };
}

ULoadoutComponent::ULoadoutComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void ULoadoutComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UInventoryComponent *Inv = GetInventoryComponent())
    {
        Inv->OnInventoryChanged.AddUniqueDynamic(this, &ULoadoutComponent::HandleInventoryChanged);
        Inv->OnGearSocketChanged.AddUniqueDynamic(this, &ULoadoutComponent::HandleGearSocketChanged);
    }
    EnsureDefaultLoadout();
}

void ULoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UInventoryComponent *Inv = GetInventoryComponent())
    {
        Inv->OnInventoryChanged.RemoveDynamic(this, &ULoadoutComponent::HandleInventoryChanged);
        Inv->OnGearSocketChanged.RemoveDynamic(this, &ULoadoutComponent::HandleGearSocketChanged);
    }
    Super::EndPlay(EndPlayReason);
}

void ULoadoutComponent::HandleInventoryChanged(EInventoryChangeType ChangeType)
{
    NotifyActiveWeaponChanged();
}

void ULoadoutComponent::HandleGearSocketChanged(FGuid GearID)
{
    // What a socket change can break; other problems a loadout has wait for the pre-fight clean.
    static const TArray<ELoadoutSlotType> SocketBreakableSlots = {
        ELoadoutSlotType::WeaponSpell, ELoadoutSlotType::WeaponAbility, ELoadoutSlotType::RingSpell,
        ELoadoutSlotType::EvolutionOverflow, ELoadoutSlotType::RingOverflow, ELoadoutSlotType::AbilityOverflow};

    const UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        return;
    }
    // Mid-fight the loadout is in use; PrepareForBattle's ClearInvalidSlots cleans it before the next fight.
    const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UTurnManager *TurnManager = GI ? GI->GetSubsystem<UTurnManager>() : nullptr;
    if (TurnManager && TurnManager->IsCombatActive())
    {
        return;
    }
    for (const int32 Index : Inv->GetLoadoutsUsing(GearID))
    {
        AnnounceDroppedParts(Index, DropInvalidParts(Index, SocketBreakableSlots));
    }
}

void ULoadoutComponent::EnsureDefaultLoadout()
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::EnsureDefaultLoadout] No InventoryComponent found on owner"));
        return;
    }
    if (Inv->SavedLoadouts.Num() == 0)
    {
        FCombatLoadout DefaultLoadout;
        DefaultLoadout.LoadoutName = TEXT("Default");
        DefaultLoadout.InitializeForClass(GetCharacterClass());
        Inv->SavedLoadouts.Add(DefaultLoadout);
        Inv->ActiveLoadoutIndex = 0;
    }
}

// ==================== ACTIVE LOADOUT ====================

const FCombatLoadout &ULoadoutComponent::GetActiveLoadout() const
{
    static const FCombatLoadout EmptyLoadout;
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveLoadout] No InventoryComponent found on owner"));
        return EmptyLoadout;
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    }
    return EmptyLoadout;
}

ECharacterClass ULoadoutComponent::GetCharacterClass() const
{
    const UCharacterData *CharData = GetOwnerCharacterData();
    return CharData ? CharData->CharacterClass : CharacterClass;
}

bool ULoadoutComponent::GetActiveLoadoutRef(FCombatLoadout &OutLoadout) const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveLoadoutRef] No InventoryComponent found on owner"));
        return false;
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        OutLoadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        return true;
    }
    return false;
}

bool ULoadoutComponent::SetActiveLoadoutIndex(int32 Index)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::SetActiveLoadoutIndex] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    // B-085: the outgoing loadout's wear reaches the owned gear (and the incoming one) first.
    if (Index != Inv->ActiveLoadoutIndex)
    {
        Inv->WriteLoadoutToOwned(Inv->ActiveLoadoutIndex);
    }
    Inv->ActiveLoadoutIndex = Index;
    ClaimSharedGear();
    bIsReadyForBattle = false; // local field, stays
    OnLoadoutChanged.Broadcast(Index); // local delegate, broadcast preserved

    // Notify TurnManager so cached BonusTurnSpeed re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (UWorld *World = GetWorld())
    {
        if (UGameInstance *GI = World->GetGameInstance())
        {
            if (UTurnManager *TurnManager = GI->GetSubsystem<UTurnManager>())
            {
                TurnManager->OnActorSpeedChanged(GetOwner());
            }
        }
    }

    // Active equipment changed → recompute the stat-derived pools (equipment
    // BonusMaxHP/BonusMaxEnergy; the weapon's and ring's pool stones, B-082). Does NOT
    // touch CurrentHP/EP (overcap-not-clamp) — see RecomputeMaxPools doc.
    if (UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr)
    {
        CharComp->RecomputeMaxPools();
    }

    return true;
}

int32 ULoadoutComponent::GetActiveLoadoutIndex() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    return Inv ? Inv->ActiveLoadoutIndex : 0;
}

FString ULoadoutComponent::GetActiveLoadoutName() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveLoadoutName] No InventoryComponent found on owner"));
        return TEXT("Invalid");
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].LoadoutName;
    }
    return TEXT("Invalid");
}

// ==================== LOADOUT MANAGEMENT ====================

int32 ULoadoutComponent::CreateNewLoadout(const FString &LoadoutName)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::CreateNewLoadout] No InventoryComponent found on owner"));
        return -1;
    }
    if (Inv->SavedLoadouts.Num() >= Inv->MaxSavedLoadouts)
    {
        return -1;
    }

    FCombatLoadout NewLoadout;
    NewLoadout.LoadoutName = LoadoutName;
    NewLoadout.InitializeForClass(GetCharacterClass());

    int32 NewIndex = Inv->SavedLoadouts.Add(NewLoadout);
    return NewIndex;
}

int32 ULoadoutComponent::CreateAndConfigureLoadout(
    const FString &LoadoutName,
    UInventoryComponent *Inventory,
    const TArray<USpellData *> &SpellsToAdd,
    const TArray<UAbilityData *> &AbilitiesToAdd,
    const TArray<FEquippedItemSlot> &ItemsToAdd)
{
    // Validate inventory
    if (!Inventory)
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadoutComponent] CreateAndConfigureLoadout: Null Inventory"));
        return -1;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::CreateAndConfigureLoadout] No InventoryComponent found on owner"));
        return -1;
    }

    // Create new empty loadout
    int32 NewIndex = CreateNewLoadout(LoadoutName);
    if (NewIndex < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[LoadoutComponent] Failed to create loadout - max loadouts reached (%d/%d)"),
               Inv->SavedLoadouts.Num(), Inv->MaxSavedLoadouts);
        return -1;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[NewIndex];

    UE_LOG(LogTemp, Display, TEXT("[LoadoutComponent] Created loadout '%s' at index %d"), *LoadoutName, NewIndex);

    // Add spells (validate against inventory)
    int32 SpellsAdded = 0;
    for (USpellData *Spell : SpellsToAdd)
    {
        if (!Spell)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Null spell in SpellsToAdd array"));
            continue;
        }

        if (!Inventory->HasSpell(Spell))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Spell '%s' not in inventory - skipping"),
                   *Spell->Name);
            continue;
        }

        // Add to primary weapon or ring depending on class
        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
        {
            Loadout.PrimaryWeapon.AssignedSpells.Add(Spell);
            SpellsAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added spell '%s' to weapon"), *Spell->Name);
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
        {
            Loadout.PrimaryRing.RingEntry.AssignedSpells.Add(Spell);
            SpellsAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added spell '%s' to ring"), *Spell->Name);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] No valid weapon/ring to add spell to"));
        }
    }

    // Add abilities (validate against inventory)
    int32 AbilitiesAdded = 0;
    for (UAbilityData *Ability : AbilitiesToAdd)
    {
        if (!Ability)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Null ability in AbilitiesToAdd array"));
            continue;
        }

        if (!Inventory->HasAbility(Ability))
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Ability '%s' not in inventory - skipping"),
                   *Ability->Name);
            continue;
        }

        // Add to primary weapon
        if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
        {
            Loadout.PrimaryWeapon.AssignedAbilities.Add(Ability);
            AbilitiesAdded++;
            UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Added ability '%s'"), *Ability->Name);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] No valid weapon to add ability to"));
        }
    }

    // Add items — runtime equip path. Each entry pairs a CrystalId with a
    // requested Quantity; we debit what the crystal inventory actually holds
    // and store the transferred amount on the slot (transfer model: you can't
    // equip crystals you don't own). None/empty entries are skipped.
    UCrystalInventoryComponent *CrystalInv =
        Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
    if (!CrystalInv)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[LoadoutComponent] CreateAndConfigureLoadout: no UCrystalInventoryComponent on inventory owner — items not debited/equipped"));
    }

    int32 ItemsAdded = 0;
    int32 SlotIndex = 0;
    for (const FEquippedItemSlot &Entry : ItemsToAdd)
    {
        if (Entry.CrystalId.Type == ECrystalType::None || Entry.Quantity <= 0)
        {
            continue;
        }

        if (SlotIndex >= Loadout.ItemSlots.Num())
        {
            UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] Item slots full (%d) - skipping '%s'"),
                   Loadout.ItemSlots.Num(), *ItemIdentity::GetDisplayName(Entry.CrystalId));
            break;
        }

        // Route through the inventory facade so the equip-debit emits OnInventoryChanged.
        // Behaviour-identical to the old CrystalInv-guarded debit: the facade returns 0
        // when the owner has no crystal pool (same as the prior `CrystalInv ? ... : 0`).
        const int32 Debited = Inventory->RemoveCrystal(Entry.CrystalId, Entry.Quantity);
        Loadout.ItemSlots[SlotIndex].CrystalId = Entry.CrystalId;
        Loadout.ItemSlots[SlotIndex].Quantity = Debited;
        if (Debited > 0)
        {
            ItemsAdded++;
        }
        UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Equipped '%s' x%d (requested %d) to slot %d"),
               *ItemIdentity::GetDisplayName(Entry.CrystalId), Debited, Entry.Quantity, SlotIndex);
        SlotIndex++;
    }

    UE_LOG(LogTemp, Display, TEXT("[LoadoutComponent] Loadout '%s' configured: %d spells, %d abilities, %d items"),
           *LoadoutName, SpellsAdded, AbilitiesAdded, ItemsAdded);

    return NewIndex;
}

bool ULoadoutComponent::DeleteLoadout(int32 Index)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::DeleteLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    // Don't delete last loadout
    if (Inv->SavedLoadouts.Num() <= 1)
    {
        return false;
    }

    Inv->SavedLoadouts.RemoveAt(Index);

    // Adjust active index if needed
    if (Inv->ActiveLoadoutIndex >= Inv->SavedLoadouts.Num())
    {
        Inv->ActiveLoadoutIndex = Inv->SavedLoadouts.Num() - 1;
    }
    else if (Inv->ActiveLoadoutIndex > Index)
    {
        Inv->ActiveLoadoutIndex--;
    }

    return true;
}

int32 ULoadoutComponent::DuplicateLoadout(int32 SourceIndex, const FString &NewName)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::DuplicateLoadout] No InventoryComponent found on owner"));
        return -1;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(SourceIndex))
    {
        return -1;
    }

    if (Inv->SavedLoadouts.Num() >= Inv->MaxSavedLoadouts)
    {
        return -1;
    }

    FCombatLoadout NewLoadout = Inv->SavedLoadouts[SourceIndex];
    NewLoadout.LoadoutName = NewName;

    int32 NewIndex = Inv->SavedLoadouts.Add(NewLoadout);
    return NewIndex;
}

bool ULoadoutComponent::RenameLoadout(int32 Index, const FString &NewName)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::RenameLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    Inv->SavedLoadouts[Index].LoadoutName = NewName;
    return true;
}

int32 ULoadoutComponent::GetLoadoutCount() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    return Inv ? Inv->SavedLoadouts.Num() : 0;
}

TArray<FString> ULoadoutComponent::GetLoadoutNames() const
{
    TArray<FString> Names;
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetLoadoutNames] No InventoryComponent found on owner"));
        return Names;
    }
    for (const FCombatLoadout &Loadout : Inv->SavedLoadouts)
    {
        Names.Add(Loadout.LoadoutName);
    }
    return Names;
}

// ==================== VALIDATION ====================

bool ULoadoutComponent::ValidateActiveLoadout(UInventoryComponent *Inventory)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ValidateActiveLoadout] No InventoryComponent found on owner"));
        return false;
    }
    return ValidateLoadout(Inv->ActiveLoadoutIndex, Inventory);
}

bool ULoadoutComponent::ValidateLoadout(int32 Index, UInventoryComponent *Inventory)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ValidateLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return false;
    }

    if (!Inventory)
    {
        return false;
    }

    TArray<FString> Errors = GetValidationErrors(Index, Inventory);

    if (Errors.Num() > 0)
    {
        for (const FString &Error : Errors)
        {
            OnValidationFailed.Broadcast(Error);
        }
        return false;
    }

    return true;
}

TArray<FString> ULoadoutComponent::GetValidationErrors(int32 Index, UInventoryComponent *Inventory) const
{
    // B-028: one rule set (FLoadoutRules); the reasons are the slot cleaner's findings.
    if (!Inventory)
    {
        return {TEXT("No inventory component")};
    }
    return FLoadoutRules::ToReasons(CollectInvalidSlotFindings(Index, Inventory));
}

// OBSOLETE (B-028): remove after Crown's PIE pass.
TArray<FString> ULoadoutComponent::GetValidationErrorsLegacy(int32 Index, UInventoryComponent *Inventory) const
{
    TArray<FString> Errors;

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetValidationErrors] No InventoryComponent found on owner"));
        Errors.Add(TEXT("No inventory component on owner"));
        return Errors;
    }

    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        Errors.Add(TEXT("Invalid loadout index"));
        return Errors;
    }

    if (!Inventory)
    {
        Errors.Add(TEXT("No inventory component"));
        return Errors;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Index];

    // Validate primary weapon
    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
    {
        // Check weapon is owned
        bool bFound = false;
        for (const FWeaponInventoryEntry &Entry : Inventory->Weapons)
        {
            if (Entry.Weapon == Loadout.PrimaryWeapon.WeaponEntry.Weapon)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            Errors.Add(TEXT("Primary weapon not in inventory"));
        }

        // Validate abilities
        for (UAbilityData *Ability : Loadout.PrimaryWeapon.AssignedAbilities)
        {
            if (Ability && !Inventory->HasAbility(Ability))
            {
                Errors.Add(FString::Printf(TEXT("Ability '%s' not learned"), *Ability->Name));
            }
        }

        // Validate spells
        for (USpellData *Spell : Loadout.PrimaryWeapon.AssignedSpells)
        {
            if (Spell && !Inventory->HasSpell(Spell))
            {
                Errors.Add(FString::Printf(TEXT("Spell '%s' not learned"), *Spell->Name));
            }
        }
    }

    // Validate primary ring (Generic/Caster)
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
    {
        bool bFound = false;
        for (const FRingInventoryEntry &Entry : Inventory->Rings)
        {
            if (Entry.Ring == Loadout.PrimaryRing.RingEntry.Ring)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            Errors.Add(TEXT("Primary ring not in inventory"));
        }
    }

    // Validate primary evolution
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
    {
        // Evolution ownership lives in UEvolutionInventoryComponent post-refactor;
        // the legacy Items pool is empty under the new-fields path, so the old
        // HasItem read produced a false "not owned" for migrated characters.
        // Resolve the sibling on the inventory's owner; wiring is guaranteed via
        // the character base, so a missing component is a misconfiguration — log
        // and treat as not-owned to preserve the original fail-safe.
        const UEvolutionInventoryComponent *EvolutionInv =
            Inventory->GetOwner()
                ? Inventory->GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>()
                : nullptr;
        if (!EvolutionInv)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("[ULoadoutComponent::GetValidationErrors] No UEvolutionInventoryComponent on inventory owner — cannot verify Evolution ownership; treating as not owned"));
        }
        if (!EvolutionInv || !EvolutionInv->HasInstance(Loadout.PrimaryEvolution.Item))
        {
            Errors.Add(TEXT("Primary evolution crystal not in inventory"));
        }

        // Validate evolution spells count — cap keys on the evolution item's own tier.
        const int32 EvoCap = Loadout.PrimaryEvolution.Item
            ? CrystalEffectTable::SlotsForContainerTier(Loadout.PrimaryEvolution.Tier)
            : LoadoutConstants::MAX_EVOLUTION_SPELLS;
        if (Loadout.EvolutionSpells.Num() > EvoCap)
        {
            Errors.Add(FString::Printf(TEXT("Too many evolution spells (%d/%d)"),
                                       Loadout.EvolutionSpells.Num(), EvoCap));
        }
    }

    // Validate secondary (Generic)
    if (GetCharacterClass() == ECharacterClass::Generic)
    {
        if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
        {
            bool bFound = false;
            for (const FWeaponInventoryEntry &Entry : Inventory->Weapons)
            {
                if (Entry.Weapon == Loadout.SecondaryWeapon.WeaponEntry.Weapon)
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                Errors.Add(TEXT("Secondary weapon not in inventory"));
            }
        }
    }

    // Validate ring loadout (Resonator)
    if (GetCharacterClass() == ECharacterClass::Resonator)
    {
        int32 TotalSlotCost = 0;
        for (const FRingLoadoutEntry &RingEntry : Loadout.RingLoadout)
        {
            if (RingEntry.IsValid())
            {
                bool bFound = false;
                for (const FRingInventoryEntry &Entry : Inventory->Rings)
                {
                    if (Entry.Ring == RingEntry.RingEntry.Ring)
                    {
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                {
                    Errors.Add(TEXT("Ring in loadout not in inventory"));
                }

                // Add slot cost (1 for normal, 2 for evolved)
                TotalSlotCost += InventoryConstants::GetRingSlotCost(RingEntry.IsEvolved());
            }
        }

        // Unified budget: primary slot cost + ring slot costs must fit one budget.
        int32 PrimaryCost = 0;
        switch (Loadout.PrimarySlotType)
        {
        case EPrimarySlotType::Weapon:
            // Runtime path: cost reflects the actual attached crystal (bare 1 / crystal 2 / evolution 3).
            PrimaryCost = Loadout.PrimaryWeapon.WeaponEntry.GetSlotCost();
            break;
        case EPrimarySlotType::Evolution:
            PrimaryCost = LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST;
            break;
        case EPrimarySlotType::None:
        case EPrimarySlotType::Ring:
            PrimaryCost = 0;
            break;
        }
        const int32 TotalCost = PrimaryCost + TotalSlotCost;
        if (TotalCost > LoadoutConstants::LOADOUT_TOTAL_BUDGET)
        {
            Errors.Add(FString::Printf(
                TEXT("Loadout exceeds budget: primary (cost %d) + rings (cost %d) = %d, max is %d"),
                PrimaryCost, TotalSlotCost, TotalCost, LoadoutConstants::LOADOUT_TOTAL_BUDGET));
        }
    }

    // Validate innate spells (Caster)
    if (GetCharacterClass() == ECharacterClass::Caster)
    {
        // Resolve the owning actor's CharacterDataComponent + BrokenDarknessManager.
        AActor *OwnerActor = GetOwner();
        UCharacterDataComponent *CharComp = nullptr;
        UBrokenDarknessManager *BDManager = nullptr;
        if (OwnerActor)
        {
            CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
            BDManager = OwnerActor->FindComponentByClass<UBrokenDarknessManager>();
        }

        if (CharComp && CharComp->IsBrokenDarkness())
        {
            // Broken Darkness: InnateSpells is the Darkness pool, BDSpellPools the
            // per-element pools. Count + element-match + shared weight budget. The
            // discount needs the character; null CharData -> no discount, still checked.
            const UCharacterData *CharData = CharComp->CharacterData;
            const int32 Discount = CharData
                ? SpellPoolConstants::SpellSlotDiscount(
                      CharComp->GetWorldMind(), CharComp->GetWorldBody(), CharComp->GetWorldSpirit())
                : 0;
            Errors.Append(FCombatLoadout::ValidateBDSpellLoadout(
                Loadout.InnateSpells, Loadout.BDSpellPools, Discount, /*bCheckWeight=*/true));
        }
        else
        {
            // Normal Caster: ownership + element-capability per innate spell.
            for (USpellData *Spell : Loadout.InnateSpells)
            {
                if (!Spell)
                    continue;

                if (!Inventory->HasSpell(Spell))
                {
                    Errors.Add(FString::Printf(TEXT("Innate spell '%s' not learned"), *Spell->Name));
                    continue;
                }

                // Element-capability check — shared with ActionExecutor::ValidateAction.
                if (!UBrokenDarknessManager::IsElementCastable(OwnerActor, CharComp, BDManager, Spell->Element))
                {
                    Errors.Add(FString::Printf(
                        TEXT("Innate spell '%s' element not castable by this character"),
                        *Spell->Name));
                }
            }

            // Per-school count cap (tier-blind, character-independent — runs regardless of
            // CharData). Mirrors FSavedLoadout's authoring guard so both surfaces report
            // identically. Each school's spell count must fit one pool.
            TMap<ESpellSchool, int32> PerSchoolCount;
            for (const USpellData *Spell : Loadout.InnateSpells)
            {
                if (Spell)
                {
                    PerSchoolCount.FindOrAdd(Spell->School)++;
                }
            }
            for (const TPair<ESpellSchool, int32> &Pair : PerSchoolCount)
            {
                if (Pair.Value > SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL)
                {
                    Errors.Add(FString::Printf(TEXT("Too many %s innate spells (%d/%d)"),
                                               *UEnum::GetValueAsString(Pair.Key), Pair.Value,
                                               SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL));
                }
            }

            // Weight budget: sum of effective per-spell costs (tier - mastery discount)
            // must fit INNATE_SPELL_BUDGET. Discount is from raw world-pillar levels;
            // null CharData -> no discount (base costs), budget still enforced.
            const UCharacterData *CharData = CharComp ? CharComp->CharacterData : nullptr;
            const int32 Discount = CharData
                ? SpellPoolConstants::SpellSlotDiscount(
                      CharComp->GetWorldMind(), CharComp->GetWorldBody(), CharComp->GetWorldSpirit())
                : 0;
            int32 Used = 0;
            for (const USpellData *Spell : Loadout.InnateSpells)
            {
                if (Spell)
                {
                    Used += SpellPoolConstants::SpellSlotEffectiveCost(Spell->Tier, Discount);
                }
            }
            if (Used > SpellPoolConstants::INNATE_SPELL_BUDGET)
            {
                Errors.Add(FString::Printf(
                    TEXT("Innate spell budget exceeded: %d/%d points used (discount -%d)"),
                    Used, SpellPoolConstants::INNATE_SPELL_BUDGET, Discount));
            }
        }
    }

    // Item slots are self-owning under the transfer model — their Quantity is
    // debited from the crystal inventory at equip time, so there's no ongoing
    // inventory-ownership relationship left to validate. Only the duplicate-type
    // rule still applies.
    if (Loadout.HasDuplicateItemTypes())
    {
        Errors.Add(TEXT("Duplicate item types in loadout"));
    }

    return Errors;
}

TArray<FInvalidSlotFinding> ULoadoutComponent::CollectInvalidSlotFindings() const
{
    const UInventoryComponent *Inv = GetInventoryComponent();
    return CollectInvalidSlotFindings(Inv ? Inv->ActiveLoadoutIndex : INDEX_NONE);
}

TArray<FInvalidSlotFinding> ULoadoutComponent::CollectInvalidSlotFindings(int32 Index, const UInventoryComponent *OwnershipSource) const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        FInvalidSlotFinding Guard;
        Guard.SlotType = ELoadoutSlotType::None;
        Guard.SlotIndex = -1;
        Guard.bClearable = false;
        Guard.Reason = TEXT("No inventory component on owner");
        return {Guard};
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        FInvalidSlotFinding Guard;
        Guard.SlotType = ELoadoutSlotType::None;
        Guard.SlotIndex = -1;
        Guard.bClearable = false;
        Guard.Reason = TEXT("Invalid loadout index");
        return {Guard};
    }
    return FLoadoutRules::CollectFindings(Inv->SavedLoadouts[Index], BuildRuleContext(OwnershipSource ? OwnershipSource : Inv));
}

FLoadoutRuleContext ULoadoutComponent::BuildRuleContext(const UInventoryComponent *Inventory) const
{
    FLoadoutRuleContext Context;
    Context.CharacterClass = GetCharacterClass();

    AActor *OwnerActor = GetOwner();
    UCharacterDataComponent *CharComp = OwnerActor ? OwnerActor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    UBrokenDarknessManager *BDManager = OwnerActor ? OwnerActor->FindComponentByClass<UBrokenDarknessManager>() : nullptr;
    Context.bBrokenDarkness = CharComp && CharComp->IsBrokenDarkness();
    // Discount from raw world-pillar levels; no CharacterData -> no discount (base costs).
    Context.SpellDiscount = (CharComp && CharComp->CharacterData)
        ? SpellPoolConstants::SpellSlotDiscount(CharComp->GetWorldMind(), CharComp->GetWorldBody(), CharComp->GetWorldSpirit())
        : 0;

    if (Inventory)
    {
        Context.OwnsWeapon = [Inventory](const UWeaponData *Weapon)
        {
            return Inventory->Weapons.ContainsByPredicate([Weapon](const FWeaponInventoryEntry &Entry) { return Entry.Weapon == Weapon; });
        };
        Context.OwnsRing = [Inventory](const URingData *Ring)
        {
            return Inventory->Rings.ContainsByPredicate([Ring](const FRingInventoryEntry &Entry) { return Entry.Ring == Ring; });
        };
        Context.KnowsAbility = [Inventory](const UAbilityData *Ability)
        {
            return Inventory->HasAbility(const_cast<UAbilityData *>(Ability));
        };
        Context.KnowsSpell = [Inventory](const USpellData *Spell)
        {
            return Inventory->HasSpell(const_cast<USpellData *>(Spell));
        };
        // Evolution ownership lives in UEvolutionInventoryComponent; a missing one is a
        // misconfiguration, treated as not owned (fail-safe).
        const UEvolutionInventoryComponent *EvolutionInv =
            Inventory->GetOwner() ? Inventory->GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>() : nullptr;
        Context.OwnsEvolution = [EvolutionInv](const UEvolutionItemData *Item)
        {
            if (!EvolutionInv)
            {
                UE_LOG(LogWoRLoadout, Warning, TEXT("[ULoadoutComponent] No UEvolutionInventoryComponent on inventory owner; treating the evolution as not owned"));
                return false;
            }
            return EvolutionInv->HasInstance(const_cast<UEvolutionItemData *>(Item));
        };
    }

    // Element-capability check, shared with ActionExecutor::ValidateAction.
    Context.CanCastElement = [OwnerActor, CharComp, BDManager](ESpellElement Element)
    {
        return UBrokenDarknessManager::IsElementCastable(OwnerActor, CharComp, BDManager, Element);
    };
    return Context;
}

// OBSOLETE (B-028): remove after Crown's PIE pass.
TArray<FInvalidSlotFinding> ULoadoutComponent::CollectInvalidSlotFindingsLegacy() const
{
    TArray<FInvalidSlotFinding> Findings;

    auto AddFinding = [&Findings](ELoadoutSlotType Type, int32 SlotIndex, bool bClearable, const FString &Reason)
    {
        FInvalidSlotFinding Finding;
        Finding.SlotType = Type;
        Finding.SlotIndex = SlotIndex;
        Finding.bClearable = bClearable;
        Finding.Reason = Reason;
        Findings.Add(MoveTemp(Finding));
    };

    auto AddRingSpellFinding = [&Findings](int32 RingLoadoutIndex, int32 SpellIndex, const FString &Reason)
    {
        FInvalidSlotFinding Finding;
        Finding.SlotType = ELoadoutSlotType::RingSpell;
        Finding.SlotIndex = RingLoadoutIndex;
        Finding.SubIndex = SpellIndex;
        Finding.bClearable = true;
        Finding.Reason = Reason;
        Findings.Add(MoveTemp(Finding));
    };

    // Element coherence for a ring's customisable spells against its crystal.
    // Shared by the Resonator ring loadout and the Generic/Caster primary ring.
    auto CheckRingSpellCoherence = [&AddRingSpellFinding](const FRingInventoryEntry &Ring, int32 RingLoadoutIndex)
    {
        if (!Ring.Ring)
        {
            return;
        }
        for (int32 j = 0; j < Ring.AssignedSpells.Num(); ++j)
        {
            USpellData *Spell = Ring.AssignedSpells[j];
            if (!Spell)
            {
                continue;
            }
            if (!Ring.HasCrystal())
            {
                AddRingSpellFinding(RingLoadoutIndex, j,
                                    FString::Printf(TEXT("Ring spell '%s' requires a crystal but the ring has none"),
                                                    *Spell->Name));
                continue;
            }
            const ESpellElement RingElement = Ring.GetElement();
            if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, RingElement))
            {
                AddRingSpellFinding(RingLoadoutIndex, j,
                                    FString::Printf(TEXT("Ring spell '%s' element %s does not match ring crystal element %s"),
                                                    *Spell->Name,
                                                    *UEnum::GetValueAsString(Spell->Element),
                                                    *UEnum::GetValueAsString(RingElement)));
            }
        }
    };

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        AddFinding(ELoadoutSlotType::None, -1, false, TEXT("No inventory component on owner"));
        return Findings;
    }

    const int32 Index = Inv->ActiveLoadoutIndex;
    if (!Inv->SavedLoadouts.IsValidIndex(Index))
    {
        AddFinding(ELoadoutSlotType::None, -1, false, TEXT("Invalid loadout index"));
        return Findings;
    }

    // Ownership checks query the owner's inventory component. GetValidationErrors
    // takes Inventory as a separate param, but is always passed the owner's
    // component in practice — so the single resolved Inv is used for both.
    UInventoryComponent *Inventory = Inv;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Index];

    // Primary weapon
    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid())
    {
        bool bFound = false;
        for (const FWeaponInventoryEntry &Entry : Inventory->Weapons)
        {
            if (Entry.Weapon == Loadout.PrimaryWeapon.WeaponEntry.Weapon)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            AddFinding(ELoadoutSlotType::PrimaryWeapon, -1, true, TEXT("Primary weapon not in inventory"));
        }

        const UWeaponData *EquippedWeapon = Loadout.PrimaryWeapon.WeaponEntry.Weapon;
        for (int32 i = 0; i < Loadout.PrimaryWeapon.AssignedAbilities.Num(); ++i)
        {
            UAbilityData *Ability = Loadout.PrimaryWeapon.AssignedAbilities[i];
            if (!Ability)
            {
                continue;
            }
            if (!Inventory->HasAbility(Ability))
            {
                AddFinding(ELoadoutSlotType::WeaponAbility, i, true,
                           FString::Printf(TEXT("Ability '%s' not learned"), *Ability->Name));
                continue;
            }

            // Weapon-type coherence: ability must match the equipped weapon type.
            if (Ability->RequiredWeaponType != EquippedWeapon->WeaponType)
            {
                AddFinding(ELoadoutSlotType::WeaponAbility, i, true,
                           FString::Printf(TEXT("Ability '%s' requires weapon type %s, equipped weapon is %s"),
                                           *Ability->Name,
                                           *UEnum::GetValueAsString(Ability->RequiredWeaponType),
                                           *UEnum::GetValueAsString(EquippedWeapon->WeaponType)));
                continue;
            }
            if (!Ability->AllowsWieldMode(EquippedWeapon->WieldMode))
            {
                AddFinding(ELoadoutSlotType::WeaponAbility, i, true,
                           FString::Printf(TEXT("Ability '%s' requires wield mode %s, equipped weapon is %s"),
                                           *Ability->Name,
                                           *UEnum::GetValueAsString(Ability->RequiredWieldMode),
                                           *UEnum::GetValueAsString(EquippedWeapon->WieldMode)));
            }
        }

        for (int32 i = 0; i < Loadout.PrimaryWeapon.AssignedSpells.Num(); ++i)
        {
            USpellData *Spell = Loadout.PrimaryWeapon.AssignedSpells[i];
            if (!Spell)
            {
                continue;
            }
            if (!Inventory->HasSpell(Spell))
            {
                AddFinding(ELoadoutSlotType::WeaponSpell, i, true,
                           FString::Printf(TEXT("Spell '%s' not learned"), *Spell->Name));
                continue;
            }

            // Element coherence: weapon-sourced spell must match the weapon crystal.
            if (!Loadout.PrimaryWeapon.WeaponEntry.HasCrystal())
            {
                AddFinding(ELoadoutSlotType::WeaponSpell, i, true,
                           FString::Printf(TEXT("Weapon spell '%s' requires a crystal but equipped weapon has none"),
                                           *Spell->Name));
                continue;
            }
            const ESpellElement CrystalElement = Loadout.PrimaryWeapon.WeaponEntry.GetElement();
            if (!ElementHelpers::SpellElementMatchesHost(Spell->Element, CrystalElement))
            {
                AddFinding(ELoadoutSlotType::WeaponSpell, i, true,
                           FString::Printf(TEXT("Weapon spell '%s' element %s does not match weapon crystal element %s"),
                                           *Spell->Name,
                                           *UEnum::GetValueAsString(Spell->Element),
                                           *UEnum::GetValueAsString(CrystalElement)));
            }
        }
    }

    // Primary ring (Generic/Caster)
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
    {
        bool bFound = false;
        for (const FRingInventoryEntry &Entry : Inventory->Rings)
        {
            if (Entry.Ring == Loadout.PrimaryRing.RingEntry.Ring)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            AddFinding(ELoadoutSlotType::PrimaryRing, -1, true, TEXT("Primary ring not in inventory"));
        }

        // Element coherence for the primary ring's customisable spells (SlotIndex -1 = primary ring).
        CheckRingSpellCoherence(Loadout.PrimaryRing.RingEntry, -1);
    }

    // Primary evolution
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
    {
        const UEvolutionInventoryComponent *EvolutionInv =
            Inventory->GetOwner()
                ? Inventory->GetOwner()->FindComponentByClass<UEvolutionInventoryComponent>()
                : nullptr;
        if (!EvolutionInv || !EvolutionInv->HasInstance(Loadout.PrimaryEvolution.Item))
        {
            AddFinding(ELoadoutSlotType::PrimaryEvolution, -1, true, TEXT("Primary evolution crystal not in inventory"));
        }

        const int32 EvoCap = Loadout.PrimaryEvolution.Item
            ? CrystalEffectTable::SlotsForContainerTier(Loadout.PrimaryEvolution.Tier)
            : LoadoutConstants::MAX_EVOLUTION_SPELLS;
        if (Loadout.EvolutionSpells.Num() > EvoCap)
        {
            AddFinding(ELoadoutSlotType::EvolutionOverflow, -1, true,
                       FString::Printf(TEXT("Too many evolution spells (%d/%d)"),
                                       Loadout.EvolutionSpells.Num(), EvoCap));
        }
    }

    // Secondary weapon (Generic)
    if (GetCharacterClass() == ECharacterClass::Generic)
    {
        if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
        {
            bool bFound = false;
            for (const FWeaponInventoryEntry &Entry : Inventory->Weapons)
            {
                if (Entry.Weapon == Loadout.SecondaryWeapon.WeaponEntry.Weapon)
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                AddFinding(ELoadoutSlotType::SecondaryWeapon, -1, true, TEXT("Secondary weapon not in inventory"));
            }
        }
    }

    // Resonator ring loadout
    if (GetCharacterClass() == ECharacterClass::Resonator)
    {
        int32 TotalSlotCost = 0;
        for (int32 i = 0; i < Loadout.RingLoadout.Num(); ++i)
        {
            const FRingLoadoutEntry &RingEntry = Loadout.RingLoadout[i];
            if (RingEntry.IsValid())
            {
                bool bFound = false;
                for (const FRingInventoryEntry &Entry : Inventory->Rings)
                {
                    if (Entry.Ring == RingEntry.RingEntry.Ring)
                    {
                        bFound = true;
                        break;
                    }
                }
                if (!bFound)
                {
                    AddFinding(ELoadoutSlotType::ResonatorRing, i, true, TEXT("Ring in loadout not in inventory"));
                }

                // Element coherence for this ring's customisable spells (SlotIndex = ring-loadout index).
                CheckRingSpellCoherence(RingEntry.RingEntry, i);

                TotalSlotCost += InventoryConstants::GetRingSlotCost(RingEntry.IsEvolved());
            }
        }

        // Unified budget: primary slot cost + ring slot costs must fit one budget.
        int32 PrimaryCost = 0;
        switch (Loadout.PrimarySlotType)
        {
        case EPrimarySlotType::Weapon:
            // Runtime path: cost reflects the actual attached crystal (bare 1 / crystal 2 / evolution 3).
            PrimaryCost = Loadout.PrimaryWeapon.WeaponEntry.GetSlotCost();
            break;
        case EPrimarySlotType::Evolution:
            PrimaryCost = LoadoutConstants::EVOLUTION_PRIMARY_SLOT_COST;
            break;
        case EPrimarySlotType::None:
        case EPrimarySlotType::Ring:
            PrimaryCost = 0;
            break;
        }
        const int32 TotalCost = PrimaryCost + TotalSlotCost;
        if (TotalCost > LoadoutConstants::LOADOUT_TOTAL_BUDGET)
        {
            AddFinding(ELoadoutSlotType::RingOverflow, -1, true,
                       FString::Printf(
                           TEXT("Loadout exceeds budget: primary (cost %d) + rings (cost %d) = %d, max is %d"),
                           PrimaryCost, TotalSlotCost, TotalCost, LoadoutConstants::LOADOUT_TOTAL_BUDGET));
        }
    }

    // Innate spells (Caster)
    if (GetCharacterClass() == ECharacterClass::Caster)
    {
        AActor *OwnerActor = GetOwner();
        UCharacterDataComponent *CharComp = nullptr;
        UBrokenDarknessManager *BDManager = nullptr;
        if (OwnerActor)
        {
            CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
            BDManager = OwnerActor->FindComponentByClass<UBrokenDarknessManager>();
        }

        if (CharComp && CharComp->IsBrokenDarkness())
        {
            // BD pool count + element-match + shared weight budget are owned by
            // ValidateBDSpellLoadout. Surface its findings for parity, but mark them
            // non-clearable — this build does not auto-clear BD pool entries (locked spec).
            const UCharacterData *CharData = CharComp->CharacterData;
            const int32 Discount = CharData
                ? SpellPoolConstants::SpellSlotDiscount(
                      CharComp->GetWorldMind(), CharComp->GetWorldBody(), CharComp->GetWorldSpirit())
                : 0;
            for (const FString &Err : FCombatLoadout::ValidateBDSpellLoadout(
                     Loadout.InnateSpells, Loadout.BDSpellPools, Discount, /*bCheckWeight=*/true))
            {
                AddFinding(ELoadoutSlotType::BDPoolSpell, -1, false, Err);
            }
        }
        else
        {
            for (int32 i = 0; i < Loadout.InnateSpells.Num(); ++i)
            {
                USpellData *Spell = Loadout.InnateSpells[i];
                if (!Spell)
                {
                    continue;
                }

                if (!Inventory->HasSpell(Spell))
                {
                    AddFinding(ELoadoutSlotType::InnateSpell, i, true,
                               FString::Printf(TEXT("Innate spell '%s' not learned"), *Spell->Name));
                    continue;
                }

                if (!UBrokenDarknessManager::IsElementCastable(OwnerActor, CharComp, BDManager, Spell->Element))
                {
                    AddFinding(ELoadoutSlotType::InnateSpell, i, true,
                               FString::Printf(TEXT("Innate spell '%s' element not castable by this character"), *Spell->Name));
                }
            }

            // Per-school count cap (tier-blind, character-independent — runs regardless of
            // CharData). Mirrors FSavedLoadout's authoring guard. Pool-wide finding,
            // non-clearable (same pattern as the weight finding below).
            TMap<ESpellSchool, int32> PerSchoolCount;
            for (const USpellData *Spell : Loadout.InnateSpells)
            {
                if (Spell)
                {
                    PerSchoolCount.FindOrAdd(Spell->School)++;
                }
            }
            for (const TPair<ESpellSchool, int32> &Pair : PerSchoolCount)
            {
                if (Pair.Value > SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL)
                {
                    AddFinding(ELoadoutSlotType::InnateSpell, -1, false,
                               FString::Printf(TEXT("Too many %s innate spells (%d/%d)"),
                                               *UEnum::GetValueAsString(Pair.Key), Pair.Value,
                                               SpellPoolConstants::MAX_EQUIPPED_SLOT_POOL));
                }
            }

            // Weight budget finding (pool-wide; non-clearable — no single slot to null).
            // Discount from raw world-pillar levels; null CharData -> no discount.
            const UCharacterData *CharData = CharComp ? CharComp->CharacterData : nullptr;
            const int32 Discount = CharData
                ? SpellPoolConstants::SpellSlotDiscount(
                      CharComp->GetWorldMind(), CharComp->GetWorldBody(), CharComp->GetWorldSpirit())
                : 0;
            int32 Used = 0;
            for (const USpellData *Spell : Loadout.InnateSpells)
            {
                if (Spell)
                {
                    Used += SpellPoolConstants::SpellSlotEffectiveCost(Spell->Tier, Discount);
                }
            }
            if (Used > SpellPoolConstants::INNATE_SPELL_BUDGET)
            {
                AddFinding(ELoadoutSlotType::InnateSpell, -1, false,
                           FString::Printf(TEXT("Innate spell budget exceeded: %d/%d points used (discount -%d)"),
                                           Used, SpellPoolConstants::INNATE_SPELL_BUDGET, Discount));
            }
        }
    }

    // Item slots are exempt from ownership checks (self-owning transfer model).
    // Duplicate crystal types are detected and cleared — keep the first of each
    // type, empty the later duplicates (see the DuplicateItems clear case).
    if (Loadout.HasDuplicateItemTypes())
    {
        AddFinding(ELoadoutSlotType::DuplicateItems, -1, true, TEXT("Duplicate item types in loadout"));
    }

    return Findings;
}

void ULoadoutComponent::ClearInvalidSlots()
{
    if (const UInventoryComponent *Inv = GetInventoryComponent())
    {
        AnnounceDroppedParts(Inv->ActiveLoadoutIndex, DropInvalidParts(Inv->ActiveLoadoutIndex));
    }
}

void ULoadoutComponent::DropInvalidPartsFromAllLoadouts()
{
    const UInventoryComponent *Inv = GetInventoryComponent();
    const int32 LoadoutCount = Inv ? Inv->SavedLoadouts.Num() : 0;
    for (int32 Index = 0; Index < LoadoutCount; ++Index)
    {
        AnnounceDroppedParts(Index, DropInvalidParts(Index));
    }
}

void ULoadoutComponent::ClaimSharedGear()
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->DrawsSharedGear())
    {
        return;
    }
    TArray<FText> Messages;
    for (const FText &Taken : Inv->ClaimActiveSharedGear())
    {
        Messages.Add(FText::Format(LOCTEXT("SharedGearTaken", "Removed: {0}"), Taken));
    }
    if (Messages.Num() > 0)
    {
        OnPartsDropped.Broadcast(Inv->ActiveLoadoutIndex, Messages);
    }
}

void ULoadoutComponent::AnnounceDroppedParts(int32 LoadoutIndex, const TArray<FInvalidSlotFinding> &Dropped)
{
    if (Dropped.Num() == 0)
    {
        return;
    }
    TArray<FText> Messages;
    for (const FInvalidSlotFinding &Finding : Dropped)
    {
        // TODO: rule reasons are built FStrings; move them to LOCTEXT tables (D-061).
        Messages.Add(FText::Format(LOCTEXT("LoadoutPartDropped", "Removed: {0}"), FText::FromString(Finding.Reason)));
    }
    OnPartsDropped.Broadcast(LoadoutIndex, Messages);
}

TArray<FInvalidSlotFinding> ULoadoutComponent::DropInvalidParts(int32 Index, const TArray<ELoadoutSlotType> &OnlySlotTypes)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    TArray<FInvalidSlotFinding> Dropped;
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Index))
    {
        return Dropped;
    }

    // Single-pass: validate once, then mutate the live persistent storage in
    // place (not a by-value accessor copy — see LoadoutComponent.h:536).
    const TArray<FInvalidSlotFinding> Findings = CollectInvalidSlotFindings(Index);
    FCombatLoadout &Loadout = Inv->SavedLoadouts[Index];

    for (const FInvalidSlotFinding &Finding : Findings)
    {
        if (!Finding.bClearable || (OnlySlotTypes.Num() > 0 && !OnlySlotTypes.Contains(Finding.SlotType)))
        {
            continue;
        }

        FInvalidSlotFinding DroppedFinding = Finding;
        switch (Finding.SlotType)
        {
        case ELoadoutSlotType::PrimaryWeapon:
            Loadout.PrimaryWeapon.Clear();
            break;
        case ELoadoutSlotType::PrimaryRing:
            Loadout.PrimaryRing.Clear();
            break;
        case ELoadoutSlotType::PrimaryEvolution:
            Loadout.PrimaryEvolution = FEvolutionAttachment();
            break;
        case ELoadoutSlotType::SecondaryWeapon:
            Loadout.SecondaryWeapon.Clear();
            break;
        case ELoadoutSlotType::ResonatorRing:
            if (Loadout.RingLoadout.IsValidIndex(Finding.SlotIndex))
            {
                Loadout.RingLoadout[Finding.SlotIndex].Clear();
            }
            break;
        case ELoadoutSlotType::InnateSpell:
            if (Loadout.InnateSpells.IsValidIndex(Finding.SlotIndex))
            {
                Loadout.InnateSpells[Finding.SlotIndex] = nullptr;
            }
            break;
        case ELoadoutSlotType::WeaponAbility:
            if (Loadout.PrimaryWeapon.AssignedAbilities.IsValidIndex(Finding.SlotIndex))
            {
                Loadout.PrimaryWeapon.AssignedAbilities[Finding.SlotIndex] = nullptr;
            }
            break;
        case ELoadoutSlotType::WeaponSpell:
            if (Loadout.PrimaryWeapon.AssignedSpells.IsValidIndex(Finding.SlotIndex))
            {
                Loadout.PrimaryWeapon.AssignedSpells[Finding.SlotIndex] = nullptr;
            }
            break;
        case ELoadoutSlotType::RingSpell:
        {
            // SlotIndex -1 = Generic/Caster primary ring; otherwise the Resonator
            // ring-loadout index. SubIndex addresses the spell within the ring.
            TArray<USpellData *> *Spells = nullptr;
            if (Finding.SlotIndex == -1)
            {
                Spells = &Loadout.PrimaryRing.RingEntry.AssignedSpells;
            }
            else if (Loadout.RingLoadout.IsValidIndex(Finding.SlotIndex))
            {
                Spells = &Loadout.RingLoadout[Finding.SlotIndex].RingEntry.AssignedSpells;
            }
            if (Spells && Spells->IsValidIndex(Finding.SubIndex))
            {
                (*Spells)[Finding.SubIndex] = nullptr;
            }
            break;
        }
        case ELoadoutSlotType::EvolutionOverflow:
        {
            // Keep the first EvoCap (evolution-tier slot count), drop the rest.
            const int32 EvoCap = Loadout.PrimaryEvolution.Item
                ? CrystalEffectTable::SlotsForContainerTier(Loadout.PrimaryEvolution.Tier)
                : LoadoutConstants::MAX_EVOLUTION_SPELLS;
            if (Loadout.EvolutionSpells.Num() > EvoCap)
            {
                Loadout.EvolutionSpells.SetNum(EvoCap);
            }
            break;
        }
        case ELoadoutSlotType::RingOverflow:
        {
            // Walk rings accumulating cost from the primary slot's cost; truncate
            // from the first index where adding the next ring would exceed budget.
            // B-303: the same cost and budget the finding used (FLoadoutRules over the slots the fight reads).
            const int32 PrimaryCost = FLoadoutRules::NonAdapterCost(Loadout, GetCharacterClass());
            const int32 Budget = LoadoutClassTable::Get(GetCharacterClass()).Budget;

            int32 RunningTotal = PrimaryCost;
            int32 TruncateFromIndex = INDEX_NONE;
            for (int32 i = 0; i < Loadout.RingLoadout.Num(); ++i)
            {
                if (!Loadout.RingLoadout[i].IsValid())
                {
                    continue;
                }
                const int32 NextCost = InventoryConstants::GetRingSlotCost(Loadout.RingLoadout[i].IsEvolved());
                if (RunningTotal + NextCost > Budget)
                {
                    TruncateFromIndex = i;
                    break;
                }
                RunningTotal += NextCost;
            }
            if (TruncateFromIndex != INDEX_NONE)
            {
                Loadout.RingLoadout.SetNum(TruncateFromIndex);
            }
            break;
        }
        case ELoadoutSlotType::DuplicateItems:
        {
            // Keep the first occurrence of each crystal type; empty later ones
            // in place so ItemSlots indices stay stable.
            TSet<ECrystalType> Seen;
            for (FItemLoadoutSlot &Slot : Loadout.ItemSlots)
            {
                if (Slot.IsEmpty())
                {
                    continue;
                }
                if (Seen.Contains(Slot.CrystalId.Type))
                {
                    LoadoutReturnDroppedCrystals(Inv, Slot.CrystalId, Slot.Quantity, BattleItemsTaken);
                    Slot = FItemLoadoutSlot();
                }
                else
                {
                    Seen.Add(Slot.CrystalId.Type);
                }
            }
            break;
        }
        case ELoadoutSlotType::AbilityOverflow:
        {
            // Close the gaps earlier drops left, keep the first abilities up to the cap, drop the
            // rest (which ones go: Q-159b).
            FWeaponLoadoutEntry &Weapon = Finding.SlotIndex == LoadoutAbilityOverflowIndex::SECONDARY_WEAPON
                ? Loadout.SecondaryWeapon
                : Loadout.PrimaryWeapon;
            const bool bAugment = Finding.SubIndex == LoadoutAbilityOverflowIndex::AUGMENT_STONE_ABILITIES;
            TArray<UAbilityData *> &Abilities = bAugment ? Weapon.AssignedAugmentStoneAbilities : Weapon.AssignedAbilities;
            const int32 Cap = bAugment ? LoadoutConstants::MAX_AUGMENTSTONE_ABILITIES : LoadoutConstants::MAX_WEAPON_ABILITIES;
            Abilities.Remove(nullptr);
            TArray<FString> Removed;
            for (int32 i = Cap; i < Abilities.Num(); ++i)
            {
                Removed.Add(Abilities[i]->Name);
            }
            Abilities.SetNum(FMath::Min(Abilities.Num(), Cap));
            if (Removed.IsEmpty())
            {
                continue; // an earlier drop this pass already brought it under the cap
            }
            DroppedFinding.Reason += FString::Printf(TEXT(": %s"), *FString::Join(Removed, TEXT(", ")));
            break;
        }
        case ELoadoutSlotType::ItemSlot:
            if (Loadout.ItemSlots.IsValidIndex(Finding.SlotIndex))
            {
                FItemLoadoutSlot &Slot = Loadout.ItemSlots[Finding.SlotIndex];
                LoadoutReturnDroppedCrystals(Inv, Slot.CrystalId, Slot.Quantity - InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT,
                                             BattleItemsTaken);
                Slot.Quantity = FMath::Clamp(Slot.Quantity, 0, InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT);
            }
            break;
        case ELoadoutSlotType::UtilityCrystalTierCap:
        {
            const TArray<FItemLoadoutSlot> Untrimmed = Loadout.ItemSlots;
            FLoadoutRules::TrimUtilityCrystalsToTierCaps(Loadout);
            TArray<FString> Removed;
            for (int32 i = 0; i < Untrimmed.Num(); ++i)
            {
                const int32 Lost = Untrimmed[i].Quantity - Loadout.ItemSlots[i].Quantity;
                if (Lost > 0)
                {
                    LoadoutReturnDroppedCrystals(Inv, Untrimmed[i].CrystalId, Lost, BattleItemsTaken);
                    Removed.Add(FString::Printf(TEXT("%s %s x%d"), *ItemIdentity::GetTierLetter(Untrimmed[i].CrystalId.Tier),
                                                *ItemIdentity::GetTypeName(Untrimmed[i].CrystalId.Type), Lost));
                }
            }
            if (Removed.IsEmpty())
            {
                continue; // the trim for an earlier band's finding covered every band
            }
            DroppedFinding.Reason += FString::Printf(TEXT(": %s"), *FString::Join(Removed, TEXT(", ")));
            break;
        }
        case ELoadoutSlotType::None:
        case ELoadoutSlotType::BDPoolSpell:
            // Non-clearable categories (guard failures / BD pool) — never
            // reach here because their findings are bClearable == false.
            break;
        }

        UE_LOG(LogWoRLoadout, Warning,
               TEXT("Cleared invalid loadout %d slot: %s[%d] — %s"), Index,
               *UEnum::GetValueAsString(Finding.SlotType),
               Finding.SlotIndex, *DroppedFinding.Reason);
        Dropped.Add(DroppedFinding);
    }
    return Dropped;
}

// ==================== PICKS (D-081) ====================

bool ULoadoutComponent::TryChangeLoadout(int32 LoadoutIndex, bool bHasPick,
                                         TFunctionRef<FText(const FCombatLoadout &, ECharacterClass)> CheckTarget,
                                         TFunctionRef<void(FCombatLoadout &)> Change, FText &OutRefusal)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(LoadoutIndex))
    {
        OutRefusal = LOCTEXT("PickNoLoadout", "There is no such loadout.");
        return false;
    }
    if (!bHasPick)
    {
        OutRefusal = LOCTEXT("PickNothing", "Nothing to assign.");
        return false;
    }

    const FLoadoutRuleContext Context = BuildRuleContext(Inv);
    OutRefusal = CheckTarget(Inv->SavedLoadouts[LoadoutIndex], Context.CharacterClass);
    if (!OutRefusal.IsEmpty())
    {
        UE_LOG(LogWoRLoadout, Log, TEXT("Pick refused on loadout %d: %s"), LoadoutIndex, *OutRefusal.ToString());
        return false;
    }

    TMap<FString, int32> ReasonsBefore;
    for (const FString &Reason : FLoadoutRules::ToReasons(FLoadoutRules::CollectFindings(Inv->SavedLoadouts[LoadoutIndex], Context)))
    {
        ++ReasonsBefore.FindOrAdd(Reason);
    }
    FCombatLoadout Changed = Inv->SavedLoadouts[LoadoutIndex];
    Change(Changed);
    for (const FString &Reason : FLoadoutRules::ToReasons(FLoadoutRules::CollectFindings(Changed, Context)))
    {
        int32 *Left = ReasonsBefore.Find(Reason);
        if (Left && *Left > 0)
        {
            --*Left;
        }
        else
        {
            // TODO: rule reasons are built FStrings; move them to LOCTEXT tables (D-061).
            OutRefusal = FText::FromString(Reason);
            UE_LOG(LogWoRLoadout, Log, TEXT("Pick refused on loadout %d: %s"), LoadoutIndex, *Reason);
            return false;
        }
    }

    Inv->SavedLoadouts[LoadoutIndex] = MoveTemp(Changed);
    OutRefusal = FText::GetEmpty();
    return true;
}

namespace
{
    // Mirrors which primary FLoadoutRules::CollectFindings checks: a pick into a part it skips
    // would be accepted unchecked.
    bool UsesPrimaryWeapon(const FCombatLoadout &Loadout)
    {
        return Loadout.PrimarySlotType != EPrimarySlotType::Ring && Loadout.PrimaryWeapon.IsValid();
    }

    bool UsesPrimaryRing(const FCombatLoadout &Loadout, ECharacterClass CharacterClass)
    {
        return CharacterClass != ECharacterClass::Resonator && Loadout.PrimarySlotType == EPrimarySlotType::Ring &&
               Loadout.PrimaryRing.IsValid();
    }

    /** Fills the first empty (dropped) entry, else appends. */
    template <class TPart>
    void AddToFirstGap(TArray<TPart *> &Parts, TPart *Part)
    {
        const int32 Gap = Parts.Find(nullptr);
        if (Gap != INDEX_NONE)
        {
            Parts[Gap] = Part;
        }
        else
        {
            Parts.Add(Part);
        }
    }

    FText NoPrimaryWeaponText()
    {
        return LOCTEXT("PickNoWeapon", "No primary weapon is equipped.");
    }

    template <class TPart>
    FText RefuseUnlessFree(bool bPartInUse, const FText &NotInUse, const TArray<TPart *> &Parts, TPart *Pick)
    {
        if (!bPartInUse)
        {
            return NotInUse;
        }
        return Parts.Contains(Pick) ? LOCTEXT("PickAlreadyAssigned", "Already assigned.") : FText::GetEmpty();
    }
}

bool ULoadoutComponent::TryAssignWeaponAbility(int32 LoadoutIndex, UAbilityData *Ability, FText &OutRefusal)
{
    return TryChangeLoadout(LoadoutIndex, Ability != nullptr,
                            [Ability](const FCombatLoadout &Loadout, ECharacterClass)
                            {
                                return RefuseUnlessFree(UsesPrimaryWeapon(Loadout), NoPrimaryWeaponText(),
                                                        Loadout.PrimaryWeapon.AssignedAbilities, Ability);
                            },
                            [Ability](FCombatLoadout &Loadout) { AddToFirstGap(Loadout.PrimaryWeapon.AssignedAbilities, Ability); }, OutRefusal);
}

bool ULoadoutComponent::TryAssignWeaponSpell(int32 LoadoutIndex, USpellData *Spell, FText &OutRefusal)
{
    return TryChangeLoadout(LoadoutIndex, Spell != nullptr,
                            [Spell](const FCombatLoadout &Loadout, ECharacterClass)
                            {
                                return RefuseUnlessFree(UsesPrimaryWeapon(Loadout), NoPrimaryWeaponText(),
                                                        Loadout.PrimaryWeapon.AssignedSpells, Spell);
                            },
                            [Spell](FCombatLoadout &Loadout) { AddToFirstGap(Loadout.PrimaryWeapon.AssignedSpells, Spell); }, OutRefusal);
}

bool ULoadoutComponent::TryAssignRingSpell(int32 LoadoutIndex, USpellData *Spell, FText &OutRefusal)
{
    return TryChangeLoadout(LoadoutIndex, Spell != nullptr,
                            [Spell](const FCombatLoadout &Loadout, ECharacterClass CharacterClass)
                            {
                                return RefuseUnlessFree(UsesPrimaryRing(Loadout, CharacterClass), LOCTEXT("PickNoAdapter", "No primary adapter is equipped."),
                                                        Loadout.PrimaryRing.RingEntry.AssignedSpells, Spell);
                            },
                            [Spell](FCombatLoadout &Loadout) { AddToFirstGap(Loadout.PrimaryRing.RingEntry.AssignedSpells, Spell); }, OutRefusal);
}

bool ULoadoutComponent::TryAssignInnateSpell(int32 LoadoutIndex, USpellData *Spell, FText &OutRefusal)
{
    return TryChangeLoadout(LoadoutIndex, Spell != nullptr,
                            [Spell](const FCombatLoadout &Loadout, ECharacterClass)
                            { return RefuseUnlessFree(true, FText::GetEmpty(), Loadout.InnateSpells, Spell); },
                            [Spell](FCombatLoadout &Loadout) { AddToFirstGap(Loadout.InnateSpells, Spell); }, OutRefusal);
}

// ==================== BATTLE PREPARATION ====================

bool ULoadoutComponent::PrepareForBattle(UInventoryComponent *Inventory)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::PrepareForBattle] No InventoryComponent found on owner"));
        return false;
    }

    // Not initialized yet - can't prepare
    if (Inv->SavedLoadouts.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] PrepareForBattle called before initialization"));
        return false;
    }

    // B-091: a piece another party member wears leaves this loadout before the fight.
    ClaimSharedGear();

    // Soft-fail: ValidateActiveLoadout broadcasts OnValidationFailed for each
    // error but its return value is intentionally ignored. Combat proceeds
    // with whatever's valid; validation surfaces warnings, not failures.
    ValidateActiveLoadout(Inventory);

    // After surfacing the warnings, null every clearable invalidity so combat
    // starts from a guaranteed-valid loadout. Runs before ApplyAutoEquip so the
    // item-slot refill operates on the already-cleaned loadout.
    ClearInvalidSlots();

    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        // Auto-equip refills item slots from the crystal inventory at combat
        // start when the loadout's flag is set. Pass the LIVE loadout element
        // (not a by-value accessor) so the Quantity writes persist.
        FCombatLoadout &Active = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        TMap<FCrystalId, int32> Before;
        for (const FItemLoadoutSlot &ItemSlot : Active.ItemSlots)
        {
            Before.FindOrAdd(ItemSlot.CrystalId) += ItemSlot.Quantity;
        }
        FCombatLoadout::ApplyAutoEquip(Active, GetOwner());
        // B-203: what auto-equip took from the party's carried items, so the unused part goes back.
        const UCrystalInventoryComponent *CrystalInv = GetOwner()->FindComponentByClass<UCrystalInventoryComponent>();
        if (CrystalInv && CrystalInv->IsSharingItems())
        {
            TMap<FCrystalId, int32> After;
            for (const FItemLoadoutSlot &ItemSlot : Active.ItemSlots)
            {
                After.FindOrAdd(ItemSlot.CrystalId) += ItemSlot.Quantity;
            }
            for (const TPair<FCrystalId, int32> &Pair : After)
            {
                const int32 Taken = Pair.Value - Before.FindRef(Pair.Key);
                if (Taken > 0)
                {
                    BattleItemsTaken.FindOrAdd(Pair.Key) += Taken;
                }
            }
        }
    }

    bIsReadyForBattle = true;
    return true;
}

// ==================== RUNTIME COMBAT STATE ====================

bool ULoadoutComponent::UseItem(int32 SlotIndex)
{
    if (!bIsReadyForBattle)
    {
        return false;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::UseItem] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (!Loadout.ItemSlots.IsValidIndex(SlotIndex))
    {
        return false;
    }

    FItemLoadoutSlot &Slot = Loadout.ItemSlots[SlotIndex];

    if (Slot.IsEmpty())
    {
        return false;
    }

    --Slot.Quantity;

    // Combat use depletes the slot only — inventory is NOT touched (it was
    // already debited at equip/auto-equip time under the transfer model).
    OnItemUsed.Broadcast(SlotIndex, Slot.CrystalId, Slot.Quantity);
    return true;
}

int32 ULoadoutComponent::GetItemRemainingUses(int32 SlotIndex) const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetItemRemainingUses] No InventoryComponent found on owner"));
        return 0;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return 0;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (!Loadout.ItemSlots.IsValidIndex(SlotIndex))
    {
        return 0;
    }

    return Loadout.ItemSlots[SlotIndex].Quantity;
}

TArray<USkillDataBase *> ULoadoutComponent::GetAllWeaponAttacks() const
{
    TArray<USkillDataBase *> Result;

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetAllWeaponAttacks] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Primary weapon attack — OverrideAttack takes precedence over the asset attack.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid())
    {
        UWeaponData *Weapon = Loadout.PrimaryWeapon.WeaponEntry.Weapon;
        USkillDataBase *Attack = Loadout.PrimaryWeapon.OverrideAttack
                                        ? Loadout.PrimaryWeapon.OverrideAttack
                                        : (Weapon ? Weapon->WeaponAttack : nullptr);
        if (Attack)
        {
            Result.Add(Attack);
        }
    }

    // Secondary weapon attack (Generic only) — OverrideAttack takes precedence.
    if (GetCharacterClass() == ECharacterClass::Generic &&
        Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
        Loadout.SecondaryWeapon.IsValid())
    {
        UWeaponData *Weapon = Loadout.SecondaryWeapon.WeaponEntry.Weapon;
        USkillDataBase *Attack = Loadout.SecondaryWeapon.OverrideAttack
                                        ? Loadout.SecondaryWeapon.OverrideAttack
                                        : (Weapon ? Weapon->WeaponAttack : nullptr);
        if (Attack)
        {
            Result.Add(Attack);
        }
    }

    return Result;
}

TArray<UAbilityData *> ULoadoutComponent::GetAvailableAbilities() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetAvailableAbilities] No InventoryComponent found on owner"));
        return TArray<UAbilityData *>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return TArray<UAbilityData *>();
    }

    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].GetAllAbilities();
}

TArray<USpellData *> ULoadoutComponent::GetAvailableSpells() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetAvailableSpells] No InventoryComponent found on owner"));
        return TArray<USpellData *>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GetAvailableSpells] Invalid ActiveLoadoutIndex: %d"), Inv->ActiveLoadoutIndex);
        return TArray<USpellData *>();
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Broken Darkness (Model B): the castable set is the SINGLE active pool only —
    // the base Darkness pool (InnateSpells) when Darkness is active, otherwise the
    // matching absorbed-element pool. No always-on Darkness, no multi-pool append.
    if (AActor *OwnerActor = GetOwner())
    {
        UCharacterDataComponent *CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
        UBrokenDarknessManager *BDManager = OwnerActor->FindComponentByClass<UBrokenDarknessManager>();
        if (CharComp && CharComp->IsBrokenDarkness() && BDManager)
        {
            const ESpellElement Active = BDManager->GetActivePool();
            TArray<USpellData *> BDResult;
            if (Active == ESpellElement::Darkness)
            {
                BDResult = Loadout.InnateSpells;
            }
            else
            {
                for (const FBDElementSpellPool &Pool : Loadout.BDSpellPools)
                {
                    if (Pool.Element == Active)
                    {
                        BDResult = Pool.Spells;
                        break;
                    }
                }
            }
            UE_LOG(LogTemp, Verbose, TEXT("[GetAvailableSpells] BD active pool %s — returning %d spells"),
                   *UEnum::GetValueAsString(Active), BDResult.Num());
            return BDResult;
        }
    }

    // Non-BD: the full equipped spell set (unchanged).
    TArray<USpellData *> Result = Loadout.GetAllSpells();
    UE_LOG(LogTemp, Verbose, TEXT("[GetAvailableSpells] Returning %d spells"), Result.Num());
    return Result;
}

ESpellSource ULoadoutComponent::ResolveSpellSource(USpellData *Spell) const
{
    if (!Spell)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[ULoadoutComponent::ResolveSpellSource] Null spell — defaulting to Innate"));
        return ESpellSource::Innate;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[ULoadoutComponent::ResolveSpellSource] No active loadout — defaulting to Innate for spell '%s'"),
               *Spell->Name);
        return ESpellSource::Innate;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // 1. Innate pool — Caster InnateSpells plus BD per-element pools. BD-pool
    //    spells are still Refraction-source (cast as Innate, just filtered by
    //    absorption), so they belong here per the locked precedence.
    if (Loadout.InnateSpells.Contains(Spell))
    {
        return ESpellSource::Innate;
    }
    for (const FBDElementSpellPool &Pool : Loadout.BDSpellPools)
    {
        if (Pool.Spells.Contains(Spell))
        {
            return ESpellSource::Innate;
        }
    }

    // 2. Ring spells — primary ring (Generic/Caster) + Resonator ring loadout.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
    {
        if (Loadout.PrimaryRing.GetAllSpells().Contains(Spell))
        {
            return ESpellSource::RingCrystal;
        }
    }
    for (const FRingLoadoutEntry &RingEntry : Loadout.RingLoadout)
    {
        if (RingEntry.IsValid() && RingEntry.GetAllSpells().Contains(Spell))
        {
            return ESpellSource::RingCrystal;
        }
    }

    // A broken ring's F-tier spells stay castable (D-083) but drop out of its lists above (B-197).
    if (IsSpellFromBrokenRing(Spell))
    {
        return ESpellSource::RingCrystal;
    }

    // 3. Weapon spells — primary weapon (when weapon-primary) + secondary
    //    weapon (Generic only). Walks the same composition GetAllSpells uses.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid())
    {
        if (Loadout.PrimaryWeapon.GetAllSpells().Contains(Spell))
        {
            return ESpellSource::WeaponCrystal;
        }
    }
    if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
    {
        if (Loadout.SecondaryWeapon.GetAllSpells().Contains(Spell))
        {
            return ESpellSource::WeaponCrystal;
        }
    }

    // 4. Evolution spells (primary-slot evolution).
    if (Loadout.EvolutionSpells.Contains(Spell))
    {
        return ESpellSource::Evolution;
    }

    // Default — the spell isn't in any loadout source. Either a stale reference
    // or AI somehow picked a spell outside GetAvailableSpells. Log and default
    // to Innate (matches prior hardcoded behaviour, so worst case is no regression).
    UE_LOG(LogTemp, Warning,
           TEXT("[ULoadoutComponent::ResolveSpellSource] Spell '%s' not in any loadout source list — defaulting to Innate (data inconsistency)"),
           *Spell->Name);
    return ESpellSource::Innate;
}

UObject *ULoadoutComponent::FindSpellCatalystHolder(USpellData *Spell) const
{
    if (!Spell)
    {
        return nullptr;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // 1. Innate pool wins first (matches ResolveSpellSource precedence) — no
    //    catalyst holder for the caster's own refraction or BD-pool spells.
    if (Loadout.InnateSpells.Contains(Spell))
    {
        return nullptr;
    }
    for (const FBDElementSpellPool &Pool : Loadout.BDSpellPools)
    {
        if (Pool.Spells.Contains(Spell))
        {
            return nullptr;
        }
    }

    // 2. Ring catalysts — primary ring (Generic/Caster) + Resonator ring loadout.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
    {
        if (Loadout.PrimaryRing.GetAllSpells().Contains(Spell))
        {
            return Loadout.PrimaryRing.RingEntry.Ring;
        }
    }
    for (const FRingLoadoutEntry &RingEntry : Loadout.RingLoadout)
    {
        if (RingEntry.IsValid() && RingEntry.GetAllSpells().Contains(Spell))
        {
            return RingEntry.RingEntry.Ring;
        }
    }

    // 3. Weapon catalysts — primary weapon (when weapon-primary) + secondary
    //    weapon (Generic only).
    if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid())
    {
        if (Loadout.PrimaryWeapon.GetAllSpells().Contains(Spell))
        {
            return Loadout.PrimaryWeapon.WeaponEntry.Weapon;
        }
    }
    if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
    {
        if (Loadout.SecondaryWeapon.GetAllSpells().Contains(Spell))
        {
            return Loadout.SecondaryWeapon.WeaponEntry.Weapon;
        }
    }

    // Evolution / Item / unmapped — no ring/weapon catalyst.
    return nullptr;
}

TArray<USpellData *> ULoadoutComponent::GetPrimarySlotSpells() const
{
    TArray<USpellData *> Result;

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimarySlotSpells] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Ring primary
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring && Loadout.PrimaryRing.IsValid())
    {
        Result = Loadout.PrimaryRing.GetAllSpells();
    }
    // Evolution primary
    else if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        Result = Loadout.EvolutionSpells;
    }
    // Weapon primary with crystal
    else if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon && Loadout.PrimaryWeapon.IsValid())
    {
        Result = Loadout.PrimaryWeapon.GetAllSpells();
    }

    // Caster also has innate spells
    if (GetCharacterClass() == ECharacterClass::Caster)
    {
        Result.Append(Loadout.InnateSpells);
    }

    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetSecondarySlotSpells() const
{
    TArray<USpellData *> Result;

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondarySlotSpells] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Only Generic has secondary
    if (GetCharacterClass() != ECharacterClass::Generic)
    {
        return Result;
    }

    // Secondary weapon with crystal spells
    if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon && Loadout.SecondaryWeapon.IsValid())
    {
        Result = Loadout.SecondaryWeapon.GetAllSpells();
    }

    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetActiveSlotSpells() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveSlotSpells] No InventoryComponent found on owner"));
        return TArray<USpellData *>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return TArray<USpellData *>();
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Resonator uses active ring
    if (GetCharacterClass() == ECharacterClass::Resonator)
    {
        return GetRingResonateSpells();
    }

    // Generic dual-weapon: bShowPrimary IS a real gameplay toggle —
    // only the shown weapon's spells are available.
    if (GetCharacterClass() == ECharacterClass::Generic &&
        Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
        Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
    {
        return Loadout.bShowPrimary ? GetPrimarySlotSpells() : GetSecondarySlotSpells();
    }

    // All other configurations: bShowPrimary is display-only, so all spells are available.
    return GetAvailableSpells();
}

TArray<FItemLoadoutSlot> ULoadoutComponent::GetUsableItems() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetUsableItems] No InventoryComponent found on owner"));
        return TArray<FItemLoadoutSlot>();
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return TArray<FItemLoadoutSlot>();
    }

    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].GetUsableItemSlots();
}

int32 ULoadoutComponent::GetUsableItemCount() const
{
    return GetUsableItems().Num();
}

// ==================== POST-BATTLE ====================

void ULoadoutComponent::ResetBattleState()
{
    bIsReadyForBattle = false;
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || BattleItemsTaken.IsEmpty() || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        BattleItemsTaken.Reset();
        return;
    }
    // B-203: unused crystals taken from the party's carried items go back (what fits; the rest is stored).
    int32 Returned = 0;
    for (FItemLoadoutSlot &ItemSlot : Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].ItemSlots)
    {
        int32 *Taken = BattleItemsTaken.Find(ItemSlot.CrystalId);
        const int32 Back = Taken ? FMath::Min(*Taken, ItemSlot.Quantity) : 0;
        if (Back > 0 && Inv->AddCrystal(ItemSlot.CrystalId, Back))
        {
            ItemSlot.Quantity -= Back;
            *Taken -= Back;
            Returned += Back;
        }
    }
    BattleItemsTaken.Reset();
    UE_LOG(LogWoRLoadout, Log, TEXT("%s: %d unused crystal(s) back to the party's items"), *GetNameSafe(GetOwner()), Returned);
}

// ==================== QUICK SETUP ====================

bool ULoadoutComponent::AutoPopulateLoadout(int32 LoadoutIndex, UInventoryComponent *Inventory)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::AutoPopulateLoadout] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(LoadoutIndex) || !Inventory)
    {
        return false;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[LoadoutIndex];
    Loadout.Clear();
    Loadout.InitializeForClass(GetCharacterClass());

    // Auto-assign first available weapon
    if (Inventory->Weapons.Num() > 0)
    {
        Loadout.PrimaryWeapon.WeaponEntry = Inventory->Weapons[0];
        Loadout.PrimaryWeapon.InitializeFromWeapon();
    }

    // Auto-assign items (one of each type, highest tier first)
    // TODO: Implement smarter auto-population

    return true;
}

void ULoadoutComponent::ClearLoadout(int32 LoadoutIndex)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ClearLoadout] No InventoryComponent found on owner"));
        return;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(LoadoutIndex))
    {
        return;
    }

    Inv->SavedLoadouts[LoadoutIndex].Clear();
    Inv->SavedLoadouts[LoadoutIndex].InitializeForClass(GetCharacterClass());
}

void ULoadoutComponent::InitializeFromCharacterData(UCharacterData *CharacterData, UInventoryComponent *Inventory)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    if (!CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] InitializeFromCharacterData: Null CharacterData"));
        return;
    }

    // Set class from CharacterData
    CharacterClass = CharacterData->CharacterClass;

    // New path: InventoryComponent has already populated SavedLoadouts +
    // ActiveLoadoutIndex from UInventoryData. LC just sets class, applies
    // BD pool runtime flag, and returns.
    if (CharacterData->Inventory)
    {
        ApplyBDPoolsIfBroken();
        UE_LOG(LogTemp, Display,
               TEXT("[LoadoutComponent] %s: Initialized from UInventoryData (SavedLoadouts populated by InventoryComponent)"),
               *CharacterData->Name);
        return;
    }

    // No Inventory - create empty loadout for manual setup
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::InitializeFromCharacterData] No InventoryComponent found on owner — cannot seed empty loadout"));
        return;
    }
    Inv->SavedLoadouts.Empty();
    FCombatLoadout EmptyLoadout;
    EmptyLoadout.LoadoutName = TEXT("Default");
    EmptyLoadout.InitializeForClass(CharacterData->CharacterClass);
    Inv->SavedLoadouts.Add(EmptyLoadout);
    Inv->ActiveLoadoutIndex = 0;
    bIsReadyForBattle = false;

    UE_LOG(LogTemp, Warning, TEXT("[LoadoutComponent] %s has no Inventory - created empty loadout. Assign UInventoryData on CharacterData."),
           *CharacterData->Name);

    // Broken Darkness characters get their element-spell pools initialised.
    ApplyBDPoolsIfBroken();
}

void ULoadoutComponent::InitializeBDPools(FCombatLoadout &Loadout)
{
    // One pool per absorbable element other than Darkness. Darkness is the
    // always-available default pool and lives in InnateSpells, not here.
    // Idempotent — existing pools are kept, only missing ones are added.
    // None is the upper sentinel: it sits just past the real absorbable elements,
    // so iterating [0, None) covers Generic..Reality. The loop body skips Darkness
    // and anything CanAbsorbElement rejects (Generic / None / Reality), leaving the
    // 7 element pools (Fire/Water/Earth/Wind/Light/Lightning/Void).
    for (uint8 i = 0; i < static_cast<uint8>(ESpellElement::None); ++i)
    {
        const ESpellElement Elem = static_cast<ESpellElement>(i);
        if (Elem == ESpellElement::Darkness || !UBrokenDarknessManager::CanAbsorbElement(Elem))
        {
            continue;
        }

        const bool bExists = Loadout.BDSpellPools.ContainsByPredicate(
            [Elem](const FBDElementSpellPool &Pool) { return Pool.Element == Elem; });
        if (!bExists)
        {
            FBDElementSpellPool NewPool;
            NewPool.Element = Elem;
            Loadout.BDSpellPools.Add(NewPool);
        }
    }
}

void ULoadoutComponent::ApplyBDPoolsIfBroken()
{
    AActor *OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    UCharacterDataComponent *CharComp = OwnerActor->FindComponentByClass<UCharacterDataComponent>();
    if (!CharComp || !CharComp->IsBrokenDarkness())
    {
        return;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ApplyBDPoolsIfBroken] No InventoryComponent found on owner"));
        return;
    }
    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        // Pass by reference into the live inventory array so InitializeBDPools'
        // mutations land on the active loadout, not a copy.
        InitializeBDPools(Inv->SavedLoadouts[Inv->ActiveLoadoutIndex]);
    }
}

// ==================== COMBAT ACCESSORS ====================

void ULoadoutComponent::NotifyActiveWeaponChanged()
{
    // B-302: every gear change announces here, so the unified slot mirror (D-110) is rebuilt first.
    if (UInventoryComponent *Inv = GetInventoryComponent())
    {
        if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        {
            Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].RebuildSlotsFromLegacy(GetCharacterClass());
        }
    }
    // GetActiveWeapon warns when there is no inventory; with none there is no weapon to announce.
    UWeaponData *ActiveWeapon = GetInventoryComponent() ? GetActiveWeapon() : nullptr;
    if (ActiveWeapon == LastNotifiedWeapon)
    {
        return;
    }
    LastNotifiedWeapon = ActiveWeapon;
    UE_LOG(LogWoRLoadout, Log, TEXT("%s: active weapon is now %s"), *GetNameSafe(GetOwner()),
           ActiveWeapon ? *ActiveWeapon->Name : TEXT("none"));
    OnActiveWeaponChanged.Broadcast(ActiveWeapon);
}

UWeaponData *ULoadoutComponent::GetActiveWeapon() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveWeapon] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    const bool bIsGeneric = GetCharacterClass() == ECharacterClass::Generic;

    // Evolution primary = no weapon access
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        // Evolved Generic uses secondary weapon
        if (bIsGeneric &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
        }
        // Caster/Resonator have no weapon when evolved
        return nullptr;
    }

    // Ring primary - bShowPrimary controls stance display only, not combat capability.
    // Generic with weapon-secondary always has weapon access; Caster/Resonator have none.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        if (bIsGeneric &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
        }

        return nullptr;
    }

    // Weapon primary - class-specific behavior
    if (bIsGeneric)
    {
        // Generic dual-weapon: bShowPrimary IS a real gameplay toggle.
        if (Loadout.bShowPrimary)
        {
            return Loadout.PrimaryWeapon.IsValid() ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
        }
        else if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
        }
        return nullptr;
    }

    // Caster/Resonator: weapon equipped iff primary slot has weapon.
    // bShowPrimary controls stance display only, not combat capability.
    return Loadout.PrimaryWeapon.IsValid() ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
}

UWeaponData *ULoadoutComponent::GetPrimaryWeapon() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryWeapon] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
    {
        return nullptr;
    }

    return Loadout.PrimaryWeapon.IsValid() ? Loadout.PrimaryWeapon.WeaponEntry.Weapon : nullptr;
}

UWeaponData *ULoadoutComponent::GetSecondaryWeapon() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondaryWeapon] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (GetCharacterClass() != ECharacterClass::Generic)
    {
        return nullptr;
    }

    if (Loadout.SecondarySlotType != ESecondarySlotType::Weapon)
    {
        return nullptr;
    }

    return Loadout.SecondaryWeapon.IsValid() ? Loadout.SecondaryWeapon.WeaponEntry.Weapon : nullptr;
}

URingData *ULoadoutComponent::GetPrimaryRing() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryRing] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
    {
        return nullptr;
    }

    return Loadout.PrimaryRing.IsValid() ? Loadout.PrimaryRing.RingEntry.Ring : nullptr;
}

URingData *ULoadoutComponent::GetActiveRing() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveRing] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Only Resonators have ring loadout
    if (GetCharacterClass() != ECharacterClass::Resonator)
    {
        return nullptr;
    }

    if (!Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
    {
        return nullptr;
    }

    const FRingLoadoutEntry &Entry = Loadout.RingLoadout[Loadout.ActiveRingIndex];
    return Entry.IsValid() ? Entry.RingEntry.Ring : nullptr;
}

UEvolutionItemData *ULoadoutComponent::GetPrimaryEvolution() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryEvolution] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
    {
        return nullptr;
    }

    return Loadout.PrimaryEvolution.Item;
}

TArray<FEquippedCrystalSlot> ULoadoutComponent::GetEquippedCrystals() const
{
    TArray<FEquippedCrystalSlot> Result;

    auto AddAttachment = [&Result](const FRuntimeAttachedItem &Attachment, UObject *Holder)
    {
        if (Attachment.IsEmpty() || !Holder)
        {
            return;
        }

        FEquippedCrystalSlot Slot;
        Slot.Holder = Holder;
        Slot.Kind = Attachment.Kind;
        if (Attachment.IsCrystal())
        {
            Slot.CrystalId = Attachment.Crystal.Id;
        }
        else if (Attachment.IsAugmentStone())
        {
            // A augment stone stores its FCrystalId{sub-type, Tier} in the Crystal
            // slot — carry it so the emitted crystal slot keeps its identity.
            Slot.CrystalId = Attachment.Crystal.Id;
        }
        else if (Attachment.IsEvolution())
        {
            Slot.Item = Attachment.Evolution.Item;
        }

        Result.Add(Slot);
    };

    // Weapons — read attachment from the runtime inventory entry, not the asset.
    // The asset-side UWeaponData::AttachedItem field is a design-time default
    // and carries no runtime durability/broken state; the runtime attachment
    // preserves state across combats and is the actual source of truth for
    // "what is attached to this weapon right now."
    if (const FWeaponLoadoutEntry *PrimaryLoadout = GetPrimaryWeaponLoadout())
    {
        AddAttachment(PrimaryLoadout->WeaponEntry.AttachedItem,
                      PrimaryLoadout->WeaponEntry.Weapon);
    }
    if (const FWeaponLoadoutEntry *SecondaryLoadout = GetSecondaryWeaponLoadout())
    {
        AddAttachment(SecondaryLoadout->WeaponEntry.AttachedItem,
                      SecondaryLoadout->WeaponEntry.Weapon);
    }

    // Primary ring (Generic / Caster) — runtime entry
    if (const FRingLoadoutEntry *PrimaryRingLoadout = GetPrimaryRingLoadout())
    {
        AddAttachment(PrimaryRingLoadout->RingEntry.AttachedItem,
                      PrimaryRingLoadout->RingEntry.Ring);
    }

    // Resonator ring loadout — every ring in the array, runtime entry
    const FCombatLoadout ActiveLoadout = GetActiveLoadout();
    for (const FRingLoadoutEntry &Entry : ActiveLoadout.RingLoadout)
    {
        AddAttachment(Entry.RingEntry.AttachedItem, Entry.RingEntry.Ring);
    }

    // Evolution crystal slot — no inventory entry, evolution IS the crystal.
    if (UEvolutionItemData *Evolution = GetPrimaryEvolution())
    {
        FEquippedCrystalSlot Slot;
        Slot.Holder = Evolution;
        Slot.Kind = EAttachedItemKind::Evolution;
        Slot.Item = Evolution;
        Result.Add(Slot);
    }

    return Result;
}

bool ULoadoutComponent::HasEquippedSourceForElement(AActor *Actor, ESpellElement Element)
{
    if (!Actor)
    {
        return false;
    }

    const ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>();
    if (!Loadout)
    {
        return false;
    }

    // GetEquippedCrystals already covers weapon/ring crystals and the primary
    // evolution slot. A crystal channels its associated element.
    for (const FEquippedCrystalSlot &Slot : Loadout->GetEquippedCrystals())
    {
        switch (Slot.Kind)
        {
        case EAttachedItemKind::Crystal:
            if (ItemIdentity::GetElement(Slot.CrystalId) == Element)
            {
                return true;
            }
            break;
        case EAttachedItemKind::Evolution:
            if (Slot.Item && Slot.Item->GetAssociatedElement() == Element)
            {
                return true;
            }
            break;
        default:
            break;
        }
    }

    return false;
}

EPrimarySlotType ULoadoutComponent::GetPrimarySlotType() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimarySlotType] No InventoryComponent found on owner"));
        return EPrimarySlotType::None;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return EPrimarySlotType::None;
    }
    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].PrimarySlotType;
}

ESecondarySlotType ULoadoutComponent::GetSecondarySlotType() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondarySlotType] No InventoryComponent found on owner"));
        return ESecondarySlotType::None;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return ESecondarySlotType::None;
    }
    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].SecondarySlotType;
}

bool ULoadoutComponent::IsShowingPrimary() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::IsShowingPrimary] No InventoryComponent found on owner"));
        return true;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return true;
    }
    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].bShowPrimary;
}

void ULoadoutComponent::ToggleEquipment()
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::ToggleEquipment] No InventoryComponent found on owner"));
        return;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    const TArray<FGatheredEffect> OldGearEffects = GetActiveEffectsGathered(GetOwner());
    Loadout.bShowPrimary = !Loadout.bShowPrimary;

    UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] Toggled equipment: bShowPrimary = %s"),
           Loadout.bShowPrimary ? TEXT("true") : TEXT("false"));

    // Notify TurnManager so cached BonusTurnSpeed re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (UWorld *World = GetWorld())
    {
        if (UGameInstance *GI = World->GetGameInstance())
        {
            if (UTurnManager *TurnManager = GI->GetSubsystem<UTurnManager>())
            {
                TurnManager->OnActorSpeedChanged(GetOwner());
            }
        }
    }

    SwapGearEffects(OldGearEffects);

    // Active equipment changed → recompute the stat-derived pools (equipment
    // BonusMaxHP/BonusMaxEnergy; the weapon's and ring's pool stones, B-082). Does NOT
    // touch CurrentHP/EP (overcap-not-clamp) — see RecomputeMaxPools doc.
    if (UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr)
    {
        CharComp->RecomputeMaxPools();
    }
}

bool ULoadoutComponent::HasWeaponAccess() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::HasWeaponAccess] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    const bool bIsGeneric = GetCharacterClass() == ECharacterClass::Generic;

    // Evolution primary = no weapon access
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        // Evolved Generic can still use secondary weapon
        if (bIsGeneric &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid())
        {
            return true;
        }
        // Caster/Resonator lose all weapon access when evolved
        return false;
    }

    // Ring primary - only Generic with secondary weapon has access
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        return bIsGeneric &&
               Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
               Loadout.SecondaryWeapon.IsValid();
    }

    // Weapon primary - check if any weapon exists
    return Loadout.PrimaryWeapon.IsValid() ||
           (bIsGeneric &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid());
}

bool ULoadoutComponent::IsArmed() const
{
    // Armed iff combat layer has a weapon. bShowPrimary controls stance display only.
    return GetActiveWeapon() != nullptr;
}
bool ULoadoutComponent::IsEvolved() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::IsEvolved] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    return Inv->SavedLoadouts[Inv->ActiveLoadoutIndex].PrimarySlotType == EPrimarySlotType::Evolution;
}
// ==================== EQUIPMENT STATE HELPERS ====================

bool ULoadoutComponent::HasSecondaryEquipment() const
{
    if (GetCharacterClass() != ECharacterClass::Generic)
    {
        return false;
    }

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::HasSecondaryEquipment] No InventoryComponent found on owner"));
        return false;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // B-099 (D-027): an evolution primary keeps the secondary weapon (GetActiveWeaponLoadout
    // fights with it, FCombatCapabilities lists it).
    if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
    {
        return Loadout.SecondaryWeapon.IsValid();
    }

    return false;
}

bool ULoadoutComponent::HasRingInSecondary() const
{
    // The Generic secondary slot holds a weapon only (ESecondarySlotType), whatever the primary.
    return false;
}

// ==================== STANCE & ANIMATION ====================

UStanceData *ULoadoutComponent::GetCurrentStance() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetCurrentStance] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Stance is a visual concern, decoupled from combat capability:
    // bShowPrimary picks which slot is currently DISPLAYED, regardless of
    // whether the other slot's weapon is also available for combat actions.
    const FWeaponLoadoutEntry *ShownWeapon = nullptr;

    if (Loadout.bShowPrimary)
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            ShownWeapon = Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
        }
        // B-099 (D-027): an evolution has no stance, and the Generic fights with its secondary weapon.
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution &&
                 GetCharacterClass() == ECharacterClass::Generic &&
                 Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            ShownWeapon = Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
        }
        // Ring primary while showing primary => no weapon stance, fall through to unarmed.
    }
    else
    {
        // Showing secondary. Only Generic has secondary slots that can hold a weapon.
        if (GetCharacterClass() == ECharacterClass::Generic &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
        {
            ShownWeapon = Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
        }
    }

    if (ShownWeapon && ShownWeapon->WeaponEntry.Weapon)
    {
        if (ShownWeapon->WeaponEntry.Weapon->WeaponStance)
        {
            return ShownWeapon->WeaponEntry.Weapon->WeaponStance;
        }
    }

    return GetUnarmedStance();
}

UAnimMontage *ULoadoutComponent::GetCurrentIdleMontage() const
{
    UStanceData *Stance = GetCurrentStance();
    return Stance ? Stance->IdleAnimMontage : nullptr;
}

USkillDataBase *ULoadoutComponent::GetCurrentAttack() const
{
    if (!IsArmed())
    {
        return nullptr;
    }

    const FWeaponLoadoutEntry *WeaponEntry = GetActiveWeaponLoadout();
    if (!WeaponEntry)
    {
        return nullptr;
    }

    // Per-loadout attack override takes precedence over the weapon asset's attack.
    if (WeaponEntry->OverrideAttack)
    {
        return WeaponEntry->OverrideAttack;
    }

    return WeaponEntry->WeaponEntry.Weapon ? WeaponEntry->WeaponEntry.Weapon->WeaponAttack : nullptr;
}

// ==================== SPELL ACCESS ====================

TArray<USpellData *> ULoadoutComponent::GetWeaponResonateSpells() const
{
    TArray<USpellData *> Result;

    const FWeaponLoadoutEntry *WeaponEntry = GetActiveWeaponLoadout();
    if (!WeaponEntry)
    {
        return Result;
    }

    // Only evolved weapons have resonate spells
    if (!WeaponEntry->WeaponEntry.IsEvolved())
    {
        return Result;
    }

    if (!WeaponEntry->WeaponEntry.AttachedItem.IsEmpty())
    {
        // Get spells from weapon's assigned spells (no longer from crystal)
        Result = WeaponEntry->WeaponEntry.AssignedSpells;
    }
    return Result;
}

TArray<USpellData *> ULoadoutComponent::GetRingResonateSpells() const
{
    TArray<USpellData *> Result;

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetRingResonateSpells] No InventoryComponent found on owner"));
        return Result;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Result;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Caster with ring primary
    if (GetCharacterClass() == ECharacterClass::Caster && Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        const FRingLoadoutEntry *RingEntry = GetPrimaryRingLoadout();
        if (RingEntry)
        {
            return RingEntry->GetAllSpells();
        }
    }

    // Resonator uses active ring
    if (GetCharacterClass() == ECharacterClass::Resonator)
    {
        const FRingLoadoutEntry *RingEntry = GetActiveRingLoadout();
        if (RingEntry)
        {
            return RingEntry->GetAllSpells();
        }
    }

    return Result;
}

// ==================== LOADOUT ENTRY ACCESSORS ====================

const FWeaponLoadoutEntry *ULoadoutComponent::GetActiveWeaponLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveWeaponLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    const bool bIsGeneric = GetCharacterClass() == ECharacterClass::Generic;

    // Evolution primary = no weapon
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        // Evolved Generic uses secondary weapon
        if (bIsGeneric &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid())
        {
            return &Loadout.SecondaryWeapon;
        }
        return nullptr;
    }

    // Ring primary - bShowPrimary controls stance display only, not combat capability.
    // Generic with weapon-secondary always has weapon access; Caster/Resonator have none.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
    {
        if (bIsGeneric &&
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
            return Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;

        return nullptr;
    }

    // Weapon primary - class-specific behavior
    if (bIsGeneric)
    {
        // Generic dual-weapon: bShowPrimary IS a real gameplay toggle.
        if (Loadout.bShowPrimary)
            return Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
        else if (Loadout.SecondarySlotType == ESecondarySlotType::Weapon)
            return Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
        return nullptr;
    }

    // Caster/Resonator: weapon equipped iff primary slot has weapon.
    // bShowPrimary controls stance display only, not combat capability.
    return Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
}

const FRuntimeAttachedItem *ULoadoutComponent::GetStoneRingAttachment() const
{
    const FRingLoadoutEntry *Ring = GetActiveRingLoadout();
    Ring = Ring ? Ring : GetPrimaryRingLoadout();
    return Ring ? &Ring->RingEntry.AttachedItem : nullptr;
}

float ULoadoutComponent::GetAttachedStonePercent(ESubStat Stat) const
{
    float Total = 0.0f;
    for (const FRuntimeAttachedItem *Attachment : {GetActiveWeaponAttachment(), GetStoneRingAttachment()})
    {
        Total += Attachment ? CrystalEffectTable::GetAttachedStonePercent(*Attachment, Stat) : 0.0f;
    }
    return Total;
}

float ULoadoutComponent::GetAttachedStonePercentForType(ECrystalType StoneType) const
{
    float Total = 0.0f;
    for (const FRuntimeAttachedItem *Attachment : {GetActiveWeaponAttachment(), GetStoneRingAttachment()})
    {
        Total += Attachment ? CrystalEffectTable::GetAttachedStonePercentForType(*Attachment, StoneType) : 0.0f;
    }
    return Total;
}

const FRuntimeAttachedItem *ULoadoutComponent::GetActiveWeaponAttachment() const
{
    // Single source for the GetActiveWeaponLoadout()->WeaponEntry.GetAttachedItem() idiom.
    // Returns a pointer into the live loadout entry, or nullptr when no active weapon — so
    // callers null-check exactly where they previously guarded the active-weapon lookup.
    if (const FWeaponLoadoutEntry *ActiveWeapon = GetActiveWeaponLoadout())
    {
        return &ActiveWeapon->WeaponEntry.GetAttachedItem();
    }
    return nullptr;
}

const FWeaponLoadoutEntry *ULoadoutComponent::GetPrimaryWeaponLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryWeaponLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
        return nullptr;

    return Loadout.PrimaryWeapon.IsValid() ? &Loadout.PrimaryWeapon : nullptr;
}

const FWeaponLoadoutEntry *ULoadoutComponent::GetSecondaryWeaponLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetSecondaryWeaponLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (GetCharacterClass() != ECharacterClass::Generic)
        return nullptr;

    if (Loadout.SecondarySlotType != ESecondarySlotType::Weapon)
        return nullptr;

    return Loadout.SecondaryWeapon.IsValid() ? &Loadout.SecondaryWeapon : nullptr;
}

const FRingLoadoutEntry *ULoadoutComponent::GetPrimaryRingLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetPrimaryRingLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (Loadout.PrimarySlotType != EPrimarySlotType::Ring)
        return nullptr;

    return Loadout.PrimaryRing.RingEntry.Ring != nullptr ? &Loadout.PrimaryRing : nullptr;
}

const FRingLoadoutEntry *ULoadoutComponent::GetActiveRingLoadout() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::GetActiveRingLoadout] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
        return nullptr;

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (GetCharacterClass() != ECharacterClass::Resonator)
        return nullptr;

    if (!Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex))
        return nullptr;

    const FRingLoadoutEntry &Entry = Loadout.RingLoadout[Loadout.ActiveRingIndex];
    return Entry.IsValid() ? &Entry : nullptr;
}

FText ULoadoutComponent::GetTurnSwitchRefusedReason() const
{
    return bUsedTurnSwitch ? LOCTEXT("TurnSwitchUsed", "Already switched this turn.") : FText::GetEmpty();
}

bool ULoadoutComponent::TrySwitchWeaponThisTurn(FText &OutReason)
{
    OutReason = GetTurnSwitchRefusedReason();
    if (!OutReason.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] %s: weapon switch refused - already switched this turn"), *GetNameSafe(GetOwner()));
        return false;
    }
    // A toggle that changes nothing (no loadout) doesn't use up the turn's switch.
    const bool bShowPrimaryBefore = GetActiveLoadout().bShowPrimary;
    ToggleEquipment();
    bUsedTurnSwitch = GetActiveLoadout().bShowPrimary != bShowPrimaryBefore;
    return bUsedTurnSwitch;
}

bool ULoadoutComponent::TrySwitchRingThisTurn(int32 NewRingIndex, FText &OutReason)
{
    OutReason = GetTurnSwitchRefusedReason();
    if (!OutReason.IsEmpty())
    {
        UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] %s: ring switch refused - already switched this turn"), *GetNameSafe(GetOwner()));
        return false;
    }
    // A ring switch that changes nothing (bad or current index) doesn't use up the turn's switch.
    const int32 RingIndexBefore = GetActiveLoadout().ActiveRingIndex;
    SetActiveRingIndex(NewRingIndex);
    bUsedTurnSwitch = GetActiveLoadout().ActiveRingIndex != RingIndexBefore;
    return bUsedTurnSwitch;
}

void ULoadoutComponent::ResetTurnSwitch()
{
    bUsedTurnSwitch = false;
}

FText ULoadoutComponent::GetBrokenSourceReason(const USpellData *Spell) const
{
    if (!Spell)
    {
        return FText::GetEmpty();
    }
    const auto Lists = [Spell](const TArray<USpellData *> &Spells)
    {
        return Spells.ContainsByPredicate([Spell](const USpellData *Assigned) { return Assigned == Spell; });
    };

    // An innate spell is cast as innate even when a crystal also holds it (ResolveSpellSource's order).
    const UInventoryComponent *Inv = GetInventoryComponent();
    if (Inv && Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        const bool bInnate = Lists(Loadout.InnateSpells) ||
                             Loadout.BDSpellPools.ContainsByPredicate([&Lists](const FBDElementSpellPool &Pool) { return Lists(Pool.Spells); });
        if (bInnate)
        {
            return FText::GetEmpty();
        }
        // A broken primary evolution is kept (Q-159), so its Breakthrough spells grey out (D-083).
        if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.IsBroken() &&
            Lists(Loadout.EvolutionSpells))
        {
            return SourceBreakRules::BrokenEvolutionReason();
        }
        // An intact ring is cast from before any weapon (ResolveSpellSource's order).
        const auto IntactRingLists = [&Lists](const FRingLoadoutEntry &Ring)
        {
            const FRuntimeAttachedItem &Attached = Ring.RingEntry.AttachedItem;
            return Ring.IsValid() && !(Attached.IsSpellSource() && Attached.IsBroken()) && Lists(Ring.GetAllSpells());
        };
        if ((Loadout.PrimarySlotType == EPrimarySlotType::Ring && IntactRingLists(Loadout.PrimaryRing)) ||
            Loadout.RingLoadout.ContainsByPredicate(IntactRingLists))
        {
            return FText::GetEmpty();
        }
    }

    // B-204 (Q-204): the primary, then the secondary weapon (FCombatLoadout::GetAllSpells lists both);
    // the first that holds the spell is the one the cast channels (FindSpellCatalystHolder's order).
    for (const FWeaponLoadoutEntry *Weapon : {GetPrimaryWeaponLoadout(), GetSecondaryWeaponLoadout()})
    {
        // B-197: the loadout's own overrides (FWeaponLoadoutEntry::AssignedSpells) come through the
        // same crystal, so they grey out with it too.
        if (!Weapon || !(Lists(Weapon->WeaponEntry.AssignedSpells) || Lists(Weapon->AssignedSpells)))
        {
            continue;
        }
        const FRuntimeAttachedItem &Attached = Weapon->WeaponEntry.AttachedItem;
        if (Attached.IsSpellSource() && Attached.IsBroken())
        {
            return SourceBreakRules::BrokenWeaponCrystalReason();
        }
        break;
    }
    if (IsSpellFromBrokenRing(Spell) && !SourceBreakRules::IsRingSpellUsableWhenBroken(Spell->Tier))
    {
        return SourceBreakRules::BrokenRingCrystalReason();
    }
    return FText::GetEmpty();
}

bool ULoadoutComponent::IsSpellFromBrokenRing(const USpellData *Spell) const
{
    const FRingLoadoutEntry *Ring = GetActiveRingLoadout() ? GetActiveRingLoadout() : GetPrimaryRingLoadout();
    if (!Spell || !Ring)
    {
        return false;
    }
    const FRuntimeAttachedItem &Attached = Ring->RingEntry.AttachedItem;
    if (!Attached.IsSpellSource() || !Attached.IsBroken())
    {
        return false;
    }
    const auto Lists = [Spell](const TArray<USpellData *> &Spells)
    {
        return Spells.ContainsByPredicate([Spell](const USpellData *Assigned) { return Assigned == Spell; });
    };
    // An innate spell is cast as innate even when the ring also holds it (ResolveSpellSource's order).
    const UInventoryComponent *Inv = GetInventoryComponent();
    if (Inv && Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        if (Lists(Loadout.InnateSpells) ||
            Loadout.BDSpellPools.ContainsByPredicate([&Lists](const FBDElementSpellPool &Pool) { return Lists(Pool.Spells); }))
        {
            return false;
        }
    }
    // Raw lists, not the capped GetAllSpells(true): the player's capabilities list them all, and each must be greyed.
    return Lists(Ring->RingEntry.AssignedSpells) || (Ring->RingEntry.Ring && Lists(Ring->RingEntry.Ring->PresetSpells));
}

FRuntimeAttachedItem *ULoadoutComponent::FindAttachedItemByHolder(UObject *Holder)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::FindAttachedItemByHolder] No InventoryComponent found on owner"));
        return nullptr;
    }
    if (!Holder || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    // Weapon holder — match by Weapon pointer across primary/secondary slots,
    // regardless of PrimarySlotType. A holder lookup is by identity, not slot.
    if (UWeaponData *Weapon = Cast<UWeaponData>(Holder))
    {
        if (Loadout.PrimaryWeapon.WeaponEntry.Weapon == Weapon)
        {
            return &Loadout.PrimaryWeapon.WeaponEntry.AttachedItem;
        }
        if (Loadout.SecondaryWeapon.WeaponEntry.Weapon == Weapon)
        {
            return &Loadout.SecondaryWeapon.WeaponEntry.AttachedItem;
        }
        return nullptr;
    }

    // Ring holder — primary ring slot (Generic/Caster), then Resonator
    // RingLoadout array.
    if (URingData *Ring = Cast<URingData>(Holder))
    {
        if (Loadout.PrimaryRing.RingEntry.Ring == Ring)
        {
            return &Loadout.PrimaryRing.RingEntry.AttachedItem;
        }
        for (FRingLoadoutEntry &Entry : Loadout.RingLoadout)
        {
            if (Entry.RingEntry.Ring == Ring)
            {
                return &Entry.RingEntry.AttachedItem;
            }
        }
        return nullptr;
    }

    return nullptr;
}

FRuntimeAttachedItem ULoadoutComponent::GetCrystalEntryByHolder(UObject *Holder) const
{
    // const_cast: the internal helper is non-const because it's shared with
    // the mutating wrappers below. Get does not mutate — it takes a copy.
    FRuntimeAttachedItem *Entry = const_cast<ULoadoutComponent *>(this)->FindAttachedItemByHolder(Holder);
    return Entry ? *Entry : FRuntimeAttachedItem();
}

void ULoadoutComponent::ResetCrystalEntryByHolder(UObject *Holder)
{
    if (FRuntimeAttachedItem *Entry = FindAttachedItemByHolder(Holder))
    {
        *Entry = FRuntimeAttachedItem();
    }
}

int32 ULoadoutComponent::RepairCrystalEntryByHolder(UObject *Holder, int32 Amount)
{
    FRuntimeAttachedItem *Entry = FindAttachedItemByHolder(Holder);
    if (!Entry)
    {
        return -1;
    }
    Entry->RepairBetweenCombats(Amount);
    return Entry->GetCurrentDurability();
}

bool ULoadoutComponent::ApplyWearToCrystalEntryByHolder(UObject *Holder, int32 Amount)
{
    FRuntimeAttachedItem *Entry = FindAttachedItemByHolder(Holder);
    if (!Entry)
    {
        return false;
    }
    return Entry->ApplyWear(Amount);
}

bool ULoadoutComponent::ApplyWearToActivePrimaryEvolution(int32 Amount, bool bForceWear)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
    {
        return false;
    }

    return Loadout.PrimaryEvolution.ApplyWear(Amount, bForceWear);
}

int32 ULoadoutComponent::RepairActivePrimaryEvolution(int32 Amount)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return 0;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
    {
        return 0;
    }

    return Loadout.PrimaryEvolution.RepairBetweenCombats(Amount);
}

namespace LoadoutCarryInternal
{
    /** Every weapon and ring slot of Loadout that can hold an attachment, in a fixed order. */
    template <typename TLoadout, typename FFunc>
    void ForEachAttachmentSlot(TLoadout &Loadout, FFunc &&Func)
    {
        Func(static_cast<UObject *>(Loadout.PrimaryWeapon.WeaponEntry.Weapon), Loadout.PrimaryWeapon.WeaponEntry.AttachedItem);
        Func(static_cast<UObject *>(Loadout.SecondaryWeapon.WeaponEntry.Weapon), Loadout.SecondaryWeapon.WeaponEntry.AttachedItem);
        Func(static_cast<UObject *>(Loadout.PrimaryRing.RingEntry.Ring), Loadout.PrimaryRing.RingEntry.AttachedItem);
        for (auto &Entry : Loadout.RingLoadout)
        {
            Func(static_cast<UObject *>(Entry.RingEntry.Ring), Entry.RingEntry.AttachedItem);
        }
    }
}

FCarriedGearState ULoadoutComponent::SnapshotCarriedGear() const
{
    FCarriedGearState Carried;
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return Carried;
    }

    const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    LoadoutCarryInternal::ForEachAttachmentSlot(Loadout, [&Carried](UObject *Holder, const FRuntimeAttachedItem &Attached)
    {
        if (Holder)
        {
            Carried.Attachments.Add({Holder, Attached});
        }
    });
    Carried.bHasPrimaryEvolution = Loadout.PrimarySlotType == EPrimarySlotType::Evolution;
    if (Carried.bHasPrimaryEvolution)
    {
        Carried.PrimaryEvolution = Loadout.PrimaryEvolution;
    }
    Carried.bCarried = true;
    return Carried;
}

int32 ULoadoutComponent::ApplyCarriedGear(const FCarriedGearState &Carried)
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Carried.bCarried || !Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return 0;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    // Matched in slot order, each carried entry used once, so two copies of one ring keep their own state.
    TBitArray<> Used(false, Carried.Attachments.Num());
    int32 Restored = 0;
    LoadoutCarryInternal::ForEachAttachmentSlot(Loadout, [&Carried, &Used, &Restored](UObject *Holder, FRuntimeAttachedItem &Attached)
    {
        for (int32 i = 0; Holder && i < Carried.Attachments.Num(); ++i)
        {
            if (!Used[i] && Carried.Attachments[i].Holder == Holder)
            {
                Attached = Carried.Attachments[i].Attached;
                Used[i] = true;
                ++Restored;
                return;
            }
        }
    });
    if (Carried.bHasPrimaryEvolution && Loadout.PrimarySlotType == EPrimarySlotType::Evolution &&
        Carried.PrimaryEvolution.Item == Loadout.PrimaryEvolution.Item)
    {
        Loadout.PrimaryEvolution = Carried.PrimaryEvolution;
    }
    Inv->WriteLoadoutToOwned(Inv->ActiveLoadoutIndex); // B-085: the owned gear starts the fight as carried

    UE_LOG(LogTemp, Log, TEXT("[ULoadoutComponent] %s: carried crystal state restored (%d of %d attachments%s)"),
           GetOwner() ? *GetOwner()->GetName() : TEXT("<null>"), Restored, Carried.Attachments.Num(),
           Carried.bHasPrimaryEvolution ? TEXT(", primary evolution") : TEXT(""));
    return Restored;
}

bool ULoadoutComponent::ClearBrokenPrimaryEvolution()
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.IsBroken())
    {
        Loadout.PrimaryEvolution = FEvolutionAttachment();
        Loadout.PrimaryEvolutionInstance = FGuid(); // clear the instance ref too — no stale GUID (matches ClearPrimaryEvolution)
        return true;
    }
    return false;
}

bool ULoadoutComponent::ClearPrimaryEvolution()
{
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return false;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    if (Loadout.PrimarySlotType != EPrimarySlotType::Evolution)
    {
        return false;
    }

    // Unconditional empty (player-initiated removal) — slot type unchanged, like the broken sweep.
    Loadout.PrimaryEvolution = FEvolutionAttachment();
    Loadout.PrimaryEvolutionInstance = FGuid(); // clear the instance ref too — no stale GUID for an empty slot
    return true;
}

void ULoadoutComponent::SetActiveRingIndex(int32 NewIndex)
{
    // B-302: an adapter switch announces too, so the slot mirror's active adapter follows it.
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::SetActiveRingIndex] No InventoryComponent found on owner"));
        return;
    }
    if (!Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return;
    }

    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];

    if (!Loadout.RingLoadout.IsValidIndex(NewIndex))
    {
        return;
    }

    const TArray<FGatheredEffect> OldGearEffects = GetActiveEffectsGathered(GetOwner());
    Loadout.ActiveRingIndex = NewIndex;

    UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] Set ActiveRingIndex to %d"), NewIndex);

    // Notify TurnManager so cached BonusTurnSpeed re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (UWorld *World = GetWorld())
    {
        if (UGameInstance *GI = World->GetGameInstance())
        {
            if (UTurnManager *TurnManager = GI->GetSubsystem<UTurnManager>())
            {
                TurnManager->OnActorSpeedChanged(GetOwner());
            }
        }
    }

    SwapGearEffects(OldGearEffects);

    // Active equipment changed → recompute the stat-derived pools (equipment
    // BonusMaxHP/BonusMaxEnergy; the weapon's and ring's pool stones, B-082). Does NOT
    // touch CurrentHP/EP (overcap-not-clamp) — see RecomputeMaxPools doc.
    if (UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr)
    {
        CharComp->RecomputeMaxPools();
    }
}

void ULoadoutComponent::SwapGearEffects(const TArray<FGatheredEffect> &OldGearEffects) const
{
    AActor *Owner = GetOwner();
    UWorld *World = GetWorld();
    UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
    USkillEffectManager *Effects = GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
    // Out of a fight, battle prep gathers the gear when the next one starts.
    if (!Owner || !Effects || !Effects->IsGearArmed(Owner))
    {
        return;
    }
    Effects->ArmConditionalEffects(Owner, GetActiveConditionalEffectsGathered(Owner));
    Effects->SwapEquipmentEffects(Owner, OldGearEffects, GetActiveEffectsGathered(Owner));
}

ESpellElement ULoadoutComponent::GetActiveRingElement() const
{
    const FRingLoadoutEntry *Entry = GetActiveRingLoadout();
    return Entry ? Entry->RingEntry.AttachedItem.GetElement() : ESpellElement::None;
}

bool ULoadoutComponent::SwitchToNextUsableRing()
{
    const FCombatLoadout &Loadout = GetActiveLoadout();
    const int32 RingCount = Loadout.RingLoadout.Num();
    if (RingCount == 0)
    {
        return false;
    }

    const int32 StartIndex = Loadout.ActiveRingIndex;
    for (int32 Step = 1; Step <= RingCount; ++Step)
    {
        const int32 CheckIndex = (StartIndex + Step) % RingCount;
        const FRuntimeAttachedItem &Attachment = Loadout.RingLoadout[CheckIndex].RingEntry.AttachedItem;
        if (!Attachment.IsEmpty() && !Attachment.IsBroken())
        {
            SetActiveRingIndex(CheckIndex);
            return true;
        }
    }
    return false;
}

void ULoadoutComponent::HandleCrystalBroken(UObject *Holder, const FBrokenCrystalPayload &Payload)
{
    // Weapon crystals don't switch rings.
    if (!Cast<URingData>(Holder))
    {
        return;
    }

    FString CrystalName;
    switch (Payload.Kind)
    {
    case EAttachedItemKind::Crystal:
        CrystalName = ItemIdentity::GetDisplayName(Payload.CrystalId);
        break;
    case EAttachedItemKind::Evolution:
        CrystalName = Payload.Item ? Payload.Item->GetFullItemName() : TEXT("(null)");
        break;
    case EAttachedItemKind::Fusion:
        CrystalName = FString::Printf(TEXT("FUSION %s+%s"),
                                      *ItemIdentity::GetDisplayName(Payload.FusionId.HalfA),
                                      *ItemIdentity::GetDisplayName(Payload.FusionId.HalfB));
        break;
    default:
        CrystalName = TEXT("(none)");
        break;
    }

    UE_LOG(LogTemp, Log, TEXT("[LoadoutComponent] Ring crystal '%s' broke on %s — auto-switching"),
           *CrystalName, *GetNameSafe(GetOwner()));
    SwitchToNextUsableRing();
}

void ULoadoutComponent::DebugLogLoadout()
{
    UE_LOG(LogTemp, Log, TEXT("=== LoadoutComponent Debug ==="));
    UE_LOG(LogTemp, Log, TEXT("CharacterClass: %s"), *UEnum::GetValueAsString(GetCharacterClass()));
    UE_LOG(LogTemp, Log, TEXT("ActiveRingElement: %s"), *UEnum::GetValueAsString(GetActiveRingElement()));
    UE_LOG(LogTemp, Log, TEXT("bIsReadyForBattle: %s"), bIsReadyForBattle ? TEXT("true") : TEXT("false"));

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ULoadoutComponent::DebugLogLoadout] No InventoryComponent found on owner"));
        UE_LOG(LogTemp, Log, TEXT("=============================="));
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("ActiveLoadoutIndex: %d"), Inv->ActiveLoadoutIndex);
    UE_LOG(LogTemp, Log, TEXT("SavedLoadouts: %d"), Inv->SavedLoadouts.Num());

    if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        const FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        UE_LOG(LogTemp, Log, TEXT("Active Loadout: %s"), *Loadout.LoadoutName);
        UE_LOG(LogTemp, Log, TEXT("  PrimarySlotType: %s"), *UEnum::GetValueAsString(Loadout.PrimarySlotType));
        UE_LOG(LogTemp, Log, TEXT("  bShowPrimary: %s"), Loadout.bShowPrimary ? TEXT("true") : TEXT("false"));
    }

    UE_LOG(LogTemp, Log, TEXT("=============================="));
}

void ULoadoutComponent::DebugClearAndReportValidation()
{
    auto Report = [](const FString &Line, const FColor &Color)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Line);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, Color, Line);
        }
    };

    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        Report(TEXT("[DebugClearAndReportValidation] No valid active loadout"), FColor::Red);
        return;
    }

    auto PrintSummary = [&](const TCHAR *Label)
    {
        const FCombatLoadout &L = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
        Report(FString::Printf(
                   TEXT("%s '%s': Primary=%s  Rings=%d  EvoSpells=%d  Innate=%d  Items=%d"),
                   Label, *L.LoadoutName,
                   *UEnum::GetValueAsString(L.PrimarySlotType),
                   L.RingLoadout.Num(), L.EvolutionSpells.Num(),
                   L.InnateSpells.Num(), L.ItemSlots.Num()),
               FColor::Cyan);
    };

    Report(TEXT("=== DebugClearAndReportValidation ==="), FColor::White);
    PrintSummary(TEXT("PRE "));

    const TArray<FInvalidSlotFinding> Before = CollectInvalidSlotFindings();
    Report(FString::Printf(TEXT("Findings: %d"), Before.Num()), FColor::Yellow);
    for (const FInvalidSlotFinding &F : Before)
    {
        Report(FString::Printf(TEXT("  %s[%d] (clearable=%s) - %s"),
                               *UEnum::GetValueAsString(F.SlotType), F.SlotIndex,
                               F.bClearable ? TEXT("true") : TEXT("false"),
                               *F.Reason),
               F.bClearable ? FColor::Orange : FColor::Silver);
    }

    ClearInvalidSlots();

    const TArray<FInvalidSlotFinding> After = CollectInvalidSlotFindings();
    int32 ClearableAfter = 0;
    for (const FInvalidSlotFinding &F : After)
    {
        if (F.bClearable)
        {
            ++ClearableAfter;
        }
    }
    Report(FString::Printf(TEXT("Post-clear: %d clearable findings remain (expect 0)"), ClearableAfter),
           ClearableAfter == 0 ? FColor::Green : FColor::Red);

    PrintSummary(TEXT("POST"));
    Report(TEXT("====================================="), FColor::White);
}

bool ULoadoutComponent::ShouldUseWeaponParry() const
{
    // Read the per-weapon loadout-entry flag for the active weapon.
    if (const FWeaponLoadoutEntry *WeaponEntry = GetActiveWeaponLoadout())
    {
        return WeaponEntry->bUseWeaponParryAnimation;
    }
    return false;
}

UAnimMontage *ULoadoutComponent::GetDodgeLeftMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->DodgeLeftMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetDodgeRightMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->DodgeRightMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetBlockMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->BlockMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetParryMontage() const
{
    // Weapon-entry preference for the weapon's own parry animation.
    if (ShouldUseWeaponParry())
    {
        if (UWeaponData *Weapon = GetActiveWeapon())
        {
            if (Weapon->ParryMontage)
            {
                return Weapon->ParryMontage;
            }
        }
    }

    // Fall back to the character's parry montage from the Cosmetics asset.
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->ParryMontage : nullptr;
}

UStanceData *ULoadoutComponent::GetUnarmedStance() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->UnarmedStance : nullptr;
}

UInfusionDisplayData *ULoadoutComponent::GetInfusionDisplay() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->InfusionDisplay : nullptr;
}

UAnimMontage *ULoadoutComponent::GetRingSwitchMontage() const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    return CharData->Cosmetics ? CharData->Cosmetics->RingSwitchMontage : nullptr;
}

UAnimMontage *ULoadoutComponent::GetItemUseAnimation(bool bIsSelfTarget) const
{
    UCharacterData *CharData = GetOwnerCharacterData();
    if (!CharData)
    {
        return nullptr;
    }
    if (!CharData->Cosmetics)
    {
        return nullptr;
    }
    return bIsSelfTarget ? CharData->Cosmetics->ItemUseSelfMontage : CharData->Cosmetics->ItemUseTargetMontage;
}

UCharacterData *ULoadoutComponent::GetOwnerCharacterData() const
{
    if (!GetOwner())
    {
        return nullptr;
    }

    if (UCharacterDataComponent *CharComp = GetOwner()->FindComponentByClass<UCharacterDataComponent>())
    {
        return CharComp->CharacterData;
    }
    return nullptr;
}

UInventoryComponent *ULoadoutComponent::GetInventoryComponent() const
{
    if (!GetOwner())
    {
        return nullptr;
    }
    return GetOwner()->FindComponentByClass<UInventoryComponent>();
}

// ==================== EQUIPMENT STAT QUERIES ====================

FEquipmentStatBonus ULoadoutComponent::GetActiveStatBonus(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    FEquipmentStatBonus Combined;

    // TierPowerScaling §3 (cluster 3c): tier now scales the per-point VALUE, paired with 3a's flat
    // ~20 budget. Each equipped item's 12 substat contributions are multiplied by
    // GetTierPowerMultiplier(itemTier) BEFORE this (tierless) sum erases per-item tier. BOTH signs
    // scale by magnitude — a −8 roll on an S item becomes ~−38 (×4.8) — so cursed gear scales harder
    // at higher tier (that behaviour moved HERE from the old tier-scaled budget). EXCLUDED from tier
    // scaling: BonusMaxHP/BonusMaxEnergy (pools — already got the 3.0/3.5 gear rate in 3b; scaling
    // here would double-dip) and the pillar-percent fields (designer-tuned, not tier substat
    // capacity). Substats accumulate as FLOAT and round ONCE at the end so multi-item rounding never
    // compounds; consumers read the same int fields, now tier-weighted. F-tier (×1.0) stays
    // byte-identical.
    float AccRawDamage = 0.0f, AccSpellDamage = 0.0f, AccEfficiency = 0.0f, AccStatusMultiplier = 0.0f,
          AccCritDamage = 0.0f, AccSpellSpeed = 0.0f, AccDefense = 0.0f, AccActionSpeed = 0.0f,
          AccReflex = 0.0f, AccResistance = 0.0f, AccTurnSpeed = 0.0f, AccLuck = 0.0f;

    auto AddScaled = [&](const FEquipmentStatBonus &Src, EItemTier Tier)
    {
        const float F = TierPowerScaling::GetTierPowerMultiplier(Tier);
        AccRawDamage        += Src.BonusRawDamage        * F;
        AccSpellDamage      += Src.BonusSpellDamage      * F;
        AccEfficiency       += Src.BonusEfficiency       * F;
        AccStatusMultiplier += Src.BonusStatusMultiplier * F;
        AccCritDamage       += Src.BonusCritDamage       * F;
        AccSpellSpeed       += Src.BonusSpellSpeed       * F;
        AccDefense          += Src.BonusDefense          * F;
        AccActionSpeed      += Src.BonusActionSpeed      * F;
        AccReflex           += Src.BonusReflex           * F;
        AccResistance       += Src.BonusResistance       * F;
        AccTurnSpeed        += Src.BonusTurnSpeed        * F;
        AccLuck             += Src.BonusLuck             * F;
        // Excluded from tier scaling — accumulate raw (pools get the 3b rate downstream; pillar
        // percents are designer-tuned).
        Combined.BonusMaxHP                 += Src.BonusMaxHP;
        Combined.BonusMaxEnergy             += Src.BonusMaxEnergy;
        Combined.BonusMindModifierPercent   += Src.BonusMindModifierPercent;
        Combined.BonusBodyModifierPercent   += Src.BonusBodyModifierPercent;
        Combined.BonusSpiritModifierPercent += Src.BonusSpiritModifierPercent;
    };

    auto WeaponTier = [](const FWeaponInventoryEntry &E) { return E.Weapon ? E.Tier : EItemTier::F_Tier; }; // instance Tier (was E.Weapon->Tier)
    auto RingTier   = [](const FRingInventoryEntry &E)   { return E.Ring   ? E.Tier : EItemTier::F_Tier; };  // instance Tier (was E.Ring->Tier)

    const FCombatLoadout Loadout = GetActiveLoadout();

    switch (GetCharacterClass())
    {
    case ECharacterClass::Generic:
    {
        const bool bHasSecondaryWeapon =
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid();

        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (bHasSecondaryWeapon)
            {
                // Dual weapon — bShowPrimary picks which one is active.
                if (const FWeaponLoadoutEntry *Active = GetActiveWeaponLoadout())
                {
                    AddScaled(Active->WeaponEntry.StatBonus, WeaponTier(Active->WeaponEntry));
                }
            }
            else if (Loadout.PrimaryWeapon.IsValid())
            {
                AddScaled(Loadout.PrimaryWeapon.WeaponEntry.StatBonus, WeaponTier(Loadout.PrimaryWeapon.WeaponEntry));
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            // Ring primary; weapon-secondary (if any) sums in.
            if (Loadout.PrimaryRing.IsValid())
            {
                AddScaled(Loadout.PrimaryRing.RingEntry.StatBonus, RingTier(Loadout.PrimaryRing.RingEntry));
            }
            if (bHasSecondaryWeapon)
            {
                AddScaled(Loadout.SecondaryWeapon.WeaponEntry.StatBonus, WeaponTier(Loadout.SecondaryWeapon.WeaponEntry));
            }
        }
        // Evolution primary slots contribute via the innate-evolution sum after
        // this switch (Base + Generated, pillar percents excluded).
        break;
    }

    case ECharacterClass::Resonator:
    {
        // Active ring (from RingLoadout) + primary weapon (if equipped).
        if (const FRingLoadoutEntry *ActiveRing = GetActiveRingLoadout())
        {
            AddScaled(ActiveRing->RingEntry.StatBonus, RingTier(ActiveRing->RingEntry));
        }
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
            Loadout.PrimaryWeapon.IsValid())
        {
            AddScaled(Loadout.PrimaryWeapon.WeaponEntry.StatBonus, WeaponTier(Loadout.PrimaryWeapon.WeaponEntry));
        }
        break;
    }

    case ECharacterClass::Caster:
    default:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
            Loadout.PrimaryWeapon.IsValid())
        {
            AddScaled(Loadout.PrimaryWeapon.WeaponEntry.StatBonus, WeaponTier(Loadout.PrimaryWeapon.WeaponEntry));
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring &&
                 Loadout.PrimaryRing.IsValid())
        {
            AddScaled(Loadout.PrimaryRing.RingEntry.StatBonus, RingTier(Loadout.PrimaryRing.RingEntry));
        }
        break;
    }
    }

    // Innate (primary-slot) evolution — its flat substats are slot-level stats,
    // summed like a weapon/ring entry's (asset Base + per-instance Generated roll).
    // ATTACHED-to-gear evolutions stay infusion-only — deliberately not summed here.
    // Mutually exclusive with the weapon/ring primary branches on PrimarySlotType
    // (same two-branch precedent as ApplyEvolutionPillarModifier). Base + Generated
    // are the SAME item, so both ride the evolution item's tier.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
    {
        FEquipmentStatBonus EvoBonus = Loadout.PrimaryEvolution.Item->BaseStatBonus;
        EvoBonus.Accumulate(Loadout.PrimaryEvolution.GeneratedStatBonus);
        // Pillar percents deliberately NOT summed: the evolution's pillar % already
        // applies as ApplyEvolutionPillarModifier's CRYSTAL layer; leaving them in
        // would double-apply through that function's EQUIPMENT layer, which reads
        // this return value. (Zeroed before AddScaled, which would otherwise pass them through.)
        EvoBonus.BonusMindModifierPercent = 0.0f;
        EvoBonus.BonusBodyModifierPercent = 0.0f;
        EvoBonus.BonusSpiritModifierPercent = 0.0f;
        AddScaled(EvoBonus, Loadout.PrimaryEvolution.Tier);
    }

    // Round the tier-scaled substat accumulators ONCE into Combined (BonusCritDamage is a float
    // field — store directly). Pools + pillar percents were accumulated raw inside AddScaled.
    Combined.BonusRawDamage        = FMath::RoundToInt(AccRawDamage);
    Combined.BonusSpellDamage      = FMath::RoundToInt(AccSpellDamage);
    Combined.BonusEfficiency       = FMath::RoundToInt(AccEfficiency);
    Combined.BonusStatusMultiplier = FMath::RoundToInt(AccStatusMultiplier);
    Combined.BonusCritDamage       = AccCritDamage;
    Combined.BonusSpellSpeed       = FMath::RoundToInt(AccSpellSpeed);
    Combined.BonusDefense          = FMath::RoundToInt(AccDefense);
    Combined.BonusActionSpeed      = FMath::RoundToInt(AccActionSpeed);
    Combined.BonusReflex           = FMath::RoundToInt(AccReflex);
    Combined.BonusResistance       = FMath::RoundToInt(AccResistance);
    Combined.BonusTurnSpeed        = FMath::RoundToInt(AccTurnSpeed);
    Combined.BonusLuck             = FMath::RoundToInt(AccLuck);

    return Combined;
}

FResistanceBonus ULoadoutComponent::GetActiveResistanceBonus(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    FResistanceBonus Combined;

    const FCombatLoadout Loadout = GetActiveLoadout();

    // Mirrors GetActiveStatBonus's per-class slot resolution exactly, summing
    // each contributing entry's ResistanceBonus instead of StatBonus.
    switch (GetCharacterClass())
    {
    case ECharacterClass::Generic:
    {
        const bool bHasSecondaryWeapon =
            Loadout.SecondarySlotType == ESecondarySlotType::Weapon &&
            Loadout.SecondaryWeapon.IsValid();

        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (bHasSecondaryWeapon)
            {
                if (const FWeaponLoadoutEntry *Active = GetActiveWeaponLoadout())
                {
                    Combined = Active->WeaponEntry.ResistanceBonus;
                }
            }
            else if (Loadout.PrimaryWeapon.IsValid())
            {
                Combined = Loadout.PrimaryWeapon.WeaponEntry.ResistanceBonus;
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            if (Loadout.PrimaryRing.IsValid())
            {
                Combined.Accumulate(Loadout.PrimaryRing.RingEntry.ResistanceBonus);
            }
            if (bHasSecondaryWeapon)
            {
                Combined.Accumulate(Loadout.SecondaryWeapon.WeaponEntry.ResistanceBonus);
            }
        }
        break;
    }

    case ECharacterClass::Resonator:
    {
        if (const FRingLoadoutEntry *ActiveRing = GetActiveRingLoadout())
        {
            Combined.Accumulate(ActiveRing->RingEntry.ResistanceBonus);
        }
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
            Loadout.PrimaryWeapon.IsValid())
        {
            Combined.Accumulate(Loadout.PrimaryWeapon.WeaponEntry.ResistanceBonus);
        }
        break;
    }

    case ECharacterClass::Caster:
    default:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon &&
            Loadout.PrimaryWeapon.IsValid())
        {
            Combined = Loadout.PrimaryWeapon.WeaponEntry.ResistanceBonus;
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring &&
                 Loadout.PrimaryRing.IsValid())
        {
            Combined = Loadout.PrimaryRing.RingEntry.ResistanceBonus;
        }
        break;
    }
    }

    // Evolution resistance — a PERMANENT equipped bonus (always-on while slotted):
    // authored Base from the ASSET + rolled Generated from the slotted ATTACHMENT
    // (the per-instance roll; the asset's GeneratedResistance is preview-only as
    // of U3c). Two mutually-exclusive cases mirroring the pillar read:
    //   A: evolution attached to the active weapon (PrimarySlotType==Weapon)
    //   B: evolution as the standalone primary slot (PrimarySlotType==Evolution) —
    //      where the U1c instance bridge lands the roll. (B was previously not
    //      aggregated at all — coverage aligned with the pillar read here.)
    const FEvolutionAttachment WeaponEvo = GetActivePrimaryEvolutionAttachment(Actor);
    if (WeaponEvo.Item)
    {
        FResistanceBonus EvoResist = WeaponEvo.Item->BaseResistance;
        EvoResist.Accumulate(WeaponEvo.GeneratedResistance);
        Combined.Accumulate(EvoResist);
    }
    else if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
    {
        FResistanceBonus EvoResist = Loadout.PrimaryEvolution.Item->BaseResistance;
        EvoResist.Accumulate(Loadout.PrimaryEvolution.GeneratedResistance);
        Combined.Accumulate(EvoResist);
    }

    return Combined;
}

TArray<FSkillEffect> ULoadoutComponent::GetActiveEffects(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    // STARTING effects only (no condition — FSkillEffect::IsConditionalEffect's
    // complement), applied once at combat start. Coverage mirrors
    // GetActiveStatBonus's per-class gear selection — if gear's stats count for
    // you, its starting effects apply — EXCEPT evolutions ATTACHED to a
    // weapon/ring contribute NO effects; only the innate (primary-slot)
    // evolution does (the deliberate exception).
    TArray<FSkillEffect> Result;

    const FCombatLoadout Loadout = GetActiveLoadout();

    switch (GetCharacterClass())
    {
    case ECharacterClass::Generic:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetActiveWeapon())
            {
                Result.Append(Weapon->GetStartingEffects());
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            // Ring primary; weapon-secondary sums in (GetActiveWeapon resolves
            // to the secondary weapon here) — matching the stat sum.
            if (URingData *Ring = GetPrimaryRing())
            {
                Result.Append(Ring->GetStartingEffects());
            }
            if (UWeaponData *Weapon = GetActiveWeapon())
            {
                Result.Append(Weapon->GetStartingEffects());
            }
        }
        break;
    }

    case ECharacterClass::Resonator:
    {
        // Active ring + primary weapon (if equipped) — matching the stat sum.
        if (URingData *Ring = GetActiveRing())
        {
            Result.Append(Ring->GetStartingEffects());
        }
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetPrimaryWeapon())
            {
                Result.Append(Weapon->GetStartingEffects());
            }
        }
        break;
    }

    case ECharacterClass::Caster:
    default:
    {
        // Primary slot only (weapon OR ring, not both).
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetPrimaryWeapon())
            {
                Result.Append(Weapon->GetStartingEffects());
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            if (URingData *Ring = GetPrimaryRing())
            {
                Result.Append(Ring->GetStartingEffects());
            }
        }
        break;
    }
    }

    // Innate (primary-slot) evolution starting effects. ATTACHED evolutions are
    // deliberately excluded — same two-branch exclusivity on PrimarySlotType as
    // ApplyEvolutionPillarModifier and the stat sum above.
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
    {
        Result.Append(Loadout.PrimaryEvolution.Item->GetStartingEffects());
    }

    return Result;
}

TArray<FGatheredEffect> ULoadoutComponent::GetActiveEffectsGathered(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    // Def-identity mirror of GetActiveEffects: identical per-class gear coverage, but
    // calls the ...Gathered() accessors so each effect carries its def id + bundle index.
    TArray<FGatheredEffect> Result;

    const FCombatLoadout Loadout = GetActiveLoadout();
    // B-192: bound gear adds its bonus only on its owner.
    const UCharacterData *Wearer = GetOwnerCharacterData();

    switch (GetCharacterClass())
    {
    case ECharacterClass::Generic:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetActiveWeapon())
            {
                Result.Append(Weapon->GetStartingEffectsGathered(Wearer));
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            if (URingData *Ring = GetPrimaryRing())
            {
                Result.Append(Ring->GetStartingEffectsGathered(Wearer));
            }
            if (UWeaponData *Weapon = GetActiveWeapon())
            {
                Result.Append(Weapon->GetStartingEffectsGathered(Wearer));
            }
        }
        break;
    }

    case ECharacterClass::Resonator:
    {
        if (URingData *Ring = GetActiveRing())
        {
            Result.Append(Ring->GetStartingEffectsGathered(Wearer));
        }
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetPrimaryWeapon())
            {
                Result.Append(Weapon->GetStartingEffectsGathered(Wearer));
            }
        }
        break;
    }

    case ECharacterClass::Caster:
    default:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetPrimaryWeapon())
            {
                Result.Append(Weapon->GetStartingEffectsGathered(Wearer));
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            if (URingData *Ring = GetPrimaryRing())
            {
                Result.Append(Ring->GetStartingEffectsGathered(Wearer));
            }
        }
        break;
    }
    }

    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
    {
        Result.Append(Loadout.PrimaryEvolution.Item->GetStartingEffectsGathered());
    }

    return Result;
}

TArray<FGatheredEffect> ULoadoutComponent::GetActiveConditionalEffectsGathered(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    // Identical per-class gear coverage to GetActiveEffectsGathered, but the CONDITIONAL
    // subset (effects gated by a trigger, fired on impact rather than at combat start).
    TArray<FGatheredEffect> Result;

    const FCombatLoadout Loadout = GetActiveLoadout();
    // B-192: bound gear adds its bonus only on its owner.
    const UCharacterData *Wearer = GetOwnerCharacterData();

    switch (GetCharacterClass())
    {
    case ECharacterClass::Generic:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetActiveWeapon())
            {
                Result.Append(Weapon->GetConditionalEffectsGathered(Wearer));
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            if (URingData *Ring = GetPrimaryRing())
            {
                Result.Append(Ring->GetConditionalEffectsGathered(Wearer));
            }
            if (UWeaponData *Weapon = GetActiveWeapon())
            {
                Result.Append(Weapon->GetConditionalEffectsGathered(Wearer));
            }
        }
        break;
    }

    case ECharacterClass::Resonator:
    {
        if (URingData *Ring = GetActiveRing())
        {
            Result.Append(Ring->GetConditionalEffectsGathered(Wearer));
        }
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetPrimaryWeapon())
            {
                Result.Append(Weapon->GetConditionalEffectsGathered(Wearer));
            }
        }
        break;
    }

    case ECharacterClass::Caster:
    default:
    {
        if (Loadout.PrimarySlotType == EPrimarySlotType::Weapon)
        {
            if (UWeaponData *Weapon = GetPrimaryWeapon())
            {
                Result.Append(Weapon->GetConditionalEffectsGathered(Wearer));
            }
        }
        else if (Loadout.PrimarySlotType == EPrimarySlotType::Ring)
        {
            if (URingData *Ring = GetPrimaryRing())
            {
                Result.Append(Ring->GetConditionalEffectsGathered(Wearer));
            }
        }
        break;
    }
    }

    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution && Loadout.PrimaryEvolution.Item)
    {
        Result.Append(Loadout.PrimaryEvolution.Item->GetConditionalEffectsGathered());
    }

    return Result;
}

UEvolutionItemData *ULoadoutComponent::GetActivePrimaryEvolutionCrystal(AActor *Actor) const
{
    (void)Actor;

    const FCombatLoadout Loadout = GetActiveLoadout();
    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon)
    {
        return nullptr;
    }
    if (!Loadout.PrimaryWeapon.IsValid())
    {
        return nullptr;
    }

    const FRuntimeAttachedItem &Attachment = Loadout.PrimaryWeapon.WeaponEntry.AttachedItem;
    return Attachment.IsEvolution() ? Attachment.Evolution.Item : nullptr;
}

FEvolutionAttachment ULoadoutComponent::GetActivePrimaryEvolutionAttachment(AActor *Actor) const
{
    (void)Actor;

    // Same resolution as GetActivePrimaryEvolutionCrystal, returning the whole
    // attachment (by value — GetActiveLoadout copies, so no stable pointer exists).
    const FCombatLoadout Loadout = GetActiveLoadout();
    if (Loadout.PrimarySlotType != EPrimarySlotType::Weapon || !Loadout.PrimaryWeapon.IsValid())
    {
        return FEvolutionAttachment();
    }
    const FRuntimeAttachedItem &Attachment = Loadout.PrimaryWeapon.WeaponEntry.AttachedItem;
    return Attachment.IsEvolution() ? Attachment.Evolution : FEvolutionAttachment();
}

FGuid ULoadoutComponent::GetActivePrimaryEvolutionInstance() const
{
    // The owned-entry identity retained on the runtime loadout (iii-b). Invalid when the primary
    // slot is not an instance-resolved evolution (inflation only sets it in the Evolution case).
    return GetActiveLoadout().PrimaryEvolutionInstance;
}

#undef LOCTEXT_NAMESPACE
