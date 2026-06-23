// CurrencyTypes.h
// Enums + replicable storage shapes for the resource-economy wallet
// (UCurrencyComponent). The typed Essence currency is held in a FastArraySerializer
// — NOT TMap<Enum,int32> — because TMap is not natively replicable (see
// Resources_Design.md §16.2). Gold/Prisms/Diamond/GearEssence are plain int32s
// on the component.
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
    EssenceTyped, // persistent; per-character; sub-keyed by EEssenceType (14 keys) — FastArray
    GearEssence   // persistent; per-character; single scalar (gear / kit leveling)
};

/** 14 typed-essence keys: 10 element (Generic = Quartz's element), 3 pillar, 1 ability. */
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
