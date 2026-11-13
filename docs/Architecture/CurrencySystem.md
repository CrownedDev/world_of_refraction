# Currency System

## Overview

The Currency System is the per-owner **wallet** for the resource economy (see
`docs/Design/Resources_Design.md`). It is a single replicated `UActorComponent`,
`UCurrencyComponent`, holding the economy's currencies (five scalars + the 14-key typed
Essence) behind one unified Add / Spend / CanAfford / GetBalance API. The **orchestration**
that spends/grants these (dismantle/merge/purchase/level/downgrade) lives in
`UEconomyService` — see [`EconomySystem.md`](./EconomySystem.md). It is **replication-aware from line one** (server-gated
mutations, dual-tagged `SaveGame`+`Replicated` storage) and **owner-agnostic** — it is
attached in the owner Blueprint and is deliberately not hard-bound to the pawn, so it can
later be re-homed on `APlayerState` (account scope) or the pawn (character scope) without
changing the class.

World Stat Points are **not** here — they are run power that lives on the character
component (a separate build).

## Architecture

### `UCurrencyComponent` (`UActorComponent`, ClassGroup `Economy`, BlueprintSpawnableComponent)

**The currencies** (`ECurrencyType` — 6 selectors: 5 scalar + the typed-Essence FastArray):

| Currency | Storage | Tags | Scope (today) |
|----------|---------|------|---------------|
| `Gold` | `int32` | **Replicated only** (run-volatile, never banked) | per-character (run) |
| `Prisms` | `int32` | `SaveGame` + `Replicated` (`OnRep_Prisms`) | per-character + account-shareable (routing TODO) |
| `Diamond` | `int32` | `SaveGame` + `Replicated` (`OnRep_Diamond`) | account-wide premium (routing TODO) |
| `GearEssence` | `int32` | `SaveGame` + `Replicated` (`OnRep_GearEssence`) | per-character (levels **weapons + rings**, and the evolution gear type) |
| `SkillEssence` | `int32` | `SaveGame` + `Replicated` (`OnRep_SkillEssence`) | per-character (levels **abilities + spells**) |
| `ElementalEssence` (B-333) | `int32` | `SaveGame` + `Replicated` (`OnRep_ElementalEssence`) | per-character (anything with an element, D-125) |
| `PillarEssence` (B-333) | `int32` | `SaveGame` + `Replicated` (`OnRep_PillarEssence`) | per-character (stat stones, D-125) |
| `EssenceTyped` (14 keys) — **OBSOLETE (B-333)** | `FCurrencyArray` (FastArray) | `SaveGame` + `Replicated` | per-character; nothing earns or spends it; delete in the D-124 sweep |

- **Typed Essence keys** (`EEssenceType`, 14) — **OBSOLETE (B-333, D-125)**: nothing earns or spends them; deleted in the D-124 sweep.
- The scalar currencies (`Gold`/`Prisms`/`Diamond`/`GearEssence`/`SkillEssence`/`ElementalEssence`/`PillarEssence`) are plain `int32` with `ReplicatedUsing = OnRep_*`.
  The typed Essence uses a FastArray (`TMap<Enum,int32>` is not natively replicable — see `Resources_Design.md` §16.2).
- **`GearEssence` vs `SkillEssence`** is the §3 category split: the gear faucet (weapon/ring/evolution dismantle) feeds Gear and the gear leveling sink spends it; the skill faucet (spell/ability dismantle) feeds Skill. The two never merge. (Note: `EEssenceType::Reality`, in the typed wallet, is the shared ½-cost co-currency for leveling — see `EconomySystem.md`.)

**Unified API** (all `BlueprintCallable`/`BlueprintPure`):
- `bool Add(ECurrencyType, int32 Amount, uint8 SubKey = 0)` — server-gated; no-op `false` on client or `Amount<=0`.
- `bool Spend(ECurrencyType, int32 Amount, uint8 SubKey = 0)` — server-gated; **all-or-nothing** (debits nothing if insufficient).
- `bool CanAfford(...) const` / `int32 GetBalance(...) const` — const, ungated reads (0 for an absent typed-Essence key).
- `SubKey` selects the typed Essence entry (cast `EEssenceType`); ignored for the scalars.

