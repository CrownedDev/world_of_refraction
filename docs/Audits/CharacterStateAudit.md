# Character state audit (2026-11-15)

*Independent read-only audit by a fresh agent. Traced from source; nothing was run. Builds on LoadoutAlignmentAudit.md.*

**Verdict.** The weapon bug has two causes that stack: the field pick is written to a loadout that is thrown away on every rebuild, and the pick's own record is never written because field pawns are not "stamped" as party members. The pool keeps gear, skills, items and custom loadouts well; what is lost is everything stored per loadout on a lore loadout, the "which loadout is in use" index, and every setting that only a stamped pawn may write. Four more clashes can show in normal play (stale crystal state, two max-HP formulas, run levels not saved, run gear picked from the wrong loadout).

All paths below are under `D:/github/world_of_refraction/Source/world_of_refraction/` (`Public/` = headers, `Private/` = code).

Key fact for everything below: the real rebuild path is `UCharacterDataComponent::BeginPlay` -> `InitializeFromCharacterData` (asset) -> `DrawSharedGear` (pool) (`Private/Character/CharacterDataComponent.cpp:85-91`). `UInventoryComponent::InitializeFromPool` is only called by the debug command `wor.Pool.Draw` (`Private/Pool/PoolSubsystem.cpp:1344`).

---

## 1. The map

