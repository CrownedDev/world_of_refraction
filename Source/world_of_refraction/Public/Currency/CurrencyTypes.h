// CurrencyTypes.h
// Enums + replicable storage shapes for the resource-economy wallet
// (UCurrencyComponent). Dust and Essence are held in a FastArraySerializer —
// NOT TMap<Enum,int32> — because TMap is not natively replicable (see
// Resources_Design.md §16.2). Prismas/Prisms/Diamond are plain int32s on the
// component.
//
// A SINGLE reusable FCurrencyEntry / FCurrencyArray pair backs BOTH the Dust
// (14 keys) and Essence (2 keys) wallets; each wallet interprets Key via its
// own enum (EDustType / EEssencePool, stored as uint8). The owning component
// stamps each wallet's WalletType + OwnerComponent once at construction so the
// client-side FastArray callbacks can report which wallet changed and to whom.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "CurrencyTypes.generated.h"

class UCurrencyComponent;

/** Top-level currency selector for the unified Add/Spend/CanAfford/GetBalance API. */
UENUM(BlueprintType)
enum class ECurrencyType : uint8
{
    Prismas, // run-volatile, never banked (Replicated, NOT SaveGame)
    Prisms,  // persistent; account-shareable (routing wired later)
    Diamond, // persistent; account-wide premium (routing wired later)
    Dust,    // persistent; per-character; sub-keyed by EDustType
    Essence  // persistent; per-character; sub-keyed by EEssencePool
};

/** 14 dust wallets: 10 element (Generic = Quartz's element), 3 pillar, 1 ability. */
UENUM(BlueprintType)
enum class EDustType : uint8
{
    // Element (10)
    Fire, Water, Lightning, Wind, Earth, Light, Darkness, Void, Reality, Generic,
    // Pillar (3)
    Mind, Body, Spirit,
    // Ability (1)
    Ability
};

/** 2 essence pools, split by what they level. */
UENUM(BlueprintType)
enum class EEssencePool : uint8
{
    Weapon,
    Crystal
};

/** One keyed balance inside a FastArray wallet. Key is a uint8-cast EDustType
 *  or EEssencePool, disambiguated by the owning FCurrencyArray's WalletType. */
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

/** Replicable wallet: a FastArraySerializer over FCurrencyEntry. Used as two
 *  distinct UPROPERTY members on the component (Dust, Essence). */
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
    ECurrencyType WalletType = ECurrencyType::Dust;

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
