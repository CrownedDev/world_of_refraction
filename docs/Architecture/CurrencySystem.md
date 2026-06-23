# Currency System

## Overview

The Currency System is the per-owner **wallet** for the resource economy (see
`docs/Design/Resources_Design.md`). It is a single replicated `UActorComponent`,
`UCurrencyComponent`, holding five currencies behind one unified Add / Spend /
CanAfford / GetBalance API. It is **replication-aware from line one** (server-gated
mutations, dual-tagged `SaveGame`+`Replicated` storage) and **owner-agnostic** — it is
attached in the owner Blueprint and is deliberately not hard-bound to the pawn, so it can
later be re-homed on `APlayerState` (account scope) or the pawn (character scope) without
changing the class.

World Stat Points are **not** here — they are run power that lives on the character
component (a separate build).

## Architecture

### `UCurrencyComponent` (`UActorComponent`, ClassGroup `Economy`, BlueprintSpawnableComponent)

**The five currencies** (`ECurrencyType`):

| Currency | Storage | Tags | Scope (today) |
|----------|---------|------|---------------|
| `Gold` | `int32` | **Replicated only** (run-volatile, never banked) | per-character (run) |
| `Prisms` | `int32` | `SaveGame` + `Replicated` (`OnRep_Prisms`) | per-character + account-shareable (routing TODO) |
| `Diamond` | `int32` | `SaveGame` + `Replicated` (`OnRep_Diamond`) | account-wide premium (routing TODO) |
| `GearEssence` | `int32` | `SaveGame` + `Replicated` (`OnRep_GearEssence`) | per-character (gear / kit leveling) |
| `EssenceTyped` (14 keys) | `FCurrencyArray` (FastArray) | `SaveGame` + `Replicated` | per-character |

- **Typed Essence keys** (`EEssenceType`, 14): 10 element (`Fire`…`Generic`), 3 pillar (`Mind`/`Body`/`Spirit`), 1 `Ability`.
- The scalar currencies (`Gold`/`Prisms`/`Diamond`/`GearEssence`) are plain `int32` with `ReplicatedUsing = OnRep_*`.
  The typed Essence uses a FastArray (`TMap<Enum,int32>` is not natively replicable — see `Resources_Design.md` §16.2).

**Unified API** (all `BlueprintCallable`/`BlueprintPure`):
- `bool Add(ECurrencyType, int32 Amount, uint8 SubKey = 0)` — server-gated; no-op `false` on client or `Amount<=0`.
- `bool Spend(ECurrencyType, int32 Amount, uint8 SubKey = 0)` — server-gated; **all-or-nothing** (debits nothing if insufficient).
- `bool CanAfford(...) const` / `int32 GetBalance(...) const` — const, ungated reads (0 for an absent typed-Essence key).
- `SubKey` selects the typed Essence entry (cast `EEssenceType`); ignored for the scalars.

**Typed convenience** (thin wrappers over the unified API): `AddEssenceType`/`SpendEssenceType`/`GetEssenceType(EEssenceType)`
for the typed wallet, and `AddGearEssence`/`SpendGearEssence`/`GetGearEssence()` (scalar) for gear essence.

**Authority & persistence**: every mutation is gated by `HasServerAuthority()` (PIE-safe: true in
`NM_Standalone`, else owner authority — mirrors `UCharacterDataComponent::HasServerAuthority`). The
ctor calls `SetIsReplicatedByDefault(true)` so the component is genuinely network-ready (not just
PIE-correct). Persistent balances are `SaveGame`-tagged for a future save system; `Gold` is
`Replicated`-only (run-volatile).

### Replicable storage — `CurrencyTypes.h`

- **`FCurrencyEntry : FFastArraySerializerItem`** — one keyed balance (`uint8 Key`, `int32 Amount`).
  `Key` is a `uint8`-cast `EEssenceType`, disambiguated by the owning array's `WalletType`.
  Its `PostReplicatedAdd`/`PostReplicatedChange` callbacks route client-side changes via
  `FCurrencyArray::NotifyEntryChanged`.
