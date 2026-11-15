# Loadout restructure plan (D-221)

**Status:** PLAN, for Crown's OK. Nothing here is built. Each stage is its own Backlog job once Crown agrees.
**Written:** 2026-11-15 (desktop writer), from D-221, D-219 "Starting gear", D-218, `Audits/LoadoutAlignmentAudit.md` (LA-n), `Audits/CharacterStateAudit.md` (CS-n), `Audits/CharacterAssetDump.md`.
**Supersedes:** B-503t (its WIP branch `feature/B-503t-run-end-loadout` is reused in stage 1b). B-503v, w, x and y stay on hold until stage 2 is in; most of them fold into it.

## In plain words

Today a character's gear is stored the old way: a primary slot, a secondary slot, a ring list and a "show primary" flag. The character's own loadout is rebuilt from its asset on every map, and nothing records which loadout is in use. So any change to it is lost at the next map.

After the restructure:

- **One "wearing now" per character.** It is a short list of owned pieces, which one is ACTIVE, the apparel, the character's own evolution and the wheel pick. It is stored with the character, saved, and read by everything.
- **Presets.** A preset is a saved copy of "wearing now", made and put on at a mirror.
- **The template gives the starting gear once.** After that it is never read again.

## 1. The target structs

All new structs are `USTRUCT`s on the existing types. There is no parallel loadout model: `FLoadoutSlot` becomes the one entry, and the v2 screen's `FLoadoutV2Entry` already draws one entry per slot.

### `FLoadoutSlot` (exists: `Public/Loadout/FLoadoutSlot.h`): one piece of equipment

| Field | Today | After |
|---|---|---|
| `FGuid GearID` | the owned piece (the truth) | kept, unchanged |
| `FGuid OffHandID` | (new) | the second piece of a same-type pair (D-209 weapon sets); replaces `PrimaryOffHand` / `SecondaryOffHand` |
| `FGearInstance Gear` | a cached copy | kept as a cache only, filled from the pool on every build, never saved |
| `AssignedAbilities`, `SpellOverrides`, `OverrideAttack` | copies on the slot | deleted in stage 4. Skill picks live on the owned piece only (`FGearInstance::SkillPicks`, LA-5). `OverrideAttack` is deleted (LA-15) |
| `LegacyOrigin`, `LegacyIndex` | the mirror's link to the old fields | deleted in stage 4 |

Sockets (crystal, stone, socketed evolution) stay on the owned piece (`FGearInstance::AttachedItem` / `StoneSocket`). The slot never copies them.

### `FWornGear` (new, `Public/Loadout/WornGear.h`): "wearing now"

```cpp
USTRUCT(BlueprintType)
struct FWornGear
{
    TArray<FLoadoutSlot> Equipment;   // weapons and adapters, in the player's order (D-220: no "weapon slot")
    FGuid ActiveGearID;               // the ACTIVE entry's GearID: replaces bShowPrimary, ActiveLoadoutIndex, B-503s2's ActiveWeaponId
    FGuid EvolutionID;                // the character's own evolution (D-210 one line; LA-6: no longer "the primary slot")
    TArray<TObjectPtr<USpellData>> EvolutionSpells;
    TArray<FGuid> Apparel;            // owned apparel pieces
    TArray<TObjectPtr<USpellData>> InnateSpells;     // casters (and Broken Darkness's Darkness pool)
    TArray<FBDElementSpellPool> BDSpellPools;        // Broken Darkness
    FWheelPick WheelPick;             // D-218
};

USTRUCT()
struct FWheelPick                     // moved off FPartyMember, same meaning
{
    FFieldSkillChoice FieldSkill;     // today FPartyMember::FieldSkill
    int32 FieldAttackIndex = 0;       // today FPartyMember::FieldAttackIndex (Q-503 (3): the place in the list)
    bool bPicked = false;             // today FPartyMember::bFieldAttackPicked
};
```

The class limits (a Generic: 3 pieces, at most 2 weapons and 1 adapter) stay in the existing rules (`WeaponSetRules`, `TryChangeGear`). The struct does not enforce them.

**Item slots and the pouch.** These are not in `FWornGear`. The pouch per character (B-322) already holds carried items. `ItemSlots` / `EquippedItems` stay where they are, and are deleted later in their own clean-up (LA-20).

### `FGearPreset` (new, in `WornGear.h`): a saved copy

