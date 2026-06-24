// PoolAccessors.cpp
// Bodies for the accessor/translator layer. The asset-dereffing accessors include the data-asset
// headers here (kept out of the public header). All assets are always-loaded → derefs are cheap.

#include "Pool/PoolAccessors.h"

#include "Equipment/Weapons/WeaponData.h"        // UWeaponData::WeaponType
#include "Equipment/Rings/RingData.h"            // URingData (FRingInventoryEntry::GetElement uses it)
#include "Equipment/Crystals/EvolutionItemData.h" // UEvolutionItemData::GetAssociatedElement
#include "Equipment/Crystals/ItemIdentity.h"     // ItemIdentity::GetElement(FCrystalId)
#include "Equipment/Crystals/CrystalTypeHelpers.h" // IsGemType for the bucket split
#include "Skills/Definitions/SpellData.h"        // USpellData::Element / School
#include "Skills/Definitions/AbilityData.h"      // UAbilityData::RequiredWeaponType

namespace PoolAccessors
{
    // ==================== ELEMENT ====================

    ESpellElement GetElement(const FWeaponInventoryEntry &Entry)
    {
        return Entry.GetElement(); // instance — resolves the slotted crystal (Generic if none/broken)
    }

    ESpellElement GetElement(const FRingInventoryEntry &Entry)
    {
        return Entry.GetElement(); // instance — slotted crystal
    }

    ESpellElement GetElement(const FEvolutionInventoryEntry &Entry)
    {
        return Entry.Item ? Entry.Item->GetAssociatedElement() : ESpellElement::Generic;
    }

    ESpellElement GetElement(const FSpellInstance &Instance)
    {
        return Instance.Spell ? Instance.Spell->Element : ESpellElement::Generic;
    }

    ESpellElement GetElement(const FCrystalId &Id)
    {
        return ItemIdentity::GetElement(Id); // key — derived from ECrystalType
    }

    // ==================== TIER ====================

    EItemTier GetTier(const FWeaponInventoryEntry &Entry) { return Entry.Tier; }
    EItemTier GetTier(const FRingInventoryEntry &Entry) { return Entry.Tier; }
    EItemTier GetTier(const FEvolutionInventoryEntry &Entry) { return Entry.Tier; }
    EItemTier GetTier(const FSpellInstance &Instance) { return Instance.Tier; }
    EItemTier GetTier(const FAbilityInstance &Instance) { return Instance.Tier; }
    EItemTier GetTier(const FCrystalId &Id) { return Id.Tier; }

    // ==================== QUALITY ====================

    EItemQuality GetQuality(const FWeaponInventoryEntry &Entry) { return Entry.Quality; }
    EItemQuality GetQuality(const FRingInventoryEntry &Entry) { return Entry.Quality; }
    EItemQuality GetQuality(const FEvolutionInventoryEntry &Entry) { return Entry.Quality; }
    EItemQuality GetQuality(const FSpellInstance &Instance) { return Instance.Quality; }
    EItemQuality GetQuality(const FAbilityInstance &Instance) { return Instance.Quality; }

    // ==================== WEAPON-TYPE ====================

    EWeaponType GetWeaponType(const FWeaponInventoryEntry &Entry)
    {
        return Entry.Weapon ? Entry.Weapon->WeaponType : EWeaponType::Sword;
    }

    EWeaponType GetWeaponType(const FAbilityInstance &Instance)
    {
        return Instance.Ability ? Instance.Ability->RequiredWeaponType : EWeaponType::Sword;
    }

    // ==================== SCHOOL ====================

    ESpellSchool GetSchool(const FSpellInstance &Instance)
    {
        return Instance.Spell ? Instance.Spell->School : ESpellSchool::Destruction;
    }

    EItemBucket GetBucket(const FCrystalId &Id)
    {
        // Locked 2-bucket split: gems are Crystal; everything non-gem (stat stones +
        // AbilityStone) folds into AugmentStone.
        return CrystalTypeHelpers::IsGemType(Id.Type) ? EItemBucket::Crystal : EItemBucket::AugmentStone;
    }

    // ==================== MATCH PREDICATES ====================
    // Each encodes its type's axis applicability: an ACTIVE filter axis the type lacks =>
    // exclude (return false). An owned asset pointer must be valid to match an asset-borne axis.

    bool MatchesFilter(const FPoolFilter &F, const FWeaponInventoryEntry &Entry)
    {
        if (F.bUseSchool) return false; // weapons have no school
        if (F.bUseElement && GetElement(Entry) != F.Element) return false;
        if (F.bUseWeaponType && (!Entry.Weapon || Entry.Weapon->WeaponType != F.WeaponType)) return false;
        if (F.bUseTier && Entry.Tier != F.Tier) return false;
        if (F.bUseQuality && Entry.Quality != F.Quality) return false;
        return true;
    }

    bool MatchesFilter(const FPoolFilter &F, const FRingInventoryEntry &Entry)
    {
        if (F.bUseSchool) return false;     // rings have no school
        if (F.bUseWeaponType) return false; // rings have no weapon-type
        if (F.bUseElement && GetElement(Entry) != F.Element) return false;
        if (F.bUseTier && Entry.Tier != F.Tier) return false;
        if (F.bUseQuality && Entry.Quality != F.Quality) return false;
        return true;
    }

    bool MatchesFilter(const FPoolFilter &F, const FEvolutionInventoryEntry &Entry)
    {
        if (F.bUseSchool) return false;     // evolutions have no school
        if (F.bUseWeaponType) return false; // ...nor weapon-type
        if (F.bUseElement && GetElement(Entry) != F.Element) return false;
        if (F.bUseTier && Entry.Tier != F.Tier) return false;
        if (F.bUseQuality && Entry.Quality != F.Quality) return false;
        return true;
    }

    bool MatchesFilter(const FPoolFilter &F, const FSpellInstance &Instance)
    {
        if (F.bUseWeaponType) return false; // spells have no weapon-type
        if (F.bUseElement && (!Instance.Spell || Instance.Spell->Element != F.Element)) return false;
        if (F.bUseSchool && (!Instance.Spell || Instance.Spell->School != F.School)) return false;
        if (F.bUseTier && Instance.Tier != F.Tier) return false;
        if (F.bUseQuality && Instance.Quality != F.Quality) return false;
        return true;
    }

    bool MatchesFilter(const FPoolFilter &F, const FAbilityInstance &Instance)
    {
        if (F.bUseElement) return false; // abilities have no element
        if (F.bUseSchool) return false;  // ...nor school
        if (F.bUseWeaponType && (!Instance.Ability || Instance.Ability->RequiredWeaponType != F.WeaponType)) return false;
        if (F.bUseTier && Instance.Tier != F.Tier) return false;
        if (F.bUseQuality && Instance.Quality != F.Quality) return false;
        return true;
    }

    bool MatchesFilter(const FPoolFilter &F, const FCrystalId &Id)
    {
        if (F.bUseSchool) return false;     // crystals have no school
        if (F.bUseWeaponType) return false; // ...nor weapon-type
        if (F.bUseQuality) return false;    // ...nor quality (fungible)
        if (F.bUseElement && ItemIdentity::GetElement(Id) != F.Element) return false;
        if (F.bUseTier && Id.Tier != F.Tier) return false;
        return true; // the refined/consumable pool dimension is applied by the caller (GetItems)
    }
}
