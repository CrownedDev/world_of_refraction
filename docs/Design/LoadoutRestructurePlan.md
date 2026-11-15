# Loadout restructure plan (D-221)

**Status:** AGREED (Crown, 2026-11-15: questions 1-7 answered, §7). Stage 1 is being built; each stage is its own Backlog job.
**Written:** 2026-11-15 (desktop writer), from D-221 (with Crown's answers to the plan's questions), D-219, D-218, `Audits/LoadoutAlignmentAudit.md` (LA-n), `Audits/CharacterStateAudit.md` (CS-n), `Audits/CharacterAssetDump.md`.
**Supersedes:** B-503t (its WIP branch `feature/B-503t-run-end-loadout` is reused in stage 1b). B-503v, w, x and y stay on hold until stage 2 is in; most of them fold into it.

## In plain words

Today a character's gear is stored the old way: a primary slot, a secondary slot, a ring list and a "show primary" flag. The character's own loadout is rebuilt from its asset on every map, and nothing records which loadout is in use. So any change to it is lost at the next map.

After the restructure:

- **One "wearing now" per character.** It is a short list of owned pieces, which one is ACTIVE, the apparel, the character's own evolution and the wheel pick. It is stored with the character, saved, and read by everything.
- **Presets.** A preset is a saved copy of "wearing now", made and put on at a mirror.
- **The template gives the hub's starting gear once.** After that it is never read again. **A trial always starts on the class default loadout** (`DA_ClassDefaultLoadouts`), the same for everyone.

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
    bool bLastUsed = false;                  // the automatic "last used gear" (question 9); one per character
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

**D-221, the class default loadout at a trial's start (stage 1c):**

A trial always starts on the class's default loadout, so every run has the same starting point (Crown: "maybe F tier gear"). This replaces "the last used gear is the trial's starting gear" and D-219's "Starting gear" paragraph. Roguelites work this way: Hades and Dead Cells start every run from the same base kit.

- **Where it's authored.** A new data asset, `UClassDefaultLoadoutData` (`UPrimaryDataAsset`, `Public/Loadout/ClassDefaultLoadoutData.h`), holding `TMap<ECharacterClass, FSavedStartingGear> Defaults` (the same starting-gear form as stage 4's template). There is one asset, `DA_ClassDefaultLoadouts` (an editor step under ContentNaming). It's referenced from `UTrialRunSubsystem` by a config path in `DefaultGame.ini`, the way `UPartySessionSubsystem` holds its fallback pawn class. With no asset or no entry for a class, the trial keeps what is worn and logs a warning.
- **The default pieces are owned once.** The first time a character needs its class defaults, the pieces are minted into the pool as that character's owned gear, tagged `bClassDefault` on their pool mark. They can't be sold, dismantled or given away; nothing piles up from run to run.
- **How a trial start applies them** (`BeginRunLayer`):
  1. Snapshot the hub's `FWornGear`, saved in `FSavedRun`, for a quit.
  2. Reset each default piece to its authored state: tier, empty sockets (crystals in them go back to the inventory, not destroyed) and the class's skill picks.
  3. Set `FWornGear::Equipment` and `Apparel` to the defaults, and `ActiveGearID` to the default ACTIVE piece; clear the wheel pick.
  4. The character's own evolution (`EvolutionID`, D-210) and innate spells stay: they belong to the character, not the kit.
  5. Home gear left behind stays owned in the pool. The trial mirror puts it on from a preset, priced by tier (§6).

