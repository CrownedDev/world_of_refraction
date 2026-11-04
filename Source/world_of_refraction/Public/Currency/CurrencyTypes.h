// CurrencyTypes.h
// Enums + replicable storage shapes for the resource-economy wallet
// (UCurrencyComponent). The typed Essence currency is held in a FastArraySerializer
// — NOT TMap<Enum,int32> — because TMap is not natively replicable (see
// Resources_Design.md §16.2). Gold/Prisms/Diamond/GearEssence are plain int32s
// on the component.
//
// B-333 (D-125): seven balances, all plain scalars: Gold, Prisms, Diamond, Gear / Skill /
// Elemental / Pillar Essence. The typed wallet below is OBSOLETE (B-333), kept until the sweep.
//
// The reusable FCurrencyEntry / FCurrencyArray pair backs the single typed Essence
// wallet (14 keys, interpreting Key via EEssenceType, stored as uint8). The owning
// component stamps the wallet's WalletType + OwnerComponent once at construction so
// the client-side FastArray callbacks can report which wallet changed and to whom.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "CurrencyTypes.generated.h"

class UCurrencyComponent;

/** Top-level currency selector for the unified Add/Spend/CanAfford/GetBalance API. */
UENUM(BlueprintType)
enum class ECurrencyType : uint8
{
    Gold,      // run-volatile, never banked (Replicated, NOT SaveGame)
    Prisms,       // persistent; account-shareable (routing wired later)
    Diamond,      // persistent; account-wide premium (routing wired later)
    EssenceTyped, // OBSOLETE (B-333): the 14 typed essences retire (D-125). Nothing earns or spends it now; delete in the sweep (D-124).
    GearEssence,  // persistent; per-character; single scalar (levels weapons + rings)
    SkillEssence, // persistent; per-character; single scalar (spells, abilities, ability stones) — appended
    ElementalEssence, // B-333 (D-125): persistent scalar; pays for anything with an element (Quartz folds in) — appended
    PillarEssence,    // B-333 (D-125): persistent scalar; pays for stat stones — appended
    MentorTokens      // B-361b (D-154): persistent scalar, the player's own; earned playing together, spent teaching — appended
};

namespace CurrencyBalances
{
    /** B-333 (D-125): the seven balances, in display order. Gold is run money; the rest are banked. */
    inline constexpr ECurrencyType SEVEN[] = {
        ECurrencyType::Gold, ECurrencyType::Prisms, ECurrencyType::Diamond, ECurrencyType::GearEssence,
        ECurrencyType::SkillEssence, ECurrencyType::ElementalEssence, ECurrencyType::PillarEssence};
    /** B-361b (D-154): every live balance, in display order: D-125's seven, then Mentor Tokens. */
    inline constexpr ECurrencyType ALL[] = {
        ECurrencyType::Gold, ECurrencyType::Prisms, ECurrencyType::Diamond, ECurrencyType::GearEssence,
        ECurrencyType::SkillEssence, ECurrencyType::ElementalEssence, ECurrencyType::PillarEssence, ECurrencyType::MentorTokens};
}

/** OBSOLETE (B-333): the 14 typed-essence keys (10 element, 3 pillar, 1 ability) retire (D-125);
 *  Elemental / Pillar / Skill Essence replace them. Kept, unrenumbered, until the sweep (D-124). */
UENUM(BlueprintType)
enum class EEssenceType : uint8
{
    // Element (10)
    Fire, Water, Lightning, Wind, Earth, Light, Darkness, Void, Reality, Generic,
    // Pillar (3)
    Mind, Body, Spirit,
    // Ability (1)
    Ability
};

/** One keyed balance inside a FastArray wallet. Key is a uint8-cast EEssenceType,
 *  disambiguated by the owning FCurrencyArray's WalletType. */
USTRUCT()
struct FCurrencyEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY()
    uint8 Key = 0;

    UPROPERTY()
    int32 Amount = 0;

    // FastArray client-side callbacks — broadcast the owning component's
    // OnCurrencyChanged. Defined in CurrencyComponent.cpp (needs the full
    // UCurrencyComponent type). Server-side change notification is broadcast
    // directly by the mutating Add/Spend call, not from here.
    void PostReplicatedAdd(const struct FCurrencyArray &InArray);
    void PostReplicatedChange(const struct FCurrencyArray &InArray);
};

/** Replicable wallet: a FastArraySerializer over FCurrencyEntry. Used as the single
 *  typed Essence wallet UPROPERTY on the component. */
USTRUCT()
struct FCurrencyArray : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FCurrencyEntry> Items;

    // Non-replicated wiring, stamped once in the component constructor (runs on
    // server AND clients). Lets the item callbacks above report the correct
    // currency to the correct component. Raw ptr is safe: OwnerComponent is the
    // outer that owns this struct-by-value member, so it outlives the wallet.
    UCurrencyComponent *OwnerComponent = nullptr;
    ECurrencyType WalletType = ECurrencyType::EssenceTyped;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo &DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FCurrencyEntry, FCurrencyArray>(Items, DeltaParms, *this);
    }

    // Routes a replicated client-side change to OwnerComponent->NotifyChanged.
    // Defined in CurrencyComponent.cpp.
    void NotifyEntryChanged(uint8 Key, int32 Amount) const;
};

template <>
struct TStructOpsTypeTraits<FCurrencyArray> : public TStructOpsTypeTraitsBase2<FCurrencyArray>
{
    enum
    {
        WithNetDeltaSerializer = true
    };
};