| Fact | Authored | Lives at runtime | Kept between maps | Saved |
|---|---|---|---|---|
| Identity (name, portrait, pawn class) | `UCharacterData` `CharacterData.h:125-155` | `UCharacterDataComponent::CharacterData` `CharacterDataComponent.h:115` | `FPartyMember::PawnClass / CharacterData / MemberId` `Party.h:64,69,162` | `FSavedPartyMember` `WoRSaveTypes.h:102,105,150` |
| Class, innate element, born-BD flag | `CharacterData.h:129,134,144` | read from the asset | asset ref on the member | asset ref only |
| Sub-stats (13 points) | `CharacterData.h:199-255` | read from the asset | asset | not saved (immutable) |
| Mind / Body / Spirit levels | base: `CharacterData.h:182-190` | `LiveWorldMind/Body/Spirit` `CharacterDataComponent.h:666-672`, seeded at `CharacterDataComponent.cpp:312-341` | head-start: `Party.h:126`; run-earned: `UTrialRunSubsystem::RunLevels` `TrialRunSubsystem.h:546` | head-start: `WoRSaveTypes.h:122-128`; run-earned: **not saved** (`SaveSubsystem.cpp:1352-1400`) |
| Level-purchase count | none | `WorldStatPurchaseCount` `CharacterDataComponent.h:690` | `RunLevels.Purchases` `TrialRunSubsystem.h:535` (in a run only) | not saved |
| HP / EP | max from asset formula + gear | `CurrentHP/EP/MaxHP/MaxEP` `CharacterDataComponent.h:128-139` | `FPartyMember::CurrentHP/EP` `Party.h:80,83`, banked at fight end `BattleGameMode.cpp:546` | `WoRSaveTypes.h:185,188` (in a run only, `SaveSubsystem.cpp:1213`) |
| Alive / fallen | none | `bIsAlive` `CharacterDataComponent.h:154` | carried HP of 0 (`Party.h:210`) | with HP |
| Status effects, build-up | skill assets | `USkillEffectManager` on the pawn; build-up is a fight subsystem | none (per fight) | none |
| Equipment owned (weapons, adapters, evolutions) | `UInventoryData` (`CharacterData.h:176`) | `UInventoryComponent::Weapons/Rings` `InventoryComponent.h:114,118`, `UEvolutionInventoryComponent::Entries` | `UPoolSubsystem::OwnedWeapons/Rings/Evolutions + GearOwners` `PoolSubsystem.h` (private store), pushed on every change `InventoryComponent.cpp:2111` | `FSavedPool::Gear/Evolutions/Owners` `WoRSaveTypes.h:641+` |
| Crystals on gear, crystal wear | default crystal on the gear asset | on the loadout's copy and on the owned piece (`WriteLoadoutToOwned` `InventoryComponent.cpp:1299`) | pool piece **and** `FPartyMember::CarriedGear` `Party.h:88` | `FSavedGearDetail` `WoRSaveTypes.h:400` **and** `FSavedCarriedGear` `:70` |
| Learned spells and abilities | `UInventoryData` | `UInventoryComponent::Spells/Abilities` `InventoryComponent.h:106,110` | `UPoolSubsystem::CharacterSkills` (banked once, then the pool wins, `InventoryComponent.cpp:1601-1606`) | `FSavedCharacterSkills` `WoRSaveTypes.h:608` |
| Carried items (pouch) | `UInventoryData` | `UCrystalInventoryComponent` bound to the pouch | `UPoolSubsystem::Pouches` | `FSavedPouch` `WoRSaveTypes.h:624` |
| Lore loadouts | `UInventoryData::SavedLoadouts` `InventoryData.h:114` | `UInventoryComponent::SavedLoadouts` with `bLore` `FCombatLoadout.h:115` | **nowhere**: rebuilt from the asset `InventoryComponent.cpp:2787-2790` | not saved |
| Custom loadouts | none | same array, `bLore` false | `UPoolSubsystem::CustomLoadouts`, pushed only at EndPlay and before a save `InventoryComponent.cpp:518-559`, `SaveSubsystem.cpp:846-851` | `FSavedCustomLoadout` `WoRSaveTypes.h:515` |
| Active loadout | `DefaultActiveLoadoutIndex` `InventoryData.h:121` | `UInventoryComponent::ActiveLoadoutIndex` `InventoryComponent.h:138` | **nowhere**: reset at `InventoryComponent.cpp:2796` | not saved |
| Active weapon (in hand) | per loadout `FSavedLoadout.h:154` | `FCombatLoadout::bShowPrimary` `FCombatLoadout.h:124` | only inside a custom loadout | `FSavedCustomLoadout::bShowPrimary` `WoRSaveTypes.h:527` |
| Skill and spell picks | gear asset built-ins | owned piece and each loadout's copy (`InventoryComponent.cpp:1313-1318`) | pool piece | `FSavedGearDetail` `WoRSaveTypes.h:417,433` |
| Field attack pick | none | `UFieldMovementComponent::FieldAttackIndex` `FieldMovementComponent.h:395` | `FPartyMember::FieldAttackIndex / bFieldAttackPicked` `Party.h:198,203` (never written in play, see clash 1) | `WoRSaveTypes.h:178,182` |
| Field skill (movement) | none | `ActiveChoice` `FieldMovementComponent.h:345` | `FPartyMember::FieldSkill` `Party.h:192` (same problem) | `WoRSaveTypes.h:170,173` |
| Guard style, parry colour | none | `LocalGuardStyle / LocalGuardWeaponId / LocalParryElementPriority` `LoadoutComponent.h:912-916` | `FPartyMember` `Party.h:175,180,186` | `WoRSaveTypes.h:158-166` |
| Apparel owned | apparel assets | read from the pool | `UPoolSubsystem::OwnedApparel` | in `FSavedPool::Gear` `SaveSubsystem.cpp:1268-1275` |
| Apparel worn | per loadout `FSavedLoadout.h:182` | `FCombatLoadout::Apparel` `FCombatLoadout.h:214` | only inside a custom loadout (lore refuses, `LoadoutApparelModel.cpp:40`) | `FSavedCustomLoadout::Apparel` `WoRSaveTypes.h:578+` |
| Cosmetics (montages, VFX) | `UCosmeticsData` `CosmeticsData.h:45-122` via `CharacterData.h:263` | read from the asset | asset | not saved |
| Form / skin / outfit (per player, not per character) | `UCharacterFormData` | `UCharacterFormSubsystem` `CharacterFormSubsystem.h:117-126` and a copy on `AWoRPlayerState` `WoRPlayerState.h:243-247` | the subsystem (GameInstance); player state copies itself on travel `WoRPlayerState.cpp:198` | `FSavedForms` `WoRSaveTypes.h:708` |
| Form progress (per player) | form asset | `UCharacterFormSubsystem::Progress` `:117` | the subsystem | `FSavedForms::Progress` |
| Broken Darkness flag | `bBrokenDarknessInnate` `CharacterData.h:144` | `bIsBrokenDarkness` `CharacterDataComponent.h:183` | `FPartyMember::bBrokenDarkness` `Party.h:138`, banked at `BattleGameMode.cpp:555` | `WoRSaveTypes.h:133` |
| BD strain, absorption pool | none | `UBrokenDarknessManager` (`BrokenDarknessManager.h:136,488`) | `FPartyMember::RunStrain / CarriedAbsorption` `Party.h:143,92` | `WoRSaveTypes.h:136,195` (in a run only) |
| Curses (per player) | `UCurseData` | `UCurseSubsystem::HeldCurses` `CurseSubsystem.h:93` | the subsystem | `FSavedCurses` `WoRSaveTypes.h:764` |
| Memories | `UMemoryData` | run: `UTrialRunSubsystem::RunMemories` `TrialRunSubsystem.h:613`; known ids: `WoRPlayerState.h:265` | run subsystem; player state copies on travel `WoRPlayerState.cpp:199` | `FSavedRun::RunMemories`, `FSavedJournal` |
| Hidden counters | none | `UHiddenCounterSubsystem::Counts` `HiddenCounterSubsystem.h:49`, keyed by the asset, not the member | the subsystem | `FSavedCounters` `WoRSaveTypes.h:794` |
| Formation row, AI seat, AI difficulty | none | copies on the battle pawn `CharacterDataComponent.h:160,165` | `FPartyMember` `Party.h:98,115,120` | `WoRSaveTypes.h:108-118` |
| Run-only recruit's given gear | none | built into a run preset `InventoryComponent.cpp:1443-1449` | `FPartyMember::RunWeapon / RunRing` `Party.h:152` | `WoRSaveTypes.h:143,146` |
| Who commands the character (co-op) | none | `AWoRPlayerState::ClaimedMembers / ClaimedCharacters` `WoRPlayerState.h:250-255` | copied on travel `WoRPlayerState.cpp:203-209` | not saved |
| Wallet (per player, not per character) | none | `UCurrencyComponent` on `AWoRPlayerState` `WoRPlayerState.cpp:104` | copied on travel `WoRPlayerState.cpp:194-197`; banked at EndPlay `:171` | `FSavedWallet` `WoRSaveTypes.h:385` |