**D-219, run endings (stage 1b, reusing B-503t's WIP):**
- **Extract, or beating the boss (a finish).**
  - What is worn carries into the hub (D-219).
  - Worn found gear and the found crystals in its sockets are **kept for the hub**: the `bRunOnly` mark becomes `bHubGear` (Crown: "just to have in the hub").
  - Unworn found gear goes back, as now.
  - The hub snapshot is dropped.
- **Out of wipes.**
  - What is worn carries into the hub (D-219).
  - **Found gear is not kept** ("a wipe does not provide it"). Worn found pieces go back and leave their slots empty.
  - The tier drop applies (below).
- **Quit, or the game closed mid-run.**
  - The tier drop applies first (Q7).
  - Then the hub snapshot is put back, and the found items are taken back (Q-439).
- **Hub gear (`bHubGear`).**
  - Worn and changed in the hub, and kept in presets.
  - Not sold or dismantled.
  - Can be put on at the trial mirror like home gear, priced by tier. This is the plan's reading of "only counts once it's kept"; see question 8.
- **Superseded.** D-219's "Starting gear" paragraph and the "last used gear is the starting gear" line in D-221 (4) no longer apply. B-503t's WIP rule ER-65f is rewritten for these rules before it lands in `docs/Rules/05-economy-and-runs.md`.
- **The run's gear pick.** `PoolRunLayer.cpp:131-162` picks the run's gear from **wearers**. The class defaults replace that pick, and wearers go in stage 4 (CS-3 second half, CS-14).

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
- **Socketed crystals and evolutions don't drop** (Q6, Crown took the default). Only the pieces drop.
- **Class default pieces** drop too, but are reset at the next trial start anyway.
- **A quit and the restore** (Q7, Crown took the default). A quit puts back the hub snapshot. The drop is applied **first**, to the pieces worn at the moment of quitting that the player still owns after the quit. That means home pieces bought in at the trial mirror, hub gear and the class defaults: found pieces go back anyway.
  - A home piece worn at the quit loses tier even if the restore then moves it to another slot or takes it off.
  - A home piece worn at the run start but taken off before the quit does not lose tier.
  - For a closed game, "the moment of quitting" is the `FWornGear` in the last save before the close.

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
- **As built (B-505):** until stage 2 moves the readers, `Equipment` keeps the old fields' places (`WornGearLayout`: the primary, the Generic's secondary, then the adapter loadout), so the old loadout is rebuilt with no loss. `CreateFromWornGear` takes each piece from the member's drawn copies only: a piece not drawn leaves its place empty. A Resonator's active adapter isn't recorded yet (no map kept it before either). Jobs: B-505 (the record), B-506 (every map wears it), B-507 (save v40).
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

### Stage 1b: run endings on the record (M, about 4 commits; reuses B-503t)
- The hub snapshot and its restore on a quit or a close.
- Found gear kept as hub gear on an extract or a boss win, and not kept on out of wipes (§3).
- The tier drop on a lost run (§3): `ApplyLostRunTierDrop`, and the `GearTierLoss` constant.
- D-219's rows go into `docs/Rules/05-economy-and-runs.md`.
- Specs per ending (extract, boss win, out of wipes, quit, closed mid-run):
  - what is worn in the hub after each ending;
  - found gear: kept on an extract or a win only;
  - tiers: out of wipes, a quit and a close drop each worn piece by 3, clamped at F; an extract, a win and a single wipe drop nothing; a home piece taken off before a quit keeps its tier.
- **PIE:** extract with a found axe socketed: it is worn in the hub. Wipe out with one: it's gone, and the worn pieces are 3 tiers lower. A quit puts the hub gear back.

### Stage 1c: the class default loadout at a trial's start (M, about 3 commits, plus an editor step)
- `UClassDefaultLoadoutData`, the `UTrialRunSubsystem` config path, minting the default pieces once (`bClassDefault`), and the reset-and-wear at `BeginRunLayer` (§3).
- Editor step: author `DA_ClassDefaultLoadouts` with an F-tier kit per class (a Generic, a Refractor and a Resonator). `// TODO:` the pieces, picked by Crown or by the editor writer from the existing F-tier weapons and adapters.
- Spec `WoR.Trial.ClassDefaults`: a trial starts on the class kit whatever the hub wore; default pieces are reset each start (tier, sockets, skills); the character's own evolution and innate spells stay; home gear stays owned.
- **PIE:** wear an S-tier sword in the hub, enter a trial: the class kit is worn. Extract: the run's gear is in the hub.
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
  - the automatic "last used gear" preset (question 9; no longer the starting gear).
- **`ApplyPreset` rules (D-221 defaults):**
  - A piece that is no longer owned leaves its slot empty, with a note.
  - A remembered crystal or evolution goes in **only** if one is in the inventory; otherwise the socket stays as it is, with a note.
  - A crystal already in a socket is replaced only when the preset's crystal is there to go in. The replaced one is destroyed (CL-28b), so the mirror lists it and asks before confirming.
