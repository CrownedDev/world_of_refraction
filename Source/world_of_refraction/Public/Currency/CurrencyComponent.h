// CurrencyComponent.h
// Per-character wallet for the resource economy (Resources_Design.md §1, §15,
// §16). Holds five currencies; World Stat Points are NOT here (they live on the
// character component — separate build).
//
// Replication-aware from line one (§16.2): SetIsReplicatedByDefault(true) in the
// ctor (the CharacterDataComponent reference omits this — a latent gap; here it
// is set so the component is genuinely network-ready, not just PIE-correct).
// Every Add/Spend is gated by HasServerAuthority() (no-op-safe in PIE-standalone);
// reads are const and ungated. Persistent balances are dual-tagged
// SaveGame + Replicated; Gold is Replicated only (run-volatile, never banked).
//
// SCOPE SEAM (account-vs-character routing is a LATER step): this component is
// deliberately NOT hard-bound to the pawn. It is a BlueprintSpawnableComponent
// attached in the owner Blueprint, so it can later be owned by APlayerState
// (account scope) or the pawn (character scope) without changing this class.
// Per-currency scope today:
//   - Gold / Dust / Essence : per-character (run / persistent)
//   - Prisms                   : per-character + account-shareable  (TODO: routing)
//   - Diamond                  : account-wide premium               (TODO: routing)
// TODO(scope-routing): when the PlayerState wiring lands, route Prisms/Diamond
// reads+writes to the account-scoped wallet; leave character-scoped balances here.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Currency/CurrencyTypes.h"
#include "CurrencyComponent.generated.h"

/** Fired after any balance changes — from a server-side mutation AND from the
 *  client-side replication callbacks (OnRep_* / FastArray item callbacks), so
 *  listeners update on both sides. SubKey is the uint8-cast EEssenceType for the
 *  typed Essence currency; 0 (ignored) for the scalar currencies. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCurrencyChanged, ECurrencyType, Currency, uint8, SubKey, int32, NewBalance);

UCLASS(ClassGroup = (Economy), meta = (BlueprintSpawnableComponent))
class WORLD_OF_REFRACTION_API UCurrencyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCurrencyComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const override;

    // ==================== UNIFIED API ====================

    /** Add Amount of Currency. Server-gated; no-op false on client or Amount<=0.
     *  SubKey selects the typed Essence wallet entry (cast EEssenceType);
     *  ignored for scalar currencies. */
    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool Add(ECurrencyType Currency, int32 Amount, uint8 SubKey = 0);

    /** Spend Amount of Currency. Server-gated; all-or-nothing (returns false and
     *  debits nothing if the balance is insufficient or Amount<=0). */
    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool Spend(ECurrencyType Currency, int32 Amount, uint8 SubKey = 0);

    /** True iff GetBalance(Currency, SubKey) >= Amount (and Amount>0). */
    UFUNCTION(BlueprintPure, Category = "Economy|Currency")
    bool CanAfford(ECurrencyType Currency, int32 Amount, uint8 SubKey = 0) const;

    /** Current balance. 0 for an absent Dust/Essence key. */
    UFUNCTION(BlueprintPure, Category = "Economy|Currency")
    int32 GetBalance(ECurrencyType Currency, uint8 SubKey = 0) const;

    // ==================== TYPED CONVENIENCE ====================

    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool AddEssenceType(EEssenceType Type, int32 Amount) { return Add(ECurrencyType::EssenceTyped, Amount, static_cast<uint8>(Type)); }

    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool SpendEssenceType(EEssenceType Type, int32 Amount) { return Spend(ECurrencyType::EssenceTyped, Amount, static_cast<uint8>(Type)); }

    UFUNCTION(BlueprintPure, Category = "Economy|Currency")
    int32 GetEssenceType(EEssenceType Type) const { return GetBalance(ECurrencyType::EssenceTyped, static_cast<uint8>(Type)); }

    /** Single gear-leveling essence (scalar, no sub-key). NOTE: a later step-7
     *  "deconstruct skill/ability/gear → essence" faucet will call AddGearEssence(...).
     *  Keep this clean + public; deconstruct logic is NOT built here. */
    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool AddGearEssence(int32 Amount) { return Add(ECurrencyType::GearEssence, Amount); }

    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool SpendGearEssence(int32 Amount) { return Spend(ECurrencyType::GearEssence, Amount); }

    UFUNCTION(BlueprintPure, Category = "Economy|Currency")
    int32 GetGearEssence() const { return GetBalance(ECurrencyType::GearEssence); }

    /** Single skill-leveling essence (scalar, no sub-key) — mirrors GearEssence; levels
     *  abilities + spells. A later deconstruct faucet will call AddSkillEssence(...). */
    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool AddSkillEssence(int32 Amount) { return Add(ECurrencyType::SkillEssence, Amount); }

    UFUNCTION(BlueprintCallable, Category = "Economy|Currency")
    bool SpendSkillEssence(int32 Amount) { return Spend(ECurrencyType::SkillEssence, Amount); }

    UFUNCTION(BlueprintPure, Category = "Economy|Currency")
    int32 GetSkillEssence() const { return GetBalance(ECurrencyType::SkillEssence); }

    // ==================== EVENTS ====================

    UPROPERTY(BlueprintAssignable, Category = "Economy|Currency")
    FOnCurrencyChanged OnCurrencyChanged;

    /** Broadcast helper — called by server-side mutations AND by the replication
     *  callbacks (OnRep_* + FCurrencyArray::NotifyEntryChanged). Public so the
     *  wallet struct can route client-side changes back here. */
    void NotifyChanged(ECurrencyType Currency, uint8 SubKey, int32 NewBalance);

    // ==================== DEBUG ====================

    /** Log + on-screen dump of the full wallet. Formatting lives in
     *  FCurrencyComponentDebug::GetWalletString (Debug holds the formatting, the
     *  component holds the button). CallInEditor button in the component's Details panel. */
    UFUNCTION(BlueprintCallable, Category = "Debug", meta = (CallInEditor = "true"))
    void PrintWallet();