---

## 2. The clashes

### Must fix

**1. Field pawns are never stamped, so five settings never reach the party record.**
What: only `ABattleGameMode::SpawnCombatant` sets `PartyMemberId` (`Private/Trial/BattleGameMode.cpp:439`; the header says a hub pawn is invalid, `CharacterDataComponent.h:118-121`). The trial spawn sets `CharacterData` only (`Private/Trial/TrialGameMode.cpp:98-107`). Every "only a stamped pawn writes" guard therefore blocks the field pawn: field attack pick (`FieldMovementComponent.cpp:122-127`, `FieldMovementAttack.cpp:365`), field skill (`FieldMovementComponent.cpp:213`), guard and parry colour (`LoadoutComponent.cpp:3640-3644, 3719, 3749`), head-start (`CharacterDataComponent.cpp:398`), a BD revert (`BrokenDarknessManager.cpp:440`).
Case: on the trial map pick the Hammer slice on the wheel, or pick Glide as the field skill. Win one fight. Back on the map the wheel is on slice 1 and the field skill is Dash again. `bFieldAttackPicked` is never true in play, so the D-218 "pick drives guard and parry" code (`LoadoutComponent.cpp:3646-3676`) never runs. The specs pass because they stamp the pawn by hand (`Tests/ParryVisualsSpec.cpp:211`).
Fix: stamp the leader's pawn in `ATrialGameMode::SpawnDefaultPawnAtTransform_Implementation` (`CharComp->PartyMemberId = Party->Members[0].MemberId;` beside line 104), and the same wherever the hub pawn is made.