- **The mirror:**
  - a hub actor and a trial actor (`AGearMirror`, `bChargesPrice`);
  - the trial one is placed by a designer; it needs one placed actor per trial map, as an editor step.
- **Placeholder UI (C++, Backlog rule 4):** the preset list, "Save current as…", "Put on", the price (trial), the notes, and the replace warning. It is replaced by the CommonUI screen later (D-024).
- **The Equipment screen loses its loadout switching** (D-221 (2)). The v2 screen's loadout picker goes; it edits `FWornGear` only.
- **Save version 42:** `FSavedGearPreset` replaces `FSavedCustomLoadout`.
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
- Delete `DA_Character_Template` (Q5); the played test character moves to `DA_Character_GenericLord`.

**Clashes removed:**
- LA-11 (the old save shape);
- LA-14 to LA-23 and LA-25;
- CS-14, CS-18, CS-23 in part;
- the asset-dump oddities.

## 5. Save versions

| Stage | Version | Change | Older saves |
|---|---|---|---|
| 1 | 40 | `FSavedWornGear` per character in `FSavedPool`; `FSavedPartyMember` loses `ActiveWeaponId` and the field-pick fields (moved) | wiped with a warning (D-060, D-138) |
| 1b / 1c | 41 | the hub snapshot (`FSavedWornGear`) in `FSavedRun`; `bHubGear` and `bClassDefault` on `FSavedRunMark` / the owned-item marks | wiped |
| 3 | 42 | `FSavedGearPreset` replaces `FSavedCustomLoadout` | wiped |
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

## 7. Questions: answered (Crown, 2026-11-15, D-221)

| # | Question | Answer |
|---|---|---|
| 1 | Is found gear kept after a run owned outright? | **No**: "just to have in the hub". It is hub gear (`bHubGear`), kept only on an extract or a boss win (§3) |
| 2 | Hub changes vs the next trial's start | No longer applies: a trial always starts on the class defaults (§3) |
| 3 | Can the trial mirror save presets too? | **Yes**, mid-run, found gear included; that gear only counts once it is kept. Presets are kept |
| 4 | What the mirror charges for | Crown took the default: only what it changes |
| 5 | Template and GenericLord | Delete `DA_Character_Template`; the played test character moves to `DA_Character_GenericLord` (stage 4 editor step) |
| 6 | Do socketed crystals drop tier? | Crown took the default: no, only the pieces |
| 7 | Which pieces drop on a quit? | Crown took the default: those worn at the quit that the player still owns |

**Still open (non-lore, built with the default unless Crown changes it):**

8. **Hub gear at the trial mirror.** The plan's reading of "only counts once it's kept" is that kept found gear (hub gear) can be put on at the trial mirror from a preset, priced by tier like home gear. The other reading is that hub gear stays in the hub only, and a preset naming it leaves that slot empty in a trial. **Default: it can be bought in.**
9. **The "last used gear" preset.** It's no longer the starting gear. **Default:** keep it as an automatic preset (the gear worn at the last run's end), so it can be put back on at a mirror. The other option is to drop it.

## 7b. A future stage, not designed (Crown's idea)

A trial-side place or track that changes the starting defaults and gives trial-related skills (Crown: "maybe a trial-specific one where you can get all your trial-related skills"). It would sit on `UClassDefaultLoadoutData` (§3). Nothing is planned or built until Crown designs it.

## 8. What stays on hold

- B-503t, v, w, x and y: superseded by or folded into stages 1b and 2, as listed above.
- B-504 (weapon positioning): separate.
- The three audit issues are **not** in this plan's stages, except where noted:
  - CS-4 (the crystal undo): likely gone in stage 1, checked there;
  - CS-5 (run-earned levels not saved): its own job;
  - CS-3's wrong run gear: stage 1c (the class defaults replace the run's gear pick).

**Not covered here:** CS-6, 10, 11, 13, 15-17, 19-22, 24 and 25. These are other systems (HP, head-start, vitals, AI seat flags, forms, counters, wallet). Each stays its own job.