- **`FCurrencyArray : FFastArraySerializer`** — `TArray<FCurrencyEntry> Items` + `NetDeltaSerialize`
  (`WithNetDeltaSerializer`). The reusable struct backs the single typed Essence wallet `UPROPERTY`
  on the component. Non-replicated `OwnerComponent` + `WalletType`
  are stamped once in the ctor (runs on server and clients) so the item callbacks can report which
  wallet changed and to whom.

### Change notification

`FOnCurrencyChanged(ECurrencyType Currency, uint8 SubKey, int32 NewBalance)` — fired on **both** sides:
server-side mutations broadcast it directly; client-side replication (scalar `OnRep_*` +
FastArray item callbacks → `NotifyEntryChanged` → `NotifyChanged`) broadcasts it too. `SubKey` is the
`uint8`-cast typed-Essence key (`EEssenceType`), or `0` (ignored) for the scalars.

### Debug tooling

- **`UCurrencyComponentDebug`** (`UBlueprintFunctionLibrary`) — `static FString GetWalletString(const UCurrencyComponent*)`:
  a one-line dump. Scalar currencies are **always** shown (incl. zero); the typed Essence list shows only
  **non-zero** entries. e.g.
  `Gold: 0 | Prisms: 120 | Diamond: 5 | GearEssence: 40 | Essence[Fire:50, Mind:12]`.
  Enumerator names come from `UEnum::GetAuthoredNameStringByValue` (build-config independent).
- **`UCurrencyComponent::PrintWallet()`** — a `CallInEditor` "Print Wallet" button in the component's
  Details panel; calls `GetWalletString` and surfaces it via `UE_LOG` + `GEngine->AddOnScreenDebugMessage`.
  The split is deliberate: the Debug library holds the formatting, the component holds the button.

## Integration Points

- **Build dependency:** `NetCore` added to `world_of_refraction.Build.cs` (FastArray / NetDeltaSerialize).
- **Delegate:** `OnCurrencyChanged` — UI/economy listeners bind here for both server and client updates.
- **Owner:** attached in the owner Blueprint (`BP_CharacterBase` currently — the loose `.uasset` is
  deferred / not part of the C++ clusters). No pawn binding in C++.

## Known Limitations / TODOs

- **Account-vs-character routing not wired.** `Prisms` (account-shareable) and `Diamond` (account-wide)
  currently store per-character. `TODO(scope-routing)`: when the `APlayerState` wiring lands, route their
  reads/writes to the account-scoped wallet; leave the character-scoped balances here.
- **No SaveGame system yet.** The `SaveGame` tags are latent until a save system exists (session-only today).
- **No reward/faucet sources.** Nothing awards currency in C++ yet (the post-defeat reward hook is
  greenfield — see `Resources_Design.md` BUILD-STATE NOTE). The gear-essence faucet
  (`AddGearEssence(…)` from a later deconstruct step) is public and ready but uncalled.
- **World Stat Points are not here** — run power lives on the character component (separate build).

## Changelog

| Date | Change | Branch |
|------|--------|--------|
| 2026-06-23 | Initial system — `UCurrencyComponent` replicated wallet (6 currencies; FastArray Dust+Essence; server-gated, dual-tagged storage; `OnCurrencyChanged`; `NetCore` dep). Debug pair `UCurrencyComponentDebug::GetWalletString` + `PrintWallet` CallInEditor button. Owner-agnostic (PlayerState home deferred). | feature/currency-component |
| 2026-06-23 | **Vocabulary + structure pass** — `Prismas`→`Gold` (run-volatile, tags unchanged); `Dust`→typed `Essence` (`EDustType`→`EEssenceType`; `Add/Spend/GetDust`→`*EssenceType`; `ECurrencyType::Dust`→`EssenceTyped`); the 2-pool essence collapsed to a single `GearEssence` scalar (`EEssencePool` + `ECurrencyType::Essence` dropped, `OnRep_GearEssence` added). Debug labels updated. Pure rename + scalar collapse — enum values preserved, serialized keys map identically. | feature/currency-component |