protected:
    // ==================== STORAGE ====================
    // Persistent balances: SaveGame + Replicated. Gold: Replicated ONLY
    // (run-volatile — never banked, lost at run end). FastArrays notify via
    // their item callbacks, so they use plain Replicated (no ReplicatedUsing).

    /** Run currency — temp buffs / consumables only. Never persisted. */
    UPROPERTY(ReplicatedUsing = OnRep_Gold, BlueprintReadOnly, Category = "Economy|Currency")
    int32 Gold = 0;

    /** Hub spend-currency (spells/equipment floors). Account-shareable (TODO routing). */
    UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Prisms, BlueprintReadOnly, Category = "Economy|Currency")
    int32 Prisms = 0;

    /** Account-wide premium currency (TODO routing). */
    UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Diamond, BlueprintReadOnly, Category = "Economy|Currency")
    int32 Diamond = 0;

    /** Single gear-leveling essence (per-character) — scalar, like Prisms/Diamond. Levels weapons + rings. */
    UPROPERTY(SaveGame, ReplicatedUsing = OnRep_GearEssence, BlueprintReadOnly, Category = "Economy|Currency")
    int32 GearEssence = 0;

    /** Single skill-leveling essence (per-character) — scalar, mirrors GearEssence. Levels abilities + spells. */
    UPROPERTY(SaveGame, ReplicatedUsing = OnRep_SkillEssence, BlueprintReadOnly, Category = "Economy|Currency")
    int32 SkillEssence = 0;

    /** 14 typed-essence wallets (per-character). Not BlueprintReadOnly — FCurrencyArray
     *  is not a BlueprintType; BP access is via GetEssenceType/AddEssenceType/SpendEssenceType. */
    UPROPERTY(SaveGame, Replicated)
    FCurrencyArray EssenceWallet;

    // ==================== REP CALLBACKS ====================

    UFUNCTION()
    void OnRep_Gold();

    UFUNCTION()
    void OnRep_Prisms();

    UFUNCTION()
    void OnRep_Diamond();

    UFUNCTION()
    void OnRep_GearEssence();

    UFUNCTION()
    void OnRep_SkillEssence();

private:
    /** PIE-safe authority check: true in NM_Standalone, else owner authority.
     *  Mirrors UCharacterDataComponent::HasServerAuthority. */
    bool HasServerAuthority() const;

    /** Find (mutable) the wallet entry for Key, creating a zero entry if absent.
     *  Never returns null. */
    FCurrencyEntry &FindOrAddEntry(FCurrencyArray &Wallet, uint8 Key);

    /** Read-only balance for Key in Wallet; 0 if absent. */
    int32 GetWalletAmount(const FCurrencyArray &Wallet, uint8 Key) const;
};