```cpp
USTRUCT()
struct FGearPreset
{
    FString Name;
    bool bLastUsed = false;                  // the automatic "last used gear" (D-221 (4)); one per character
    FWornGear Gear;
    TArray<FPresetSocket> Sockets;           // what was in each socket when the preset was made
};

USTRUCT()
struct FPresetSocket
{
    FGuid GearID;                            // which piece
    uint8 Socket = 0;                        // 0 = crystal / evolution socket, 1 = stone socket (D-141)
    FRuntimeAttachedItem Item;               // a crystal by FCrystalId {kind, tier}; an evolution by its asset
};
```

`FPoolCustomLoadouts` becomes `TArray<FGearPreset> Presets`. The struct keeps its name and the pool's per-character map, so custom loadouts are reused, not paralleled.

## 2. Where they live

| What | Where | Why |
|---|---|---|
| `FWornGear` per character | `UPoolSubsystem`: `TMap<FName, FWornGear> WornGear`, keyed by `GetCharacterKey(member)` (the custom loadouts' key, B-360b copies included) | The pool already owns the gear and the per-character loadouts. A benched character keeps its gear. Cross-run, so it is a game-instance subsystem (D-064 is for per-fight state). |
| Presets | `UPoolSubsystem::CustomLoadouts` (exists), now `FGearPreset`s | as above |
| On the pawn | `UInventoryComponent::SavedLoadouts` holds exactly one `FCombatLoadout`, built from `FWornGear` at spawn (`CreateFromWornGear`), and written back to the pool **on every change** (CS-8: today only at EndPlay and before a save) | Readers keep reading the pawn's loadout while stage 2 moves them |
| `FPartyMember` | loses `ActiveWeaponId`, `FieldSkill`, `FieldAttackIndex` and `bFieldAttackPicked` (moved to `FWornGear`). Keeps `GuardStyle`, `GuardWeaponId` and `ParryElementPriority` as the fallback for members never picked (D-218), and `CarriedGear` for wear and durability only | One fact, one place (LA-1, CS-9) |
| The template | `UInventoryData::SavedLoadouts[DefaultActiveLoadoutIndex]` is read **once**, when the character has no `FWornGear` yet (a new game, a new recruit); its pieces are added to the pool as owned | D-221 (1) |
| Save | `FSavedPool` gains `TArray<FSavedWornGear> WornGear` (per character). `FSavedCharacterLoadouts::Loadouts` becomes `TArray<FSavedGearPreset>` | D-059 explicit save structs |

## 3. How D-219 and D-218 sit on it

**D-219, run endings (stage 1b, reusing B-503t's WIP):**
- **Run start.** `BeginRunLayer` copies each member's `FWornGear`, plus the socket states of the pieces worn, into the run snapshot (`FSavedRun`, saved).
- **Finish, extract, out of wipes.**
  - The current `FWornGear` stays.
  - Found gear that is worn, and found crystals in its sockets, become owned: their `bRunOnly` mark is cleared.
  - The "last used gear" preset is overwritten with what is worn.
  - Unworn found gear keeps its rule (it goes back).
- **Quit, or the game closed mid-run.** The snapshot is put back, and the found items are taken back (Q-439).
- **"Starting gear" supersedes two lines.** D-219's two lines that say kept gear is "lost when the next run starts" no longer apply. Nothing is removed at the next run start. B-503t's "lost at the next run" step is **dropped**. Its WIP rule ER-65f is reworded before it lands in `docs/Rules/05-economy-and-runs.md`.
- **The run's gear pick.** `PoolRunLayer.cpp:131-162` picks the run's gear from **wearers**. It now reads `FWornGear` (CS-3, second half; CS-14).

**D-219, a lost run costs tier (stage 1b):**
- **Where a piece's tier lives.** On the owned instance, never the asset (`TierOnInstance.md`). That is `FGearInstance::Tier` in the pool's owned store (`UPoolSubsystem::OwnedWeapons` and the other owned lists; evolutions: `FEvolutionInventoryEntry::Tier`). It is saved as `FSavedOwnedItem::Tier`, so **the drop needs no new save field**.
- **When.** Once per run, on the ending:
  - **out of wipes:** yes;
  - **quit mid-run:** yes (`AbandonRun`);
  - **game closed mid-run:** yes, settled at the next start by `SettleRunAsQuit` (B-408a);
  - **a single wipe that doesn't end the run, a finish, an extract:** no.
- **Which pieces.** Every piece in `FWornGear` at that moment, owned gear included:
  - each `Equipment` entry's `GearID` and `OffHandID`;
  - `EvolutionID`;
  - each `Apparel` piece.
- **How.**
  - `PoolRunLayer` gets `ApplyLostRunTierDrop(Key)`. It lowers each piece's instance tier by `GearTierLoss::RANKS_LOST_ON_LOST_RUN`, clamped at F.
  - This goes below the asset's authored tier, which `TierOnInstance.md` calls the floor. Crown's ruling is "never below F", so the floor rule changes for this one case, and the doc gets a line.
  - The values derived from tier are worked out again from the new tier (budget, power, wear, dismantle value). Leveled stat pools are checked while building.
  - It runs in the same step that closes the run, so the save that records the ending already holds the lower tiers. A closed game is settled on load before anything else saves, so there is no double drop and no missed drop.
- **The named number:**
  ```cpp
  namespace GearTierLoss
  {
      constexpr int32 RANKS_LOST_ON_LOST_RUN = 3; // D-219: S to C; tuned in play (Crown: two, "maybe even 3")
  }
  ```
- **Socketed crystals.** A crystal is its `FCrystalId{Type, Tier}`. Crown's "everything you were wearing" reads as the pieces, not what is in their sockets. **Default:** sockets keep their crystal unchanged. This is question 6 below.
- **A quit and the restore.** A quit puts back the run-start loadout. The drop is applied **first**, to the pieces worn at the moment of quitting that the player still owns after the quit. That means home pieces only: found pieces go back anyway.
  - A home piece worn at the quit loses tier even if the restore then moves it to another slot or takes it off.
  - A home piece worn at the run start but taken off before the quit does not lose tier.
  - For a closed game, "the moment of quitting" is the `FWornGear` in the last save before the close.
  - This is question 7 below.

**D-218, the wheel (stage 1):**
- The wheel pick lives in `FWornGear::WheelPick`. A weapon pick sets `ActiveGearID` in the same write. The pick and the ACTIVE entry stay separate fields because an element-source pick leaves the ACTIVE weapon as it is.
- Guard and parry are worked out from the pick when `bPicked` is set, and otherwise from the `FPartyMember` fallbacks (as today, B-503s).
- B-503s2's stamping of field pawns stays. `ActiveWeaponId` folds into `ActiveGearID`.

## 4. The stages

Each stage is playable in PIE and builds, and every commit touches at most 3 files. Sizes are rough: S is about 1 commit, M is 2-4, L is 5-8.

### Stage 1: the record (L, about 6 commits)
- `WornGear.h` (the structs, and `CreateFromWornGear` / `ToWornGear` on `FCombatLoadout`). The pool holds `WornGear`, with get / set and `OnWornGearChanged`.
- The spawn draw (`UCharacterDataComponent::BeginPlay` → `DrawSharedGear`) builds the pawn's one loadout from `FWornGear`. The lore-loadout rebuild and `DefaultActiveLoadoutIndex` are read only to seed a character with no record.
- `TryChangeGear`'s commit, the socket writes, the wheel pick and Switch Weapon write `FWornGear` straight away.
- **Save version 40:** `FSavedWornGear`.
- Debug: `wor.Loadout.PrintWorn [member]` (`LoadoutDebug`), and `GetWornGearString`.
- Spec `WoR.Loadout.WornGear`: a change survives a map, a fight and a save/load; the wheel pick is the ACTIVE entry; an enemy sharing the asset is untouched.
- **PIE:** change gear, change map, fight, save, load. It is all still worn.
- **Clashes removed:**
  - CS-2 and CS-7 (LA-3): lore edits are no longer lost;
  - CS-3 first half (LA-4): the loadout in use is kept;
  - CS-8: gear reaches the pool on every change;
  - LA-1: the pick and the ACTIVE entry become one write.
- **Likely also removed:** CS-4 (the stale crystal undo), because sockets are read from the owned piece rather than `CarriedGear`. This is checked while building, not promised.

### Stage 1b: run endings on the record (M, about 3 commits; reuses B-503t)
- Snapshot and restore of `FWornGear`, the found-gear keep on carrying endings, and the "last used gear" write. D-219's rows go into `docs/Rules/05-economy-and-runs.md`.
- The tier drop on a lost run (§3): `ApplyLostRunTierDrop`, and the `GearTierLoss` constant.
- Specs per ending: finish, extract, out of wipes, quit, and closed mid-run. Each one checks the tiers:
  - out of wipes, a quit and a close drop each worn piece by 3, clamped at F;
  - a finish, an extract and a single wipe drop nothing;
  - a home piece taken off before a quit keeps its tier.
- **PIE:** finish a run with a found axe socketed. It is worn in the hub and at the next trial's start. A quit puts the old gear back.
- **Clashes removed:** CS-3 second half and CS-14 (the run's gear pick reads the record, not wearers).

### Stage 2: the readers (L, about 8 commits)
- **Every reader reads `FCombatLoadout::Slots`, now the truth.** Today it is a mirror built by `RebuildSlotsFromLegacy`. Readers ask through `GetActiveWeaponLoadout()` and a new `GetSlots()`. That means the roughly ten hand walkers listed in LA-7 (the memory walkers, the field's copy, the budget, the infusion sources, form progress, vendor compare and the command menu), done one walker per commit.
- **The old fields are still written, but derived from `Slots`.** This is the reverse of today, so nothing breaks mid-stage.
- **Skills:** the Skills tab and the fight read `GetSkillSlotAbilities` from the owned piece; the old merge for `!bSkillSlotsActive` goes.
- **Clashes removed:**
  - LA-2: the Skills tab matches the fight;
  - LA-5 / CS-12: one copy of the skill picks;
  - LA-6: the evolution beside the gear;
  - LA-7: one walker;
  - LA-8: Switch Weapon for a None primary;
  - LA-9: form progress;
  - LA-10: an off-hand socket warns;
  - LA-12: vendor compare on the ACTIVE piece;
  - LA-13: the put-away weapon refreshes on a gear change;
  - CS-9: guard, parry and pick read one record.
- **Folds in:** B-503v (audit 2), B-503w and B-503x where they are reader fixes. B-503y (the hub attack and wheel) is rebuilt on top of this stage.
- **PIE:** the Equipment and Skills tabs, the wheel, the strip, a fight and the parry all show the same piece and the same skills.

### Stage 3: the mirror and presets (L, about 6 commits)
- `UPoolSubsystem`:
  - `SavePreset(Key, Name)`;
  - `ApplyPreset(Key, Index, OUT TArray<FText> Notes)`;
  - `GetPresetPrice(Key, Index)`;
  - the automatic "last used gear".
- **`ApplyPreset` rules (D-221 defaults):**
  - A piece that is no longer owned leaves its slot empty, with a note.
  - A remembered crystal or evolution goes in **only** if one is in the inventory; otherwise the socket stays as it is, with a note.
  - A crystal already in a socket is replaced only when the preset's crystal is there to go in. The replaced one is destroyed (CL-28b), so the mirror lists it and asks before confirming.
- **The mirror:**
  - a hub actor and a trial actor (`AGearMirror`, `bChargesPrice`);
  - the trial one is placed by a designer; it needs one placed actor per trial map, as an editor step.
- **Placeholder UI (C++, Backlog rule 4):** the preset list, "Save current as…", "Put on", the price (trial), the notes, and the replace warning. It is replaced by the CommonUI screen later (D-024).
- **The Equipment screen loses its loadout switching** (D-221 (2)). The v2 screen's loadout picker goes; it edits `FWornGear` only.
- **Save version 41:** `FSavedGearPreset` replaces `FSavedCustomLoadout`.
- Spec `WoR.Loadout.Presets`: each default above, the price sum, and the last-used write.
- **PIE:** save a preset in the hub, change gear, put the preset on (free); at the trial mirror, the price shows and is paid in Prisms.
- **Clashes removed:** LA-24 in part (the v2 screen no longer turns its entry back into the old row to switch loadouts).

### Stage 4: the deletes and data-asset clean-up (M, about 5 commits, plus editor steps)

**Code (only after Crown's PIE pass of stages 1-3, D-067 / D-124 sweep):**
- On `FCombatLoadout`, delete:
  - `bShowPrimary`, `PrimarySlotType`, `PrimaryWeapon`, `PrimaryRing`, `PrimaryEvolution`, `PrimaryEvolutionInstance`;
  - `SecondarySlotType`, `SecondaryWeapon`, `RingLoadout`;
  - `RebuildSlotsFromLegacy`, `ApplyAutoEquip`, `bLore`, `HeldHomeLoadout`.
- On `FSavedLoadout`, delete:
  - the primary / secondary / `EquippedRings` fields;
  - the template form becomes `FSavedStartingGear`: a list of assets (each with its off hand), the ACTIVE index, the evolution and apparel.
- On `FLoadoutSlot`, delete the legacy and copy fields.
- Delete the old functions and getters: LA-14, LA-15, LA-17 to LA-23 and LA-25.
- Delete the wearers and `FPoolGearClash` (CS-14, CS-23 in part), and `InitializeFromPool` (CS-18).
- **No save bump:** the save structs already changed shape in stages 1 and 3.

**Editor steps (Crown, or the editor writer through the MCP connection; `.uasset`s are not edited from C++):**
- Re-author each character's template in the new starting-gear form. These are the two slot styles in the dump: the Generic's primary / secondary weapon, the casters' `primaryRing`, and the Resonator's `primaryWeapon` + `equippedRings` with zero GUIDs.
- Clear the old `primary*` / `secondary*` / `primaryWeaponAbilities` fields (filled on Resonator and Test).
- Remove the baked crystals and the leftover Garnet / F defaults with kind None.
- Fix the typo names (`Equpment`) under ContentNaming (B-123).
- Delete or repoint `DA_Test_EvoCrystal_Water`, and give `DA_Enemy_Darkness` its own inventory (it uses `DA_Test_Inventory`).
- Template and GenericLord are identical: Crown says if one goes.

**Clashes removed:**
- LA-11 (the old save shape);
- LA-14 to LA-23 and LA-25;
- CS-14, CS-18, CS-23 in part;
- the asset-dump oddities.

## 5. Save versions

| Stage | Version | Change | Older saves |
|---|---|---|---|
| 1 | 40 | `FSavedWornGear` per character in `FSavedPool`; `FSavedPartyMember` loses `ActiveWeaponId` and the field-pick fields (moved) | wiped with a warning (D-060, D-138) |
| 3 | 41 | `FSavedGearPreset` replaces `FSavedCustomLoadout` | wiped |
| 4 | — | none (the template's starting gear is not in the save) | — |

## 6. The trial mirror's price (placeholder)

Each piece the preset **puts on** that is not already worn in that place is priced by its tier, and the prices are summed. The same goes for each crystal or evolution it puts in a socket. The price is paid in the run's Prisms (D-156); the hub is free.

```cpp
namespace GearMirrorPrice
{
    // Prisms per item, by EItemTier (F=0 .. S=6). TODO: tune in PIE against a run's Prism income.
    constexpr int32 PIECE_PRISMS_BY_TIER[7]   = {0, 0, 0, 0, 0, 0, 0};   // TODO: values
    // Crystals by FCrystalId tier; evolutions by their item tier. TODO: values
    constexpr int32 SOCKET_PRISMS_BY_TIER[7]  = {0, 0, 0, 0, 0, 0, 0};   // TODO: values
    constexpr int32 MIN_TRIAL_PRICE = 0;                                  // TODO: a floor, if any
}
```

The values are zero until tuned, so the trial mirror is free until Crown sets them.

## 7. Open questions for Crown

1. **Found gear in "last used gear".** On a carrying ending, worn found gear becomes owned and is part of "last used gear" and the next run's start (D-219 "Starting gear"). Is it then owned outright, so that it can be put in other presets, sold or dismantled like home gear? **Default: yes, owned outright.** This matches what D-219 now says, where the gear is not removed at the next run.
2. **Hub changes and the next trial's start.** D-221 (4) says the last used gear is the trial's starting gear. If you change gear in the hub (free mirror, or the Equipment screen) after a run, what does the next trial start with?
   - **Default: what you wear when you enter.** That equals the last used gear unless you changed it. Monster Hunter works this way: you leave the village in what you wear.
   - The other reading: the trial always puts the last used gear back on.
3. **Does the trial mirror save presets too, or only put them on?** **Default: both**, since saving is free and only putting on costs. FFXIV's gear sets can be saved anywhere.
4. **What the mirror charges for.** Only pieces and crystals it changes (default), or the whole preset?
5. **Template and GenericLord** are identical assets. Keep both?
6. **A lost run's tier drop and socketed crystals.** Does "everything you were wearing" include the crystals and evolutions in the pieces' sockets? **Default: no.** Only the pieces drop (weapons, adapters, the character's own evolution, apparel), and sockets keep what they hold.
7. **The tier drop on a quit.** A quit puts back the run-start loadout. The plan drops tier on **the pieces worn at the moment of quitting that the player still owns**, which means the home pieces, since found ones go back anyway. Examples:
   - you start with an S sword, swap to a found axe, and quit: the sword is not dropped, and the axe goes back;
   - you keep the S sword on and quit: it drops to C.
   For a closed game, it is what was worn in the last save. **Default: as described.** The other reading: drop the run-start pieces that the restore puts back on.

## 8. What stays on hold

- B-503t, v, w, x and y: superseded by or folded into stages 1b and 2, as listed above.
- B-504 (weapon positioning): separate.
- The three audit issues are **not** in this plan's stages, except where noted:
  - CS-4 (the crystal undo): likely gone in stage 1, checked there;
  - CS-5 (run-earned levels not saved): its own job;
  - CS-3's wrong run gear: stage 1b.

**Not covered here:** CS-6, 10, 11, 13, 15-17, 19-22, 24 and 25. These are other systems (HP, head-start, vitals, AI seat flags, forms, counters, wallet). Each stays its own job.