**Typed convenience** (thin wrappers over the unified API): `AddEssenceType`/`SpendEssenceType`/`GetEssenceType(EEssenceType)`
for the typed wallet, plus the scalar pairs `AddGearEssence`/`SpendGearEssence`/`GetGearEssence()` and
`AddSkillEssence`/`SpendSkillEssence`/`GetSkillEssence()`. (Leveling spends via the generic `Spend(ECurrencyType, …)`
with the essence type chosen per category — see `EconomySystem.md` `TryLevelUpEntry`.)

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
  `Gold: 0 | Prisms: 120 | Diamond: 5 | GearEssence: 40 | SkillEssence: 15 | Essence[Fire:50, Mind:12]`.
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
| 2026-11-04 | B-366 (D-159): player-facing names swap: `ECurrencyType::Gold` (run) reads **Prisms**, `ECurrencyType::Prisms` (hub) reads **Gold** (`WoRGameNames::GetCurrencyName`); code, save and function names unchanged (mapping comment at the enum); placeholder screens format the name from the table. B-362b (D-160): a wipe drops the run balance into a drop on the run, recovered by beating that team. | feature/B-366-currency-names |
| 2026-06-23 | Initial system — `UCurrencyComponent` replicated wallet (6 currencies; FastArray Dust+Essence; server-gated, dual-tagged storage; `OnCurrencyChanged`; `NetCore` dep). Debug pair `UCurrencyComponentDebug::GetWalletString` + `PrintWallet` CallInEditor button. Owner-agnostic (PlayerState home deferred). | feature/currency-component |
| 2026-06-23 | **Vocabulary + structure pass** — `Prismas`→`Gold` (run-volatile, tags unchanged); `Dust`→typed `Essence` (`EDustType`→`EEssenceType`; `Add/Spend/GetDust`→`*EssenceType`; `ECurrencyType::Dust`→`EssenceTyped`); the 2-pool essence collapsed to a single `GearEssence` scalar (`EEssencePool` + `ECurrencyType::Essence` dropped, `OnRep_GearEssence` added). Debug labels updated. Pure rename + scalar collapse — enum values preserved, serialized keys map identically. | feature/currency-component |
| 2026-06-23 | **Gear/Skill split** — added `SkillEssence` scalar (`ECurrencyType::SkillEssence`, `OnRep_SkillEssence`, `Add/Spend/GetSkillEssence`): Gear levels weapons/rings/evolution, Skill levels abilities/spells (§3 category split). | feature/currency-component |
| 2026-06-24 | Doc sync — added the `SkillEssence` row + accessors (the doc had lagged the split), pointer to `EconomySystem.md` for orchestration. (Stale header-comment note: `CurrencyComponent.h` still says "five currencies" / references a "Dust" scope that was renamed to `EssenceTyped` — code is correct, comment is stale; TODO refresh.) | feature/currency-component |
| 2026-10-26 | B-027 — `UCurrencyComponent::HasServerAuthority` (and `UCharacterDataComponent`'s, `EconomyService`'s `EconomyHasAuthority`, `UInventoryComponent`'s socket gate) all call the one header-only `WoRAuthority::HasAuthority(const AActor*)` (`Public/WoRAuthority.h`): standalone or server copy, false for null/client. Edge: an ownerless component now refuses (was allowed in standalone). Spec `WoR.Authority`. | refactor/B-027-one-authority-check |
| 2026-10-26 | **B-151 (D-046, Q-151): one wallet per player.** `AWoRPlayerState` holds the wallet (a `UCurrencyComponent`); `ATrialGameMode` / `ABattleGameMode` pick it. `CurrencyWallet::FindFor(actor)` is the one lookup: the pawn's player wallet, else (a party member with no PlayerState, e.g. an AI ally) the party leader's, else the actor's own component. Routed: `UEconomyService` (all ops), crystal-break yields, the shop, flee (Gold loss once per wallet). `ACombatCharacter::CurrencyComponent` OBSOLETE for the party. Debug: `wor.Wallet.Print`. Not carried across OpenLevel or saved yet (Pool arc, D-059). Spec `WoR.Economy.PlayerWallet`. | feature/B-151-player-wallet |
| 2026-10-26 | B-075: `UCurrencyComponent::Add` saturates each balance at the int32 limit (`CurrencyComponentInternal::SaturatingAdd`) instead of wrapping negative; not a design cap. Spec `WoR.Economy.CurrencyClamp`. | fix/B-075-small-fixes |
| 2026-11-02 | B-227: `AWoRPlayerState::CopyProperties` carries the currency balances across seamless travel (`UCurrencyComponent` copy). | feature/B-227-seamless-travel |
| 2026-11-03 | B-333 (D-125): seven balances. `ECurrencyType` appends `ElementalEssence`, `PillarEssence` (no renumbering); `CurrencyBalances::SEVEN` lists them; scalars route through `FindScalar`; `CopyBalancesFrom` / save banking carry them; save version 11 wipes older saves (D-060). `WoRGameNames::GetCurrencyName` names all seven; `GetWalletString` / `wor.Wallet.Print` show the seven; `wor.Wallet.Add` accepts the new two. `EssenceTyped`, `EEssenceType` and the typed helpers are OBSOLETE (B-333). | feature/B-333-seven-balances (UNBUILT) |
| 2026-11-04 | B-333b: the specs left on the typed-essence API (`PlayerWalletSpec`, `SeamlessTravelSpec`, `OwnedGearSyncSpec`, `CurrencyClampSpec`) moved to the seven balances; `FElementRow::Essence` flagged OBSOLETE (the pool's `PoolEssence` already was, B-348). No must-fix (an elemental level-up is covered by SevenBalancesSpec and EconomyRoundTripSpec; the two specs of the obsolete names / element table go with the D-124 sweep). Merged into dev. | chore/B-333b-seven-balances-specs |
| 2026-11-04 | B-361b (D-154, Q-361b): an eighth balance `ECurrencyType::MentorTokens` (appended; `UCurrencyComponent::MentorTokens` replicated; `CurrencyBalances::ALL` = the seven + Mentor Tokens for the debug print; banked by the save, version 17). `Currency/MentorTokens.h`: `MentorTokenRules` (1 per finished co-op run, 1 per boss beaten together, teach 1 at F + 1 per tier, two players minimum) and `WoRMentorTokens` (`GetTeachCost`, `PayPlayersTogether`, `GetStateString`, `wor.Mentor.Print`). Paid by `ABattleGameMode` on a won Boss-rank fight and by `UTrialRunSubsystem::ExitTrial` after the run's boss fell (`MarkRunBossBeaten`); spent by `WoRGive::TryTeachSkill` (the learner gets its own copy, `UPoolSubsystem::LearnSkillCopy`). Spec `WoR.Currency.MentorTokens`. | feature/B-361b-mentor-tokens |
| 2026-11-04 | B-366b (Q-366): `UEconomyService::GetPlaceCurrency` (run under way = `ECurrencyType::Gold`, else `Prisms`); a duplicate convert pays it; `WoRGameNames::GetInventoryVerbName(Verb, ConvertPays)` and `WoRInventoryVerbs::GetVerbName(Context, Verb)` name it on the Convert button, refusal, confirm text and the duplicate screen. | fix/B-366b-convert-place-currency |
| 2026-11-14 | B-425: a co-op guest sends its banked wallet and carried Gold to the host once on join (`AWoRPlayerState::ServerReportWallet`, applied once per player, the flag carried by `CopyProperties`); only an authority wallet loads at BeginPlay and banks at EndPlay or `USaveSubsystem::CaptureInto`. Spec in `WoR.Economy.PlayerWallet`. Follow-up: a guest's co-op earnings aren't saved to its own save. UNBUILT (cloud). | fix/B-425-guest-wallet |
| 2026-11-14 | B-438 (Q-438, split from B-425): the host sends a guest its wallet snapshot (`AWoRPlayerState::Client_BankWallet`) once its report is applied and at every host save (`USaveSubsystem::SaveNow`); the guest banks it and writes its own slot (`BankWalletSnapshot`) and, once confirmed, banks its mirror at leave. UNBUILT (cloud). | fix/B-438-guest-wallet-writeback |
