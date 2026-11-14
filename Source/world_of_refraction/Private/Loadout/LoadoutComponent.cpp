// LoadoutComponent.cpp
// Active combat loadout and runtime battle state implementation

#include "Loadout/LoadoutComponent.h"
#include "Loadout/WeaponSpellRules.h"
#include "Equipment/Apparel/ApparelData.h" // B-380: the apparel loops read UApparelData
#include "Memory/CurseSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Algo/Count.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
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
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h" // B-392b: a guest's guard settings go to the host
#include "GameFramework/PlayerController.h"
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
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Engine/Engine.h"

#include "Skills/Definitions/SkillDataBase.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "LoadoutComponent"

namespace
{
    /** B-357b: fight-start reads see every evolution as not infused (charge level 0). */
    constexpr int32 LOADOUT_FIGHT_START_CHARGE_LEVEL = 0;
}

namespace
{
    /** Q-159 = A: utility crystals a loadout rule removes are not destroyed; they go back to the
     *  inventory, as many as fit. An authored loadout's crystals were never taken from the
     *  inventory, so this can add extra ones: a dev-time case Q-159 accepts while the inventory is
     *  the member's own (it refills at each spawn). B-313 / B-317: a member with a pouch took nothing
     *  from it into its preset's item slots, so nothing goes back (none are made up). */
    void LoadoutReturnDroppedCrystals(UInventoryComponent *Inv, const FCrystalId &Id, int32 Count)
    {
        const UCrystalInventoryComponent *CrystalInv = Inv->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>();
        if (!CrystalInv)
        {
            // An enemy's authored loadout: no crystal inventory to return to.
            UE_LOG(LogWoRLoadout, Verbose, TEXT("No crystal inventory: %d dropped %s not returned"), Count, *ItemIdentity::GetDisplayName(Id));
            return;
        }
        if (CrystalInv->IsSharingItems() || Count <= 0)
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

    /** B-313: the pool and key of the owner's pouch; null when the owner carries none (enemies, unbound tests). */
    UPoolSubsystem *LoadoutFindPouch(const AActor *Owner, FName &OutKey)
    {
        const UCrystalInventoryComponent *CrystalInv = Owner ? Owner->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
        UPoolSubsystem *Pool = CrystalInv ? CrystalInv->GetPouchPool() : nullptr;
        OutKey = Pool ? CrystalInv->GetPouchCharacterKey() : FName(NAME_None);
        return Pool;
    }

    /** B-138: announces an active-weapon change when the mutator it guards returns, on every path. */
    struct FActiveWeaponChangeScope
    {
        explicit FActiveWeaponChangeScope(ULoadoutComponent &InLoadout) : Loadout(InLoadout) {}
        ~FActiveWeaponChangeScope() { Loadout.NotifyActiveWeaponChanged(); }

        ULoadoutComponent &Loadout;
    };

    /** B-311 (D-111): room for another custom loadout (lore loadouts don't count). */
    bool HasRoomForCustomLoadout(const UInventoryComponent &Inv)
    {
        const int32 Custom = Algo::CountIf(Inv.SavedLoadouts, [](const FCombatLoadout &Loadout) { return !Loadout.bLore; });
        if (Custom >= PoolConstants::MAX_CUSTOM_LOADOUTS)
        {
            UE_LOG(LogTemp, Log, TEXT("[Loadout] %s already has %d custom loadouts (D-111)"), *GetNameSafe(Inv.GetOwner()), Custom);
            return false;
        }
        return true;
    }
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
    const UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(GI);
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
        DefaultLoadout.bLore = true; // B-311: a seed, not the player's own
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
    return CharData ? CharData->CharacterClass : ECharacterClass::Generic;
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
            if (UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(GI))
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
    if (!HasRoomForCustomLoadout(*Inv)) // D-111: 5 custom loadouts plus the lore ones (MaxSavedLoadouts no longer caps these)
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

    UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Created loadout '%s' at index %d"), *LoadoutName, NewIndex);

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

    // B-313 (D-111): a member with a pouch fights with the pouch; a preset holds no items of its own, so the
    // pick keeps only the identity (Quantity 0) and nothing leaves the pouch.
    const bool bPouchCarries = CrystalInv && CrystalInv->IsSharingItems();
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
        const int32 Debited = bPouchCarries ? 0 : Inventory->RemoveCrystal(Entry.CrystalId, Entry.Quantity);
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

    UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Loadout '%s' configured: %d spells, %d abilities, %d items"),
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

    // Don't delete last loadout; a lore loadout is read-only (D-111)
    if (Inv->SavedLoadouts.Num() <= 1 || Inv->SavedLoadouts[Index].bLore)
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

    if (!HasRoomForCustomLoadout(*Inv)) // D-111: 5 custom loadouts plus the lore ones (MaxSavedLoadouts no longer caps these)
    {
        return -1;
    }

    // B-311: a copy is the player's own, even of a lore loadout (the way to keep changes to one).
    FCombatLoadout NewLoadout = Inv->SavedLoadouts[SourceIndex];
    NewLoadout.LoadoutName = NewName;
    NewLoadout.bLore = false;

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

    // D-116 (Q-320): flagged, not blocking - a member owning nothing usable has no primary to give.
    if (!UInventoryComponent::LoadoutHasPrimary(Inv->SavedLoadouts[Index]))
    {
        UE_LOG(LogWoRLoadout, Warning, TEXT("%s: loadout %d has no primary (D-116)"), *GetNameSafe(GetOwner()), Index);
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
    Context.bItemsInPouch = UsesPouchItems(); // B-313b: a pouch member's item slots are inert; the rules skip them

    if (Inventory)
    {
        // B-312b: owned means in the store's view of this member (OwnsWeaponAsset / OwnsRingAsset), not in its copy.
        Context.OwnsWeapon = [Inventory](const UWeaponData *Weapon)
        {
            return Inventory->OwnsWeaponAsset(Weapon);
        };
        Context.OwnsRing = [Inventory](const URingData *Ring)
        {
            return Inventory->OwnsRingAsset(Ring);
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
        case ELoadoutSlotType::WeaponSpell:
        {
            // B-304: SubIndex SECONDARY_WEAPON names a Generic's secondary weapon; anything else the primary.
            FWeaponLoadoutEntry &Weapon = Finding.SubIndex == LoadoutAbilityOverflowIndex::SECONDARY_WEAPON ? Loadout.SecondaryWeapon
                                                                                                           : Loadout.PrimaryWeapon;
            if (Finding.SlotType == ELoadoutSlotType::WeaponAbility && Weapon.AssignedAbilities.IsValidIndex(Finding.SlotIndex))
            {
                Weapon.AssignedAbilities[Finding.SlotIndex] = nullptr;
            }
            else if (Finding.SlotType == ELoadoutSlotType::WeaponSpell && Weapon.AssignedSpells.IsValidIndex(Finding.SlotIndex))
            {
                Weapon.AssignedSpells[Finding.SlotIndex] = nullptr;
            }
            break;
        }
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
                    LoadoutReturnDroppedCrystals(Inv, Slot.CrystalId, Slot.Quantity);
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
            TArray<UAbilityData *> &Abilities = Weapon.AssignedAbilities;
            const int32 Cap = LoadoutConstants::MAX_WEAPON_ABILITIES;
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
                LoadoutReturnDroppedCrystals(Inv, Slot.CrystalId, Slot.Quantity - InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT);
                Slot.Quantity = FMath::Clamp(Slot.Quantity, 0, InventoryConstants::MAX_QUANTITY_PER_ITEM_SLOT);
            }
            break;
        case ELoadoutSlotType::UtilityCrystalTierCap: // OBSOLETE (B-354): no longer reported
        case ELoadoutSlotType::EquippedSTierCrystal:
        {
            const TArray<FItemLoadoutSlot> Untrimmed = Loadout.ItemSlots;
            // B-354 (D-143): only the item slots' S gems can be cleared (the attachments keep their S crystal).
            if (Finding.SlotType == ELoadoutSlotType::EquippedSTierCrystal)
            {
                FLoadoutRules::TrimSTierItemCrystals(Loadout, GetCharacterClass());
            }
            else
            {
                FLoadoutRules::TrimUtilityCrystalsToTierCaps(Loadout);
            }
            TArray<FString> Removed;
            for (int32 i = 0; i < Untrimmed.Num(); ++i)
            {
                const int32 Lost = Untrimmed[i].Quantity - Loadout.ItemSlots[i].Quantity;
                if (Lost > 0)
                {
                    LoadoutReturnDroppedCrystals(Inv, Untrimmed[i].CrystalId, Lost);
                    Removed.Add(FString::Printf(TEXT("%s %s x%d"), *ItemIdentity::GetTierLetter(Untrimmed[i].CrystalId.Tier),
                                                *ItemIdentity::GetTypeName(Untrimmed[i].CrystalId.Type), Lost));
                }
            }
            if (Removed.IsEmpty())
            {
                continue; // an earlier finding's trim already removed them (one trim covers every band, and the S rule)
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
    if (!UInventoryComponent::LoadoutHasPrimary(Loadout))
    {
        Inv->RefillActivePrimary(); // D-116: a dropped primary is replaced, not left empty
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
        UE_LOG(LogWoRLoadout, Verbose, TEXT("Pick refused on loadout %d: %s"), LoadoutIndex, *OutRefusal.ToString());
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
            UE_LOG(LogWoRLoadout, Verbose, TEXT("Pick refused on loadout %d: %s"), LoadoutIndex, *Reason);
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
    if (!WeaponSpellRules::WeaponsGiveSpells())
    {
        OutRefusal = LOCTEXT("WeaponsGiveNoSpells", "Weapons don't hold spells. Put it on an adapter or evolution."); // B-098 (D-027)
        return false;
    }
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

bool ULoadoutComponent::TryChangeGear(int32 LoadoutIndex, TFunctionRef<void(FCombatLoadout &)> Change, FText &OutRefusal)
{
    const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(GI);
    if (TurnManager && TurnManager->IsCombatActive())
    {
        OutRefusal = LOCTEXT("GearInFight", "Gear can't be changed during a fight.");
        return false;
    }
    const FActiveWeaponChangeScope ActiveWeaponScope(*this);
    const TArray<FGatheredEffect> OldGearEffects = GetActiveEffectsGathered(GetOwner());
    if (!TryChangeLoadout(LoadoutIndex, true, [](const FCombatLoadout &, ECharacterClass) { return FText::GetEmpty(); }, Change, OutRefusal))
    {
        return false;
    }
    GetInventoryComponent()->SavedLoadouts[LoadoutIndex].RebuildSlotsFromLegacy(GetCharacterClass()); // TryChangeLoadout checked both
    SwapGearEffects(OldGearEffects);
    if (UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr)
    {
        CharComp->RecomputeMaxPools();
    }
    UE_LOG(LogWoRLoadout, Log, TEXT("%s: gear changed on loadout %d"), *GetNameSafe(GetOwner()), LoadoutIndex);
    return true;
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

    BattleBandUses.Reset();
    if (UsesPouchItems())
    {
        // B-313 (D-111, Q-155): a member's fight items are its pouch as it is; refills happen at rest spots.
        // The preset's item slots are neither filled nor read (OBSOLETE (B-313)).
        UE_LOG(LogWoRLoadout, Verbose, TEXT("%s fights with its pouch: %d usable item stack(s)"), *GetNameSafe(GetOwner()),
               GetUsableItems().Num());
    }
    else if (Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        // Only an actor with no pouch (an enemy, an unbound test actor) still auto-equips its item slots: the
        // enemies' own item list (Q-313b) is an editor move, so this stays until it lands (B-317c).
        // Auto-equip refills item slots from the crystal inventory at combat
        // start when the loadout's flag is set. Pass the LIVE loadout element
        // (not a by-value accessor) so the Quantity writes persist.
        FCombatLoadout::ApplyAutoEquip(Inv->SavedLoadouts[Inv->ActiveLoadoutIndex], GetOwner());
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

bool ULoadoutComponent::UsesPouchItems() const
{
    FName Key;
    return LoadoutFindPouch(GetOwner(), Key) != nullptr;
}

// OBSOLETE (B-354, D-143): Q-313's per-fight tier bands retired, so this only tells a gem (whole stack usable) from a
// stone or None (slot-only, 0). Fold that check into CanUseCarriedItem / GetUsableItems and delete it with
// BattleBandUses after Crown's PIE.
int32 ULoadoutComponent::GetCarriedBandRoom(const FCrystalId &Id) const
{
    return FLoadoutRules::GetUtilityBandIndex(Id) != INDEX_NONE ? MAX_int32 : 0;
}

bool ULoadoutComponent::CanUseCarriedItem(const FCrystalId &Id) const
{
    FName Key;
    const UPoolSubsystem *Pool = LoadoutFindPouch(GetOwner(), Key);
    return Pool && Pool->GetCarriedCount(Key, Id) > 0 && GetCarriedBandRoom(Id) > 0;
}

bool ULoadoutComponent::UseCarriedItem(const FCrystalId &Id)
{
    if (!CanUseCarriedItem(Id))
    {
        return false;
    }
    UInventoryComponent *Inv = GetInventoryComponent();
    // The facade takes from the bound pouch only (UCrystalInventoryComponent::RemoveCount -> TakeCarried).
    if (!Inv || Inv->RemoveCrystal(Id, 1) <= 0)
    {
        return false;
    }

    FName Key;
    const UPoolSubsystem *Pool = LoadoutFindPouch(GetOwner(), Key);
    const int32 Left = Pool ? Pool->GetCarriedCount(Key, Id) : 0;
    UE_LOG(LogWoRLoadout, Verbose, TEXT("%s used %s from its pouch: %d left"), *GetNameSafe(GetOwner()),
           *ItemIdentity::GetDisplayName(Id), Left);
    OnItemUsed.Broadcast(INDEX_NONE, Id, Left);
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
    FName PouchKey;
    if (const UPoolSubsystem *Pool = LoadoutFindPouch(GetOwner(), PouchKey))
    {
        // B-313 (D-111, D-117): the member's pouch is its only item list.
        TArray<FItemLoadoutSlot> Carried;
        for (const TPair<FCrystalId, int32> &Stack : Pool->GetCarriedItems(PouchKey))
        {
            const int32 Usable = FMath::Min(Stack.Value, GetCarriedBandRoom(Stack.Key));
            if (Usable > 0)
            {
                FItemLoadoutSlot &Item = Carried.AddDefaulted_GetRef();
                Item.CrystalId = Stack.Key;
                Item.Quantity = Usable;
            }
        }
        // A stable order for menus that index this list (the pouch is a map).
        Carried.Sort([](const FItemLoadoutSlot &A, const FItemLoadoutSlot &B)
                     { return A.CrystalId.Type != B.CrystalId.Type ? A.CrystalId.Type < B.CrystalId.Type : A.CrystalId.Tier > B.CrystalId.Tier; });
        return Carried;
    }

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
    BattleBandUses.Reset();
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

    // New path: InventoryComponent has already populated SavedLoadouts +
    // ActiveLoadoutIndex from UInventoryData. LC just applies the
    // BD pool runtime flag and returns.
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
    // B-311: the seed replaces the lore loadouts only; custom ones drawn from the pool stay.
    Inv->SavedLoadouts.RemoveAll([](const FCombatLoadout &Loadout) { return Loadout.bLore; });
    FCombatLoadout EmptyLoadout;
    EmptyLoadout.LoadoutName = TEXT("Default");
    EmptyLoadout.InitializeForClass(CharacterData->CharacterClass);
    EmptyLoadout.bLore = true;
    Inv->SavedLoadouts.Insert(EmptyLoadout, 0);
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

void ULoadoutComponent::NotifyApparelChanged()
{
    if (UCharacterDataComponent *Data = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr)
    {
        Data->RecomputeMaxPools(); // gear HP
    }
    const UInventoryComponent *Inv = GetInventoryComponent();
    OnLoadoutChanged.Broadcast(Inv ? Inv->ActiveLoadoutIndex : 0); // GAS stats and resistances re-sync on this
}

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
    UE_LOG(LogWoRLoadout, Verbose, TEXT("%s: active weapon is now %s"), *GetNameSafe(GetOwner()),
           ActiveWeapon ? *ActiveWeapon->Name : TEXT("none"));
    OnActiveWeaponChanged.Broadcast(ActiveWeapon);
}

UWeaponData *ULoadoutComponent::GetActiveWeapon() const
{
    // B-304 (D-110): forwards to the one active-slot rule.
    const FLoadoutSlot *Slot = GetActiveSlot(EGearKind::Weapon);
    return Slot ? Slot->Gear.GetWeapon() : nullptr;
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
    // B-304: a Generic's or Refractor's adapter is its active adapter (a Resonator's lives in its adapter loadout).
    if (GetCharacterClass() == ECharacterClass::Resonator)
    {
        return nullptr;
    }
    const FLoadoutSlot *Slot = GetActiveSlot(EGearKind::Adapter);
    return Slot ? Slot->Gear.GetAdapter() : nullptr;
}

URingData *ULoadoutComponent::GetActiveRing() const
{
    // B-304: the Resonator's active adapter (others answer through GetPrimaryRing, as before).
    if (GetCharacterClass() != ECharacterClass::Resonator)
    {
        return nullptr;
    }
    const FLoadoutSlot *Slot = GetActiveSlot(EGearKind::Adapter);
    return Slot ? Slot->Gear.GetAdapter() : nullptr;
}

UEvolutionItemData *ULoadoutComponent::GetPrimaryEvolution() const
{
    return GetCharacterSlotEvolution().Item; // B-357e: through the one resolve path
}

TArray<FEquippedCrystalSlot> ULoadoutComponent::GetEquippedCrystals() const
{
    TArray<FEquippedCrystalSlot> Result;

    auto AddAttachment = [&Result](const FRuntimeAttachedItem &Attachment, UObject *Holder, const FGuid &GearID)
    {
        if (Attachment.IsEmpty() || !Holder)
        {
            return;
        }

        FEquippedCrystalSlot Slot;
        Slot.Holder = Holder;
        Slot.GearID = GearID;
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
                      PrimaryLoadout->WeaponEntry.Weapon, PrimaryLoadout->WeaponEntry.PersistentID);
    }
    if (const FWeaponLoadoutEntry *SecondaryLoadout = GetSecondaryWeaponLoadout())
    {
        AddAttachment(SecondaryLoadout->WeaponEntry.AttachedItem,
                      SecondaryLoadout->WeaponEntry.Weapon, SecondaryLoadout->WeaponEntry.PersistentID);
    }

    // Primary ring (Generic / Caster) — runtime entry
    if (const FRingLoadoutEntry *PrimaryRingLoadout = GetPrimaryRingLoadout())
    {
        AddAttachment(PrimaryRingLoadout->RingEntry.AttachedItem,
                      PrimaryRingLoadout->RingEntry.Ring, PrimaryRingLoadout->RingEntry.PersistentID);
    }

    // Resonator ring loadout — every ring in the array, runtime entry
    const FCombatLoadout &ActiveLoadout = GetActiveLoadout();
    for (const FRingLoadoutEntry &Entry : ActiveLoadout.RingLoadout)
    {
        AddAttachment(Entry.RingEntry.AttachedItem, Entry.RingEntry.Ring, Entry.RingEntry.PersistentID);
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

    UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Toggled equipment: bShowPrimary = %s"),
           Loadout.bShowPrimary ? TEXT("true") : TEXT("false"));

    // Notify TurnManager so cached BonusTurnSpeed re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (UWorld *World = GetWorld())
    {
        if (UGameInstance *GI = World->GetGameInstance())
        {
            if (UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(GI))
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

    // Only evolved weapons have resonate spells; none since D-027 (B-098, WeaponSpellRules)
    if (!WeaponSpellRules::WeaponsGiveSpells() || !WeaponEntry->WeaponEntry.IsEvolved())
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
    // B-304: the old entry behind the active weapon slot.
    // One in-step walk, then the slot (B-304 review: the forwards used to walk twice).
    const FCombatLoadout *Loadout = GetActiveLoadoutInStep();
    const FLoadoutSlot *Slot = Loadout ? GetActiveSlotIn(*Loadout, EGearKind::Weapon) : nullptr;
    return Slot ? Loadout->GetLegacyWeaponEntry(static_cast<int32>(Slot - Loadout->Slots.GetData())) : nullptr;
}

const FRuntimeAttachedItem *ULoadoutComponent::GetStoneRingAttachment() const
{
    const FRingLoadoutEntry *Ring = GetActiveRingLoadout();
    Ring = Ring ? Ring : GetPrimaryRingLoadout();
    return Ring ? &Ring->RingEntry.AttachedItem : nullptr;
}

const FRuntimeAttachedItem *ULoadoutComponent::GetActiveWeaponStoneSocket() const
{
    const FWeaponLoadoutEntry *ActiveWeapon = GetActiveWeaponLoadout();
    return ActiveWeapon ? &ActiveWeapon->WeaponEntry.StoneSocket : nullptr;
}

const FRuntimeAttachedItem *ULoadoutComponent::GetStoneRingStoneSocket() const
{
    const FRingLoadoutEntry *Ring = GetActiveRingLoadout();
    Ring = Ring ? Ring : GetPrimaryRingLoadout();
    return Ring ? &Ring->RingEntry.StoneSocket : nullptr;
}

float ULoadoutComponent::GetAttachedStonePercent(ESubStat Stat) const
{
    // B-351 (D-141): the stone sockets of the weapon and adapter in use, plus a pre-B-351 stone in an element socket.
    float Total = 0.0f;
    for (const FRuntimeAttachedItem *Attachment : {GetActiveWeaponAttachment(), GetStoneRingAttachment(),
                                                   GetActiveWeaponStoneSocket(), GetStoneRingStoneSocket()})
    {
        Total += Attachment ? CrystalEffectTable::GetAttachedStonePercent(*Attachment, Stat) : 0.0f;
    }
    return Total;
}

float ULoadoutComponent::GetAttachedStonePercentForType(ECrystalType StoneType) const
{
    float Total = 0.0f;
    for (const FRuntimeAttachedItem *Attachment : {GetActiveWeaponAttachment(), GetStoneRingAttachment(),
                                                   GetActiveWeaponStoneSocket(), GetStoneRingStoneSocket()})
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
    // B-304: the old entry behind a Generic's or Refractor's adapter.
    if (GetCharacterClass() == ECharacterClass::Resonator)
    {
        return nullptr;
    }
    return GetActiveAdapterLegacyEntry();
}

const FRingLoadoutEntry *ULoadoutComponent::GetActiveRingLoadout() const
{
    // B-304: the old entry behind the Resonator's active adapter.
    if (GetCharacterClass() != ECharacterClass::Resonator)
    {
        return nullptr;
    }
    return GetActiveAdapterLegacyEntry();
}

const FRingLoadoutEntry *ULoadoutComponent::GetActiveAdapterLegacyEntry() const
{
    const FCombatLoadout *Loadout = GetActiveLoadoutInStep();
    const FLoadoutSlot *Slot = Loadout ? GetActiveSlotIn(*Loadout, EGearKind::Adapter) : nullptr;
    return Slot ? Loadout->GetLegacyRingEntry(static_cast<int32>(Slot - Loadout->Slots.GetData())) : nullptr;
}

FCombatLoadout *ULoadoutComponent::GetActiveLoadoutInStep() const
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return nullptr;
    }
    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    Loadout.EnsureSlotsInStep(GetCharacterClass());
    return &Loadout;
}

bool ULoadoutComponent::IsWornByAnother(const FGuid &GearID) const
{
    if (!GearID.IsValid())
    {
        return false;
    }
    const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
    // TODO: count only a wearer who is in the party, as ClaimActiveSharedGear does (its party lookup is private to the
    // inventory); a stale wearer key would hide the piece. The clash step still strips worn pieces before a fight.
    const FName Wearer = Pool ? Pool->GetWearer(GearID) : NAME_None;
    if (Wearer.IsNone())
    {
        return false;
    }
    const UCharacterData *Data = GetOwnerCharacterData();
    if (WearerKeyData.Get() != Data)
    {
        WearerKeyData = Data;
        // B-360b: the pawn's member key (a copy of a shared character wears under its own key).
        WearerKey = UPoolSubsystem::GetCharacterKey(GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr);
    }
    return Wearer != WearerKey;
}

void ULoadoutComponent::ForEachContributingGear(TFunctionRef<void(const FWeaponLoadoutEntry *, const FRingLoadoutEntry *)> Visit) const
{
    // Q-244 (A): the adapter in use, then the weapon in hand, for every class (the evolution seat is the caller's).
    // Each is the live entry behind its slot, not the mirror's copy, so levels and rolls are current (D-111).
    const FRingLoadoutEntry *Adapter = GetActiveAdapterLegacyEntry();
    if (Adapter && Adapter->RingEntry.Ring)
    {
        Visit(nullptr, Adapter);
    }
    const FWeaponLoadoutEntry *Weapon = GetActiveWeaponLoadout();
    if (Weapon && Weapon->WeaponEntry.Weapon)
    {
        Visit(Weapon, nullptr);
    }
}

const FLoadoutSlot *ULoadoutComponent::GetActiveSlot(EGearKind Kind) const
{
    const FCombatLoadout *Loadout = GetActiveLoadoutInStep();
    return Loadout ? GetActiveSlotIn(*Loadout, Kind) : nullptr;
}

const FLoadoutSlot *ULoadoutComponent::GetActiveSlotIn(const FCombatLoadout &Loadout, EGearKind Kind) const
{
    // A Resonator's adapters are its adapter loadout only (GetActiveRing); a stray primary ring is never one in use.
    const bool bAdapterLoadoutOnly = Kind == EGearKind::Adapter && GetCharacterClass() == ECharacterClass::Resonator;
    const auto IsUsable = [this, &Loadout, Kind, bAdapterLoadoutOnly](int32 Index)
    {
        if (!Loadout.Slots.IsValidIndex(Index))
        {
            return false;
        }
        const FLoadoutSlot &Slot = Loadout.Slots[Index];
        if (bAdapterLoadoutOnly && Slot.LegacyOrigin != ELegacySlotOrigin::RingLoadout)
        {
            return false;
        }
        return !Slot.IsEmpty() && Slot.Kind() == Kind && !IsWornByAnother(Slot.GearID);
    };
    const int32 Active = Kind == EGearKind::Weapon ? Loadout.ActiveWeaponSlot : Loadout.ActiveAdapterSlot;
    if (IsUsable(Active))
    {
        return &Loadout.Slots[Active];
    }
    // D-110: no primary rule. With the active slot empty (or its piece worn by another member, B-221b), the first
    // usable slot of the kind is the one in use.
    for (int32 Index = 0; Index < Loadout.Slots.Num(); ++Index)
    {
        if (IsUsable(Index))
        {
            return &Loadout.Slots[Index];
        }
    }
    return nullptr;
}

FText ULoadoutComponent::GetTurnSwitchRefusedReason() const
{
    // B-170c (Q-170c), B-170h2: a switch is always free, any number of times (ET-19 changes); the old timeline's
    // one-switch-a-turn rule went with it.
    return FText::GetEmpty();
}

bool ULoadoutComponent::TrySwitchActiveSlot(EGearKind Kind, FText &OutReason)
{
    if (Kind == EGearKind::Weapon)
    {
        return TrySwitchSlotTo(Kind, INDEX_NONE, OutReason);
    }
    // The next ring, wrapping over the valid ones (the command menu's rule; counts valid entries, indexes RingLoadout).
    const FCombatLoadout &Loadout = GetActiveLoadout();
    int32 RingCount = 0;
    for (const FRingLoadoutEntry &Entry : Loadout.RingLoadout)
    {
        RingCount += Entry.IsValid() ? 1 : 0;
    }
    if (RingCount < 2)
    {
        OutReason = FText::GetEmpty(); // nothing to switch to: no switch used, no re-equip side effects
        return false;
    }
    return TrySwitchSlotTo(Kind, (Loadout.ActiveRingIndex + 1) % RingCount, OutReason);
}

bool ULoadoutComponent::TrySwitchSlotTo(EGearKind Kind, int32 AdapterIndex, FText &OutReason)
{
    OutReason = GetTurnSwitchRefusedReason();
    if (!OutReason.IsEmpty())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] %s: %s switch refused - already switched this turn"), *GetNameSafe(GetOwner()),
               Kind == EGearKind::Weapon ? TEXT("weapon") : TEXT("adapter"));
        return false;
    }
    // A switch that changes nothing (no loadout, one adapter, a bad or the current index) doesn't use up the turn's switch.
    if (Kind == EGearKind::Weapon)
    {
        const bool bShowPrimaryBefore = GetActiveLoadout().bShowPrimary;
        ToggleEquipment();
        bUsedTurnSwitch = GetActiveLoadout().bShowPrimary != bShowPrimaryBefore;
        return bUsedTurnSwitch;
    }
    const int32 RingIndexBefore = GetActiveLoadout().ActiveRingIndex;
    SetActiveRingIndex(AdapterIndex);
    bUsedTurnSwitch = GetActiveLoadout().ActiveRingIndex != RingIndexBefore;
    return bUsedTurnSwitch;
}

bool ULoadoutComponent::TrySwitchWeaponThisTurn(FText &OutReason)
{
    return TrySwitchActiveSlot(EGearKind::Weapon, OutReason);
}

bool ULoadoutComponent::TrySwitchRingThisTurn(int32 NewRingIndex, FText &OutReason)
{
    return TrySwitchSlotTo(EGearKind::Adapter, NewRingIndex, OutReason);
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
        if (GetCharacterSlotEvolution().IsBroken() && Lists(Loadout.EvolutionSpells))
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
    // B-098 (D-027): weapons give no spells, so no weapon crystal channels (or greys out) one.
    for (const FWeaponLoadoutEntry *Weapon : WeaponSpellRules::WeaponsGiveSpells()
                                                 ? TArray<const FWeaponLoadoutEntry *>{GetPrimaryWeaponLoadout(), GetSecondaryWeaponLoadout()}
                                                 : TArray<const FWeaponLoadoutEntry *>())
    {
        // B-197: the loadout's own overrides (FWeaponLoadoutEntry::AssignedSpells) come through the
        // same crystal, so they grey out with it too.
        // B-351d review: and the crystal's one-list picks.
        if (!Weapon || !(Lists(Weapon->WeaponEntry.AssignedSpells) || Lists(Weapon->AssignedSpells) || Lists(Weapon->WeaponEntry.SpellPicks)))
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
    return Lists(Ring->RingEntry.AssignedSpells) || Lists(Ring->RingEntry.SpellPicks) // B-351d review: a pick greys out too
           || (Ring->RingEntry.Ring && Lists(Ring->RingEntry.Ring->PresetSpells));
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

    // B-304: the piece the active-slot rule calls in use comes first, so wear lands where the getters say the
    // character is fighting (a worn-elsewhere skip or a fallback can make that differ from the old fields' pick).
    if (const FWeaponLoadoutEntry *InHand = GetActiveWeaponLoadout(); InHand && InHand->WeaponEntry.Weapon == Holder)
    {
        return const_cast<FRuntimeAttachedItem *>(&InHand->WeaponEntry.AttachedItem);
    }
    if (const FRingLoadoutEntry *InUse = GetActiveAdapterLegacyEntry(); InUse && InUse->RingEntry.Ring == Holder)
    {
        return const_cast<FRuntimeAttachedItem *>(&InUse->RingEntry.AttachedItem);
    }

    // Weapon holder — match by Weapon pointer across primary/secondary slots,
    // regardless of PrimarySlotType. A holder lookup is by identity, not slot.
    if (UWeaponData *Weapon = Cast<UWeaponData>(Holder))
    {
        // B-310: a Generic showing its secondary is fighting with it, so with two identical weapons the wear goes
        // there first (only a Generic has a secondary weapon).
        if (!Loadout.bShowPrimary && Loadout.SecondaryWeapon.WeaponEntry.Weapon == Weapon)
        {
            return &Loadout.SecondaryWeapon.WeaponEntry.AttachedItem;
        }
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
        // B-310: the adapter in use first, so with two identical adapters the cast's wear lands on the one cast
        // through, not on the first copy in the list.
        if (Loadout.RingLoadout.IsValidIndex(Loadout.ActiveRingIndex) && Loadout.RingLoadout[Loadout.ActiveRingIndex].RingEntry.Ring == Ring)
        {
            return &Loadout.RingLoadout[Loadout.ActiveRingIndex].RingEntry.AttachedItem;
        }
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

FRuntimeAttachedItem *ULoadoutComponent::FindAttachedItemForSlot(const FEquippedCrystalSlot &Slot)
{
    UInventoryComponent *Inv = GetInventoryComponent();
    if (!Slot.GearID.IsValid() || !Inv || !Inv->SavedLoadouts.IsValidIndex(Inv->ActiveLoadoutIndex))
    {
        return FindAttachedItemByHolder(Slot.Holder);
    }
    FCombatLoadout &Loadout = Inv->SavedLoadouts[Inv->ActiveLoadoutIndex];
    const auto Is = [&Slot](const UObject *Asset, const FGuid &GearID) { return Asset == Slot.Holder && GearID == Slot.GearID; };
    if (Is(Loadout.PrimaryWeapon.WeaponEntry.Weapon, Loadout.PrimaryWeapon.WeaponEntry.PersistentID))
    {
        return &Loadout.PrimaryWeapon.WeaponEntry.AttachedItem;
    }
    if (Is(Loadout.SecondaryWeapon.WeaponEntry.Weapon, Loadout.SecondaryWeapon.WeaponEntry.PersistentID))
    {
        return &Loadout.SecondaryWeapon.WeaponEntry.AttachedItem;
    }
    if (Is(Loadout.PrimaryRing.RingEntry.Ring, Loadout.PrimaryRing.RingEntry.PersistentID))
    {
        return &Loadout.PrimaryRing.RingEntry.AttachedItem;
    }
    for (FRingLoadoutEntry &Entry : Loadout.RingLoadout)
    {
        if (Is(Entry.RingEntry.Ring, Entry.RingEntry.PersistentID))
        {
            return &Entry.RingEntry.AttachedItem;
        }
    }
    // A GUID that names nothing in the loadout: an asset match could be the other identical copy (D-111), so none.
    UE_LOG(LogTemp, Verbose, TEXT("[ULoadoutComponent::FindAttachedItemForSlot] GUID %s not in the active loadout"), *Slot.GearID.ToString());
    return nullptr;
}

FRuntimeAttachedItem ULoadoutComponent::GetCrystalEntryForSlot(const FEquippedCrystalSlot &Slot) const
{
    // const_cast as GetCrystalEntryByHolder: the lookup is shared with the mutating helpers; this only copies.
    const FRuntimeAttachedItem *Found = const_cast<ULoadoutComponent *>(this)->FindAttachedItemForSlot(Slot);
    return Found ? *Found : FRuntimeAttachedItem();
}

void ULoadoutComponent::ResetCrystalEntryForSlot(const FEquippedCrystalSlot &Slot)
{
    if (!Slot.GearID.IsValid())
    {
        ResetCrystalEntryByHolder(Slot.Holder);
        return;
    }
    if (FRuntimeAttachedItem *Found = FindAttachedItemForSlot(Slot))
    {
        *Found = FRuntimeAttachedItem();
    }
}

int32 ULoadoutComponent::RepairCrystalEntryForSlot(const FEquippedCrystalSlot &Slot, int32 Amount)
{
    if (!Slot.GearID.IsValid())
    {
        return RepairCrystalEntryByHolder(Slot.Holder, Amount);
    }
    FRuntimeAttachedItem *Found = FindAttachedItemForSlot(Slot);
    if (!Found)
    {
        return -1;
    }
    Found->RepairBetweenCombats(Amount);
    return Found->GetCurrentDurability();
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
    /** Every weapon and ring slot of Loadout that can hold an attachment, in a fixed order: its element socket and
     *  (B-351b) its stone socket. */
    template <typename TLoadout, typename FFunc>
    void ForEachAttachmentSlot(TLoadout &Loadout, FFunc &&Func)
    {
        Func(static_cast<UObject *>(Loadout.PrimaryWeapon.WeaponEntry.Weapon), Loadout.PrimaryWeapon.WeaponEntry.PersistentID,
             Loadout.PrimaryWeapon.WeaponEntry.AttachedItem, Loadout.PrimaryWeapon.WeaponEntry.StoneSocket);
        Func(static_cast<UObject *>(Loadout.SecondaryWeapon.WeaponEntry.Weapon), Loadout.SecondaryWeapon.WeaponEntry.PersistentID,
             Loadout.SecondaryWeapon.WeaponEntry.AttachedItem, Loadout.SecondaryWeapon.WeaponEntry.StoneSocket);
        Func(static_cast<UObject *>(Loadout.PrimaryRing.RingEntry.Ring), Loadout.PrimaryRing.RingEntry.PersistentID,
             Loadout.PrimaryRing.RingEntry.AttachedItem, Loadout.PrimaryRing.RingEntry.StoneSocket);
        for (auto &Entry : Loadout.RingLoadout)
        {
            Func(static_cast<UObject *>(Entry.RingEntry.Ring), Entry.RingEntry.PersistentID, Entry.RingEntry.AttachedItem,
                 Entry.RingEntry.StoneSocket);
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
    LoadoutCarryInternal::ForEachAttachmentSlot(Loadout, [&Carried](UObject *Holder, const FGuid &GearID, const FRuntimeAttachedItem &Attached,
                                                                    const FRuntimeAttachedItem &StoneSocket)
    {
        if (Holder)
        {
            FCarriedAttachment &Entry = Carried.Attachments.AddDefaulted_GetRef();
            Entry.Holder = Holder;
            Entry.GearID = GearID;
            Entry.Attached = Attached;
            Entry.StoneSocket = StoneSocket; // B-351b
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
    // B-310 (D-111): matched by the owned item's GUID first, so an item gets its own wear wherever it sits. A slot
    // with no GUID (asset-built gear) or an entry saved before B-310 falls back to the asset, in slot order, each
    // carried entry used once.
    TBitArray<> Used(false, Carried.Attachments.Num());
    int32 Restored = 0;
    LoadoutCarryInternal::ForEachAttachmentSlot(Loadout, [&Carried, &Used, &Restored](UObject *Holder, const FGuid &GearID, FRuntimeAttachedItem &Attached,
                                                                                  FRuntimeAttachedItem &StoneSocket)
    {
        const auto Take = [&](int32 i)
        {
            Attached = Carried.Attachments[i].Attached;
            StoneSocket = Carried.Attachments[i].StoneSocket; // B-351b
            Used[i] = true;
            ++Restored;
        };
        if (GearID.IsValid())
        {
            for (int32 i = 0; i < Carried.Attachments.Num(); ++i)
            {
                if (!Used[i] && Carried.Attachments[i].GearID == GearID)
                {
                    Take(i);
                    return;
                }
            }
        }
        for (int32 i = 0; Holder && i < Carried.Attachments.Num(); ++i)
        {
            if (!Used[i] && !Carried.Attachments[i].GearID.IsValid() && Carried.Attachments[i].Holder == Holder)
            {
                Take(i);
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

    UE_LOG(LogTemp, Verbose, TEXT("[ULoadoutComponent] %s: carried crystal state restored (%d of %d attachments%s)"),
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

    UE_LOG(LogTemp, Verbose, TEXT("[LoadoutComponent] Set ActiveRingIndex to %d"), NewIndex);

    // Notify TurnManager so cached BonusTurnSpeed re-reads for this actor.
    // Out-of-combat / pre-combat case: actor isn't in Combatants, the loop in
    // OnActorSpeedChanged simply finds no match and returns — safe no-op.
    if (UWorld *World = GetWorld())
    {
        if (UGameInstance *GI = World->GetGameInstance())
        {
            if (UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(GI))
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
    USkillEffectManager *Effects = WoRFight::Get<USkillEffectManager>(GI);
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
    return SwitchToNextUsableSlot(EGearKind::Adapter);
}

bool ULoadoutComponent::SwitchToNextUsableSlot(EGearKind Kind)
{
    if (Kind == EGearKind::Weapon)
    {
        return false; // weapons only switch when the player asks
    }
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

EGuardStyle ULoadoutComponent::ResolveGuardStyle(bool bWeaponInHand, const FGuid &ActiveWeaponId, EGuardStyle Chosen,
                                              const FGuid &ChosenWithWeaponId)
{
    if (!bWeaponInHand)
    {
        return EGuardStyle::Unarmed;
    }
    // D-170: equipping a weapon makes the weapon guard the default, so Unarmed holds only for the weapon put away.
    // TODO: D-024 (the loadout screen's equip action): set Weapon there and drop the remembered weapon id. Until one
    // equip-commit path exists, re-equipping the same weapon keeps Unarmed (Q-392). A weapon with no id (enemy / test
    // weapons) never matches, so any weapon in hand brings the weapon guard back.
    return (Chosen == EGuardStyle::Unarmed && ChosenWithWeaponId.IsValid() && ActiveWeaponId == ChosenWithWeaponId) ? EGuardStyle::Unarmed
                                                                                     : EGuardStyle::Weapon;
}

const FPartyMember *ULoadoutComponent::FindGuardRecord() const
{
    const UCharacterDataComponent *CharComp = GetOwner() ? GetOwner()->FindComponentByClass<UCharacterDataComponent>() : nullptr;
    return CharComp && CharComp->PartyMemberId.IsValid() ? CharComp->FindCarriedPartyMember() : nullptr;
}

EGuardStyle ULoadoutComponent::GetGuardStyle() const
{
    const FWeaponLoadoutEntry *WeaponEntry = GetInventoryComponent() && GetActiveWeapon() ? GetActiveWeaponLoadout() : nullptr;
    const FGuid ActiveWeaponId = WeaponEntry ? WeaponEntry->WeaponEntry.PersistentID : FGuid();
    const FPartyMember *Record = FindGuardRecord();
    return ResolveGuardStyle(WeaponEntry != nullptr, ActiveWeaponId, Record ? Record->GuardStyle : LocalGuardStyle,
                             Record ? Record->GuardWeaponId : LocalGuardWeaponId);
}

namespace LoadoutGuardSync
{
    /** B-392b: on a co-op guest, its own player state, which tells the host (the host plays the guard and picks the
     *  parry visual); null on the host or alone. */
    AWoRPlayerState *GetGuestToTellHost(const UWorld *World)
    {
        if (!World || World->GetNetMode() != NM_Client)
        {
            return nullptr;
        }
        const APlayerController *PC = World->GetFirstPlayerController();
        return PC ? PC->GetPlayerState<AWoRPlayerState>() : nullptr;
    }
}

bool ULoadoutComponent::SetGuardStyle(EGuardStyle Style)
{
    const FWeaponLoadoutEntry *WeaponEntry = GetInventoryComponent() && GetActiveWeapon() ? GetActiveWeaponLoadout() : nullptr;
    if (Style == EGuardStyle::Unarmed && !WeaponEntry)
    {
        UE_LOG(LogWoRLoadout, Log, TEXT("%s: no weapon in hand, already guarding unarmed (B-392)"), *GetNameSafe(GetOwner()));
        return false;
    }
    const FGuid GuardWeaponId = Style == EGuardStyle::Unarmed ? WeaponEntry->WeaponEntry.PersistentID : FGuid();
    if (Style == EGuardStyle::Unarmed && !GuardWeaponId.IsValid())
    {
        // Unarmed holds only while the put-away weapon is in hand, found by its id; without one it would never hold.
        UE_LOG(LogWoRLoadout, Warning, TEXT("%s: the weapon in hand has no persistent id; Unarmed refused (B-392)"),
               *GetNameSafe(GetOwner()));
        return false;
    }
    LocalGuardStyle = Style;
    LocalGuardWeaponId = GuardWeaponId;
    // B-348b's rule: only a pawn stamped with its member id writes the record, never an enemy sharing its asset.
    if (FindGuardRecord())
    {
        const UCharacterDataComponent *CharComp = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
        const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
        if (UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr)
        {
            PartySession->SetMemberGuard(PartySession->GetMemberSlotOf(CharComp), Style, GuardWeaponId);
        }
    }
    if (AWoRPlayerState *Guest = LoadoutGuardSync::GetGuestToTellHost(GetWorld()))
    {
        Guest->ServerSetGuard(GetOwner(), Style); // B-392b (D-103): the host applies it to its copy and its record
    }
    UE_LOG(LogWoRLoadout, Log, TEXT("%s: guard set to %s (B-392)"), *GetNameSafe(GetOwner()),
           *StaticEnum<EGuardStyle>()->GetNameStringByValue(static_cast<int64>(Style)));
    return true;
}

EParryElementPriority ULoadoutComponent::GetParryElementPriority() const
{
    const FPartyMember *Record = FindGuardRecord();
    return Record ? Record->ParryElementPriority : LocalParryElementPriority;
}

void ULoadoutComponent::SetParryElementPriority(EParryElementPriority Priority)
{
    LocalParryElementPriority = Priority;
    if (FindGuardRecord())
    {
        const UCharacterDataComponent *CharComp = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
        const UGameInstance *GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
        if (UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr)
        {
            PartySession->SetMemberParryPriority(PartySession->GetMemberSlotOf(CharComp), Priority);
        }
    }
    if (AWoRPlayerState *Guest = LoadoutGuardSync::GetGuestToTellHost(GetWorld()))
    {
        Guest->ServerSetParryPriority(GetOwner(), Priority); // B-392b (D-103): the host picks the parry visual
    }
    UE_LOG(LogWoRLoadout, Log, TEXT("%s: parry colour priority set to %s (B-393)"), *GetNameSafe(GetOwner()),
           *StaticEnum<EParryElementPriority>()->GetNameStringByValue(static_cast<int64>(Priority)));
}

UAnimMontage *ULoadoutComponent::GetGuardMontage() const
{
    if (GetGuardStyle() == EGuardStyle::Weapon)
    {
        if (const UWeaponData *Weapon = GetActiveWeapon())
        {
            if (Weapon->ParryMontage)
            {
                return Weapon->ParryMontage;
            }
            UE_LOG(LogWoRLoadout, Verbose, TEXT("%s: weapon %s has no guard montage yet; the unarmed guard plays (B-392)"),
                   *GetNameSafe(GetOwner()), *Weapon->Name);
        }
    }
    return GetBlockMontage();
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

namespace LoadoutBoundSkin
{
    /** B-279b (D-033): the character whose skin Wearer wears, so its bound gear gets the bonus; null without one. */
    const UCharacterData *GetWornSkinFor(const UActorComponent &Component, const UCharacterData *Wearer)
    {
        const UWorld *World = Component.GetWorld();
        const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
        const UCharacterFormSubsystem *Forms = GI ? GI->GetSubsystem<UCharacterFormSubsystem>() : nullptr;
        return Forms ? Forms->GetSkinCharacterFor(Wearer) : nullptr;
    }
}

UCharacterData *ULoadoutComponent::GetOwnerCharacterData() const
{
    if (!GetOwner())
    {
        return nullptr;
    }

    if (!CachedCharacterDataComponent.IsValid())
    {
        CachedCharacterDataComponent = GetOwner()->FindComponentByClass<UCharacterDataComponent>();
    }
    if (UCharacterDataComponent *CharComp = CachedCharacterDataComponent.Get())
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
    if (!CachedInventory.IsValid())
    {
        CachedInventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
    }
    return CachedInventory.Get();
}

namespace
{
	/** B-378b: the curse subsystem when this loadout's character is one its player's curses weaken; else null. */
	const UCurseSubsystem *CursesFor(const ULoadoutComponent &Loadout)
	{
		const UGameInstance *GI = Loadout.GetOwner() ? Loadout.GetOwner()->GetGameInstance() : nullptr;
		const UCurseSubsystem *Curses = GI ? GI->GetSubsystem<UCurseSubsystem>() : nullptr;
		if (!Curses || Curses->GetHeldCurses().IsEmpty())
		{
			return nullptr; // the usual case: no lookups past this
		}
		return Curses->IsCursedHere(Loadout.GetOwnerCharacterData()) ? Curses : nullptr; // cached component
	}
}

// ==================== EQUIPMENT STAT QUERIES ====================

FEquipmentStatBonus ULoadoutComponent::GetActiveStatBonus(AActor *Actor) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(WoR_GetActiveStatBonus); // B-266: Insights marker (PerfAudit_2026-10-28.md)
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
        // B-382 (Q-164b): Amp lines are flat adds to an action's base, which the skill's own tier power
        // scales; scaling them by the item's tier too would double it, so they sum raw.
        Combined.Amp.Accumulate(Src.Amp);
    };

    auto WeaponTier = [](const FWeaponInventoryEntry &E) { return E.Weapon ? E.Tier : EItemTier::F_Tier; }; // instance Tier (was E.Weapon->Tier)
    auto RingTier   = [](const FRingInventoryEntry &E)   { return E.Ring   ? E.Tier : EItemTier::F_Tier; };  // instance Tier (was E.Ring->Tier)

    const FCombatLoadout &Loadout = GetActiveLoadout();

    // B-387 (Q-387): a refined crystal in a worn socket grants Amp to its own element by its tier (the deckbuilder's
    // ladder). An evolution in a gear socket gives passives only (D-147), so only a crystal counts here; a broken one
    // gives none (as it loses its element key and spells, D-083).
    auto AddSocketCrystalAmp = [&Combined](const FRuntimeAttachedItem &Socket)
    {
        if (Socket.IsCrystal() && !Socket.IsBroken())
        {
            AmpRules::AddElementLine(Combined.Amp, Socket.GetElement(), AmpRules::GetCrystalTierAmp(Socket.Crystal.Id.Tier));
        }
    };

    // B-306 (Q-244): the gear that counts, one rule for stats, resistances and effects.
    ForEachContributingGear([&](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
    {
        if (Weapon)
        {
            AddScaled(Weapon->WeaponEntry.StatBonus, WeaponTier(Weapon->WeaponEntry));
            AddSocketCrystalAmp(Weapon->WeaponEntry.AttachedItem);
        }
        else
        {
            AddScaled(Adapter->RingEntry.StatBonus, RingTier(Adapter->RingEntry));
            AddSocketCrystalAmp(Adapter->RingEntry.AttachedItem);
        }
    });

    // Innate (primary-slot) evolution — its flat substats are slot-level stats,
    // summed like a weapon/ring entry's (asset Base + per-instance Generated roll).
    // ATTACHED-to-gear evolutions stay infusion-only — deliberately not summed here.
    // Sums with the gear ForEachContributingGear visits (a Generic's secondary weapon
    // beside the seat, Q-244). Base + Generated
    // are the SAME item, so both ride the evolution item's tier.
    const FEvolutionAttachment Seat = GetCharacterSlotEvolution(); // B-357e: the one resolve path
    if (Seat.Item)
    {
        FEquipmentStatBonus EvoBonus = Seat.Item->BaseStatBonus;
        EvoBonus.Accumulate(Seat.GeneratedStatBonus);
        // Pillar percents deliberately NOT summed: the evolution's pillar % already
        // applies as ApplyEvolutionPillarModifier's CRYSTAL layer; leaving them in
        // would double-apply through that function's EQUIPMENT layer, which reads
        // this return value. (Zeroed before AddScaled, which would otherwise pass them through.)
        EvoBonus.BonusMindModifierPercent = 0.0f;
        EvoBonus.BonusBodyModifierPercent = 0.0f;
        EvoBonus.BonusSpiritModifierPercent = 0.0f;
        AddScaled(EvoBonus, Seat.Tier);
        // B-387 (Q-387): the evolution in the character's own seat grants Amp to its element by its tier (on gear it
        // gives passives only, D-147).
        if (!Seat.IsBroken())
        {
            AmpRules::AddElementLine(Combined.Amp, Seat.Item->GetAssociatedElement(),
                                     AmpRules::GetCrystalTierAmp(Seat.Tier));
        }
    }

    // B-380 (D-164): every apparel piece worn adds its HP and Defence lines, tier-scaled like gear, and its Amp lines
    // raw (B-386, through AddScaled);
    // outside the budget, so it sits beside whatever ForEachContributingGear visits.
    for (const FApparelEntry &Piece : Loadout.Apparel)
    {
        if (Piece.Apparel)
        {
            AddScaled(Piece.GetStatLines(), Piece.Tier);
            AddSocketCrystalAmp(Piece.AttachedItem); // B-387
        }
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

    // B-378b (MemoryCatalogue §3.8, review): the player's held curses weaken their own party characters, flat (added
    // after the tier-scaled sum, so a retuned tier ladder never scales them).
    if (const UCurseSubsystem *Curses = CursesFor(*this))
    {
        Combined.Accumulate(Curses->GetStatPenalty());
    }

    return Combined;
}

FResistanceBonus ULoadoutComponent::GetActiveResistanceBonus(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    FResistanceBonus Combined;

    // B-306 (Q-244): the gear that counts, one rule for stats, resistances and effects.
    ForEachContributingGear([&Combined](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
    {
        Combined.Accumulate(Weapon ? Weapon->WeaponEntry.ResistanceBonus : Adapter->RingEntry.ResistanceBonus);
    });

    // Evolution resistance, a permanent equipped bonus while slotted: the asset's authored Base plus the slotted
    // attachment's per-instance roll (the asset's GeneratedResistance is preview-only since U3c).
    // B-357c (D-147): through the one resolve path; only the character's own slot's evolution gives resistance
    // (asset Base + the slotted attachment's per-instance roll). A weapon's or adapter's gives only its passives.
    for (const FAttachedEvolution &Evolution : GatherAttachedEvolutions())
    {
        if (WoREvolutionPlacement::GivesStats(Evolution.Place))
        {
            FResistanceBonus EvoResist = Evolution.Attachment.Item->BaseResistance;
            EvoResist.Accumulate(Evolution.Attachment.GeneratedResistance);
            Combined.Accumulate(EvoResist);
        }
    }

    if (const UCurseSubsystem *Curses = CursesFor(*this)) // B-378b
    {
        Combined.Accumulate(Curses->GetResistancePenalty());
    }

    // B-380 (D-164): every apparel piece's resistance, with its crystal's element added (Crown).
    const FCombatLoadout &Loadout = GetActiveLoadout(); // merge B-357c + B-380: B-357c dropped it, B-380's apparel reads it
    for (const FApparelEntry &Piece : Loadout.Apparel)
    {
        if (Piece.Apparel)
        {
            Combined.Accumulate(Piece.GetResistance());
        }
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


    // B-306 (Q-244): the gear that counts, one rule for stats, resistances and effects.
    ForEachContributingGear([&Result](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
    {
        const UEquipmentDataBase *Asset = Weapon ? static_cast<const UEquipmentDataBase *>(Weapon->WeaponEntry.Weapon)
                                                 : static_cast<const UEquipmentDataBase *>(Adapter->RingEntry.Ring);
        Result.Append(Asset->GetStartingEffects());
    });

    // B-357b (D-147): the attached evolutions whose passives act at fight start (charge 0): the character's own
    // slot. Gear evolutions give theirs only to an action charged at L2 (B-357c), so none here, as before.
    for (const FAttachedEvolution &Evolution : WoREvolutionPlacement::FilterPassives(GatherAttachedEvolutions(), LOADOUT_FIGHT_START_CHARGE_LEVEL))
    {
        Result.Append(Evolution.Attachment.Item->GetStartingEffects());
    }
    // B-380 (D-164, Crown): apparel carries effects like the deckbuilder's vessels.
    const FCombatLoadout &Loadout = GetActiveLoadout(); // merge B-357c + B-380: B-357c dropped it, B-380's apparel reads it
    for (const FApparelEntry &Piece : Loadout.Apparel)
    {
        if (Piece.Apparel)
        {
            Result.Append(Piece.Apparel->GetStartingEffects());
        }
        // Crown, D-164: its passives. Merge B-357b + B-380 (Q-380): apparel is gear, so as a gear evolution's (D-147):
        // only on an action charged to GEAR_PASSIVES_CHARGE_LEVEL, never at fight start (the L2 read:
        // GetGearEvolutionPassivesForAction, B-357d).
        const UEvolutionItemData *Evolution = Piece.GetEvolution();
        if (Evolution && WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Adapter, LOADOUT_FIGHT_START_CHARGE_LEVEL))
        {
            Result.Append(Evolution->GetStartingEffects());
        }
    }

    return Result;
}

TArray<FGatheredEffect> ULoadoutComponent::GetActiveEffectsGathered(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    // Def-identity mirror of GetActiveEffects: the same gear (ForEachContributingGear), but
    // calls the ...Gathered() accessors so each effect carries its def id + bundle index.
    TArray<FGatheredEffect> Result;

    // B-192: bound gear adds its bonus only on its owner (B-279b: or in the owner's skin).
    const UCharacterData *Wearer = GetOwnerCharacterData();
    const UCharacterData *WornSkin = LoadoutBoundSkin::GetWornSkinFor(*this, Wearer);

    // B-306 (Q-244): the gear that counts, one rule for stats, resistances and effects.
    ForEachContributingGear([&Result, Wearer, WornSkin](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
    {
        const UEquipmentDataBase *Asset = Weapon ? static_cast<const UEquipmentDataBase *>(Weapon->WeaponEntry.Weapon)
                                                 : static_cast<const UEquipmentDataBase *>(Adapter->RingEntry.Ring);
        Result.Append(Asset->GetStartingEffectsGathered(Wearer, WornSkin));
    });

    // B-357b: the one resolve path, at fight start (charge 0).
    for (const FAttachedEvolution &Evolution : WoREvolutionPlacement::FilterPassives(GatherAttachedEvolutions(), LOADOUT_FIGHT_START_CHARGE_LEVEL))
    {
        Result.Append(Evolution.Attachment.Item->GetStartingEffectsGathered());
    }
    const FCombatLoadout &Loadout = GetActiveLoadout(); // merge B-357c + B-380: B-357c dropped it, B-380's apparel reads it
    for (const FApparelEntry &Piece : Loadout.Apparel) // B-380 (D-164)
    {
        if (Piece.Apparel)
        {
            Result.Append(Piece.Apparel->GetStartingEffectsGathered(Wearer, WornSkin));
        }
        const UEvolutionItemData *Evolution = Piece.GetEvolution(); // gated as GetActiveEffects' (D-147)
        if (Evolution && WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Adapter, LOADOUT_FIGHT_START_CHARGE_LEVEL))
        {
            Result.Append(Evolution->GetStartingEffectsGathered());
        }
    }

    return Result;
}

TArray<FGatheredEffect> ULoadoutComponent::GetActiveConditionalEffectsGathered(AActor *Actor) const
{
    (void)Actor; // Always == GetOwner(); parameter kept for caller clarity.

    // The same gear as GetActiveEffectsGathered (ForEachContributingGear), but the CONDITIONAL
    // subset (effects gated by a trigger, fired on impact rather than at combat start).
    TArray<FGatheredEffect> Result;

    // B-192: bound gear adds its bonus only on its owner (B-279b: or in the owner's skin).
    const UCharacterData *Wearer = GetOwnerCharacterData();
    const UCharacterData *WornSkin = LoadoutBoundSkin::GetWornSkinFor(*this, Wearer);

    // B-306 (Q-244): the gear that counts, one rule for stats, resistances and effects.
    ForEachContributingGear([&Result, Wearer, WornSkin](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
    {
        const UEquipmentDataBase *Asset = Weapon ? static_cast<const UEquipmentDataBase *>(Weapon->WeaponEntry.Weapon)
                                                 : static_cast<const UEquipmentDataBase *>(Adapter->RingEntry.Ring);
        Result.Append(Asset->GetConditionalEffectsGathered(Wearer, WornSkin));
    });

    // B-357b: the one resolve path, armed at fight start (charge 0).
    for (const FAttachedEvolution &Evolution : WoREvolutionPlacement::FilterPassives(GatherAttachedEvolutions(), LOADOUT_FIGHT_START_CHARGE_LEVEL))
    {
        Result.Append(Evolution.Attachment.Item->GetConditionalEffectsGathered());
    }
    const FCombatLoadout &Loadout = GetActiveLoadout(); // merge B-357c + B-380: B-357c dropped it, B-380's apparel reads it
    for (const FApparelEntry &Piece : Loadout.Apparel) // B-380 (D-164): its trigger-gated rows
    {
        if (Piece.Apparel)
        {
            Result.Append(Piece.Apparel->GetConditionalEffectsGathered(Wearer, WornSkin));
        }
        const UEvolutionItemData *Evolution = Piece.GetEvolution(); // gated as GetActiveEffects' (D-147)
        if (Evolution && WoREvolutionPlacement::GivesPassives(EEvolutionPlace::Adapter, LOADOUT_FIGHT_START_CHARGE_LEVEL))
        {
            Result.Append(Evolution->GetConditionalEffectsGathered());
        }
    }

    return Result;
}

TArray<FAttachedEvolution> ULoadoutComponent::GatherAttachedEvolutions() const
{
    TArray<FAttachedEvolution> Attached;
    const FCombatLoadout &Loadout = GetActiveLoadout();
    if (Loadout.PrimarySlotType == EPrimarySlotType::Evolution)
    {
        WoREvolutionPlacement::AppendCharacterSlot(Loadout.PrimaryEvolution, Attached);
    }
    ForEachContributingGear([&Attached](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
    {
        if (Weapon)
        {
            WoREvolutionPlacement::AppendFrom(Weapon->WeaponEntry.AttachedItem, EEvolutionPlace::Weapon, Attached);
        }
        else if (Adapter)
        {
            WoREvolutionPlacement::AppendFrom(Adapter->RingEntry.AttachedItem, EEvolutionPlace::Adapter, Attached);
        }
    });
    for (const FApparelEntry &Piece : Loadout.Apparel) // B-357d (D-164): apparel is gear too
    {
        if (Piece.Apparel)
        {
            WoREvolutionPlacement::AppendFrom(Piece.AttachedItem, EEvolutionPlace::Apparel, Attached);
        }
    }
    return Attached;
}

FEvolutionAttachment ULoadoutComponent::GetCharacterSlotEvolution() const
{
    for (const FAttachedEvolution &Evolution : GatherAttachedEvolutions())
    {
        if (Evolution.Place == EEvolutionPlace::Character)
        {
            return Evolution.Attachment;
        }
    }
    return FEvolutionAttachment();
}

TArray<FGatheredEffect> ULoadoutComponent::GetGearEvolutionPassivesForAction(EActionType ActionType, EInfusionSourceOption Source,
                                                                            int32 ChargeLevel) const
{
    TArray<FGatheredEffect> Passives;
    const TArray<FAttachedEvolution> Attached = GatherAttachedEvolutions();
    TSet<const UEvolutionItemData *> Counted;
    for (const FAttachedEvolution &Evolution : Attached)
    {
        if (Evolution.Place == EEvolutionPlace::Character)
        {
            Counted.Add(Evolution.Attachment.Item); // its effects are on all fight, under the same source key
        }
    }
    for (const FAttachedEvolution &Evolution : WoREvolutionPlacement::FilterForAction(Attached, ActionType, Source, ChargeLevel))
    {
        bool bAlreadyCounted = false;
        Counted.Add(Evolution.Attachment.Item, &bAlreadyCounted);
        if (bAlreadyCounted)
        {
            continue;
        }
        // Only lasting passives (every payload permanent), the ones the gear-swap path puts on (Q-081), so the list
        // is exactly what goes on and comes off (B-357d review).
        for (const FGatheredEffect &Effect : Evolution.Attachment.Item->GetStartingEffectsGathered())
        {
            if (Effect.Effect.IsLastingPassive())
            {
                Passives.Add(Effect);
            }
        }
    }
    return Passives;
}

FString ULoadoutComponent::GetAttachedEvolutionsString() const
{
    const TArray<FAttachedEvolution> Attached = GatherAttachedEvolutions();
    FString Out = FString::Printf(TEXT("[Evolutions] %s: %d attached"), *GetNameSafe(GetOwner()), Attached.Num());
    for (const FAttachedEvolution &Evolution : Attached)
    {
        Out += FString::Printf(TEXT("\n  %s on the %s: passives at charge 0 %s, at charge %d %s"), *GetNameSafe(Evolution.Attachment.Item),
                               WoREvolutionPlacement::GetPlaceName(Evolution.Place),
                               WoREvolutionPlacement::GivesPassives(Evolution.Place, LOADOUT_FIGHT_START_CHARGE_LEVEL) ? TEXT("yes") : TEXT("no"),
                               WoREvolutionPlacement::GEAR_PASSIVES_CHARGE_LEVEL,
                               WoREvolutionPlacement::GivesPassives(Evolution.Place, WoREvolutionPlacement::GEAR_PASSIVES_CHARGE_LEVEL) ? TEXT("yes") : TEXT("no"));
    }
    return Out;
}

static FAutoConsoleCommandWithWorld GLoadoutPrintEvolutionsCommand(
    TEXT("wor.Loadout.PrintEvolutions"),
    TEXT("B-357b: every combatant's attached evolutions (character slot, weapon, adapter) and when their passives act."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
    {
        for (TObjectIterator<ULoadoutComponent> It; It; ++It)
        {
            if (!It->IsTemplate() && It->GetWorld() == World)
            {
                UE_LOG(LogWoR, Display, TEXT("%s"), *It->GetAttachedEvolutionsString());
            }
        }
    }));

UEvolutionItemData *ULoadoutComponent::GetActivePrimaryEvolutionCrystal(AActor *Actor) const
{
    (void)Actor;

    const FCombatLoadout &Loadout = GetActiveLoadout();
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
    // attachment by value, so the result outlives a later loadout change.
    const FCombatLoadout &Loadout = GetActiveLoadout();
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