**2. The weapon picked on the field is written to a loadout that is not kept.** (This is the bug seen.)
What: the pick sets `bShowPrimary` on the active loadout (`FieldMovementAttack.cpp:380-392`). If that loadout is a lore one, it is rebuilt from the asset on the next map (`InventoryComponent.cpp:2787-2790`) and the EndPlay push skips it (`InventoryComponent.cpp:534`). If it is a custom one, the flag is pushed (`FCombatLoadout.cpp:373`), but the active index goes back to the asset default (`InventoryComponent.cpp:2796`), so a different loadout fights.
Case: Generic with Sword and Hammer on the default loadout. Pick Hammer on the field. Walk into an enemy. The battle pawn is built from the asset: Sword in hand.
Fix: keep "weapon in hand" where it survives. Smallest: store the picked weapon's id on `FPartyMember` (next to `GuardWeaponId`), and after `DrawSharedGear` set `bShowPrimary` on the active loadout from it. This needs fix 1 so the record is written. (Builds on the earlier audit's 1 and 3.)

**3. The loadout in use is still not kept, and it also picks the wrong gear for the run.** (Earlier audit's 4, still open, with a new consequence.)
What: `ActiveLoadoutIndex` is reset at `InventoryComponent.cpp:2796`; no pool or save field holds it. The run takes "worn" gear by the wearers record (`PoolRunLayer.cpp:131-162`), which follows the active loadout at the time (`LoadoutComponent.cpp:259`, `InventoryComponent.cpp:2036-2050`).
Case: in the hub set custom "Bruiser" in use and enter a trial. Bruiser's pieces go on the run. The trial pawn comes up on the default loadout; its pieces were left home, so they are stripped (`InventoryComponent.cpp:1536-1537, 1831-1866`) and a primary is refilled from whatever is in view (`:1868`). The player fights with a loadout they did not choose. Worn apparel on Bruiser is off too.
Fix: one `FName -> index` (or loadout name) map in the pool beside `CustomLoadouts`, written in `SetActiveLoadoutIndex`, read at the end of `DrawSharedGear`, plus one save field.

**4. A crystal changed on the trial map between fights is undone at the next fight.**
What: crystal state is stored twice: on the owned piece in the pool, and in `FPartyMember::CarriedGear`. The carried copy is written only at a fight's end (`BattleGameMode.cpp:551`) and by repair (`GearRepair.cpp:263`). Every pawn BeginPlay copies it over the loadout by gear id with no check, then writes that to the owned piece (`LoadoutComponent.cpp:3237-3287`, called at `CharacterDataComponent.cpp:93-96`).
Case: win a fight. On the map, swap the Garnet in the sword for a Sapphire (the Garnet goes back to the pouch). Start the next fight without resting: the sword has the Garnet again, the Sapphire is gone, and the Garnet is now also in the pouch.
Fix: on a socket change (`UInventoryComponent::HandleGearSocketChanged`, `InventoryComponent.cpp:1227`) clear or update that gear id in the member's `CarriedGear`. Better later: drop `CarriedGear.Attachments` and trust the owned piece, which already gets the wear at fight end (`CombatOrchestrator.cpp:65-76`). Traced from code; confirm in PIE.

**5. Levels earned in a run are lost on save and load.**
What: `RunLevels` and `UnspentLevels` live only on `UTrialRunSubsystem` (`TrialRunSubsystem.h:546`); `CaptureRun` does not write them (`SaveSubsystem.cpp:1352-1400`) and `FSavedRun` has no field (`WoRSaveTypes.h:252-348`).
Case: earn two Body levels and buy one in a run, quit to desktop, resume the run: Body is back to the base, the vendor's price is back to the first step, unspent levels are gone.
Fix: add the map to `FSavedRun` and fill / restore it in `CaptureRun` / `RestoreRun`.

### Should fix

**6. Two "full HP" numbers.** Rest and trial entry fill the record from the asset formula (`PartySessionSubsystem.cpp:1062-1075`); the pawn's max adds gear, stones and run levels (`CharacterDataComponent.cpp:920-945`), and the carry is clamped to it (`:139-140`). Case: a member with +HP gear rests and starts the next fight below full; a BD or debuffed case can go the other way. Fix: store "full" as a flag (or the sentinel) on rest and let the pawn seed to its own max.

**7. Lore loadout edits are lost** (earlier audit's 3, still open at `InventoryComponent.cpp:534, 2787`). It now also covers the field weapon pick (clash 2). Same fix as before: copy to a custom loadout on first edit, or refuse.

**8. Custom loadouts reach the pool only at EndPlay and before a save.** `InventoryComponent.cpp:556-559`, `SaveSubsystem.cpp:846-851`. Gear and skills push on every change; loadouts do not. A crash loses the session's loadout edits, and co-op or a second pawn of the same character reads a stale list. Fix: call `PushCustomLoadoutsToPool` at the end of `TryChangeLoadout`.

**9. Guard, parry colour and field pick each have a component copy and a record copy, picked by "is the pawn stamped".** `LoadoutComponent.h:912-916` against `Party.h:175-203`; `FieldMovementComponent.h:345,395`. After fix 1 the component copies are only for enemies and guests. Fix: once stamped, read and write the record only; keep the local fields for unrostered pawns.

**10. Head-start has a record copy and a component copy, and no caller.** `SetHeadStartAllocation` has no C++ caller (`CharacterDataComponent.cpp:382`); the component copy is seeded only on a stamped pawn (`:322-328`). In the hub an allocation would not reach the record. Fix: covered by 1; check any Blueprint caller.

**11. Live vitals on the field pawn and the record can differ.** A rest writes the record only (`PartySessionSubsystem.cpp:1077-1100`); the trial pawn's `CurrentHP` stays until the next rebuild. `TrialGate.cpp:152, 206` do the reverse (pawn only). Fix: after a rest, re-seed live pawns from the record (one loop), or have field UI read the record only.

**12. Skill picks live on the owned piece and in each loadout copy** (earlier audit's 5, still open, `InventoryComponent.cpp:1313-1318`). Not repeated; listed so the map is complete.

**13. `bPlayerControlled` and `bAISeat` are one fact kept in sync by hand, and the save stores the old one.** `Party.h:105-115`, `SaveSubsystem.cpp:1192`, restore derives the other. Fix: save `bAISeat`, drop the mirror at the next save bump.

**14. Wearers and owners both say whose gear it is.** Wearers are marked obsolete (`PoolSubsystem.h` B-321 notes) but still drive the run's gear pick (`PoolRunLayer.cpp:134`), the draw order (`InventoryComponent.cpp:1541-1545`) and are saved (`WoRSaveTypes.h:645`). Fix: have `TakeHomeGearOnRun` read each member's active loadout (after fix 3), then delete wearers.

**15. A run-only recruit's gear is picked from every owned weapon.** `PartyControlWidget.cpp:392-394` cycles `Pool->GetOwnedWeapons()`, including pieces another member's active loadout holds. The record (`RunWeapon`) and the owner map can name two users of one piece; only the obsolete clash code sorts it out. Fix: filter the cycle to unworn, non-chest pieces.

**16. Form and base form are stored on the subsystem and on the player state.** `CharacterFormSubsystem.h:120-122`, `WoRPlayerState.h:243-247`, synced by a delegate (`WoRPlayerState.cpp:157-161`). Works today; the player state copy is the replicated one. Fix: none needed now; note it in the doc so nobody writes the player state copy directly.

**17. Hidden counters are keyed by the asset; the pool is keyed by member.** `HiddenCounterSubsystem.h:45` against `PoolSubsystem.h` `GetCharacterKey(const FPartyMember&)`. Two copies of one character share counters but not gear. Fix: decide which is meant (design question), then use one key helper.

### Clean-up

**18. `InitializeFromPool` is debug-only and draws everyone's gear.** `InventoryComponent.cpp:2825`, sole caller `PoolSubsystem.cpp:1344`. It ignores owners and custom loadouts. Delete it, or point `wor.Pool.Draw` at `DrawSharedGear`.

**19. The pool's flat `OwnedSpells / OwnedAbilities`.** Written only by the debug populate and the save restore (`PoolSubsystem.cpp:1078, 1092`; `SaveSubsystem.cpp:1625, 1637`); real learning goes to `CharacterSkills`. Still saved and still read (`FieldSkillUnlockSubsystem.cpp:130`). Fold into `CharacterSkills`.

**20. The pool wallet.** `PoolGold .. PoolEssence` are never written or saved (marked obsolete in `PoolSubsystem.h`). Delete.

**21. `FPartyMember::PreferredColumn`** is only a "has a formation" marker now (`Party.h:99-103`), still saved (`WoRSaveTypes.h:111`). Replace with a flag at the next save bump.

**22. `UParty::bLetAIFinish`** is saved and shown (`SaveSubsystem.cpp:1181`, `PartyControlWidget.cpp:266`) but nothing in a fight reads it (`Party.h:238-243`).

**23. `FSavedRun::DropEncounters`** is no longer written (`WoRSaveTypes.h:299`). `FPoolGearClash`, `NoteClashes`, `Clashes` are written and never used for a decision (`InventoryComponent.cpp:2103`).

**24. `bIsBrokenDarkness` still carries a `SaveGame` idea in the doc**, but the save goes through the party record. `docs/Architecture/CharacterDataSystem.md` is stale here, and it still lists `bIsAIControlled`, the old BeginPlay steps, and no pool draw. `docs/Architecture/PartySystem.md` still shows the two-field `FPartyMember`.

**25. Apparel has no way in except a debug command and a save restore.** `AddApparelToPool` callers: `LoadoutRulesDebug.cpp:245`, `SaveSubsystem.cpp:1599`. Not a clash; owned apparel cannot change in play yet.

---

## 3. Rebuild survival list

"Hub->trial" and "battle->trial" both rebuild the field pawn; "trial->battle" builds stamped battle pawns.

| Fact | hub->trial | trial->battle | battle->trial | save/load | Proof |
|---|---|---|---|---|---|
| Identity, class, element | yes | yes | yes | yes | member holds the asset `Party.h:69`; `TrialGameMode.cpp:104`; `BattleGameMode.cpp:438` |
| Sub-stats, base levels | yes | yes | yes | yes | read from the asset |
| Head-start levels | yes (read by data) only on a stamped pawn: **no on the field** | yes | no on the field | yes | `CharacterDataComponent.cpp:322-328` |
| Run-earned levels, purchase count | reset on entry by design `TrialRunSubsystem.cpp:120` | yes | yes | **no** | `CharacterDataComponent.cpp:331-340`; `SaveSubsystem.cpp:1352-1400` |
| HP / EP | set to full on entry `TrialRunSubsystem.cpp:96` | yes | yes | yes in a run | `CharacterDataComponent.cpp:129-142`; `BattleGameMode.cpp:546`; `SaveSubsystem.cpp:1213` |
| Status effects, build-up | no | no | no | no | per fight, by design |
| Owned gear, tier, quality | yes | yes | yes | yes | `InventoryComponent.cpp:1547-1555, 2111`; `SaveSubsystem.cpp:1255+` |
| Crystals and wear on gear | yes | yes, but a stale carry can overwrite it (clash 4) | yes | yes | `InventoryComponent.cpp:1299-1345`; `LoadoutComponent.cpp:3237-3287` |
| Learned skills | yes | yes | yes | yes | `InventoryComponent.cpp:1601-1606` |
| Pouch items | yes | yes | yes | yes | `InventoryComponent.cpp:1610-1618` |
| Lore loadout edits (gear, skills in it) | **no** | **no** | **no** | **no** | `InventoryComponent.cpp:534, 2787-2790` |
| Custom loadouts | yes | yes | yes | yes | `InventoryComponent.cpp:556-559, 1566-1578`; `SaveSubsystem.cpp:846-851` |
| Active loadout | **no** | **no** | **no** | **no** | `InventoryComponent.cpp:2796` |
| Active weapon on a lore loadout | **no** | **no** | **no** | **no** | `FieldMovementAttack.cpp:389`; `InventoryComponent.cpp:534` |
| Active weapon on a custom loadout | only if that loadout is the asset default (it is not) : **no** | **no** | **no** | **no** | `FCombatLoadout.cpp:373`; `InventoryComponent.cpp:2796` |
| Skill / spell picks (on the owned piece) | yes | yes | yes | yes | `InventoryComponent.cpp:1313-1318`; `WoRSaveTypes.h:417,433` |
| Field attack pick | **no** | **no** | **no** | **no** (default is saved) | `FieldMovementAttack.cpp:365`; `FieldMovementComponent.cpp:122-127, 143-147` |
| Field skill | **no** | n/a | **no** | **no** | `FieldMovementComponent.cpp:213` |
| Guard, parry colour set on the field | **no** | **no** | **no** | **no** | `LoadoutComponent.cpp:3640-3644, 3719` |
| Guard, parry colour set in a fight | yes | yes | yes (record; the field pawn does not read it) | yes | `LoadoutComponent.cpp:3719-3723`; `SaveSubsystem.cpp:1203-1205` |
| Apparel owned | yes | yes | yes | yes | `SaveSubsystem.cpp:1268-1275` |
| Apparel worn | no (active loadout resets) | no | no | no | worn list is per custom loadout `FCombatLoadout.h:214`; index lost `InventoryComponent.cpp:2796` |
| Form, skin, outfit, form progress | yes | yes | yes | yes | `CharacterFormSubsystem.h:117-126`; `SaveSubsystem.cpp:853-857` |
| Broken Darkness flag | yes | yes | yes | yes | `CharacterDataComponent.cpp:158-162`; `BattleGameMode.cpp:555`; `WoRSaveTypes.h:133` |
| BD revert done on the field | **no** | **no** | **no** | **no** | `BrokenDarknessManager.cpp:440` |
| BD strain, absorption pool | cleared on entry | yes | yes | yes in a run | `CharacterDataComponent.cpp:187-191`; `PartySessionSubsystem.cpp:1055, 1152` |
| Curses, hidden counters, journal | yes | yes | yes | yes | GameInstance subsystems; `WoRSaveTypes.h:764, 794, 829` |
| Run memories | reset on entry | yes | yes | yes | `TrialRunSubsystem.cpp:123`; `SaveSubsystem.cpp:1366-1369` |
| Formation row, AI seat, AI difficulty | yes | yes | yes | yes | `Party.h:98-120`; `SaveSubsystem.cpp:1190-1193` |
| Wallet | yes | yes | yes | yes | `WoRPlayerState.cpp:114, 171, 194-197` |
| Who commands whom (co-op) | yes | yes | yes | no | `WoRPlayerState.cpp:203-209` |

---

**Not checked:**
- Nothing was built or run. Clashes 2, 3 and 4 are traced from code and need a PIE confirmation.
- How the hub pawn is spawned. No C++ hub game mode exists; if a Blueprint stamps or swaps the pawn, clash 1's hub half may differ. `PartyMemberId` is `BlueprintReadOnly`, so a Blueprint cannot set it.
- Anything in `.uasset` files: authored loadouts and `DefaultActiveLoadoutIndex` on the inventory assets, Blueprint callers of `SetHeadStartAllocation`, `SetGuardStyle`, `SetActiveLoadoutIndex`, widget Blueprints.
- Co-op guests: their own loadout copy, the checkpoint (`CoopCheckpoint.cpp`), and `URunStateComponent` beyond its field list.
- `GearRepair.cpp`: whether a repair also updates the owned piece, not just the carried copy.
- The load order at startup (party restored before the first pawn spawns) was assumed, not traced.
- Enemy pawns, the balance sim and the legacy test actors.
- `StatComposition.md`, `TierOnInstance.md`, `PerInstanceRollSystem.md` and `LoadoutSystem.md` were not read in full; the code was traced instead.

Skipped: per-line proof for every "yes" cell was limited to the line that does the carry. Risk to know: fix 1 (stamping field pawns) turns on the D-218 guard-from-pick code for the first time in real play, so expect guard and parry behaviour to change when it lands.
