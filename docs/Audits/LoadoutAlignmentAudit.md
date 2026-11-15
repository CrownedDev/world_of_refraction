# Loadout alignment audit (independent, 2026-11-15)

Read-only audit by a fresh agent, not the writer who built the system. Question: is the weapons, loadout and equipment code aligned with the current UI design (D-210, D-211, D-215 to D-219), and what is still done the old way? Everything is traced from source, not run. Findings 3, 4 and 5 most need a PIE confirmation.

## Verdict

Clean-up alone is not enough, but a full rewrite is not needed either. The "one structure" in the design does not exist in code yet: the stored loadout is still the old primary / secondary / ring-list shape with a `bShowPrimary` flag, and the v2 entries are a view drawn over it (`FCombatLoadout.h:121-173`, `LoadoutV2Model.h:48-57`). Five things can go wrong in play today (1-5); the rest is old paths still live and dead code.

## Must fix

**1. The wheel's pick, the ACTIVE weapon, the guard and the parry are four separate stored facts.** D-218 says the wheel's pick is the ACTIVE weapon and drives the guard and parry. The pick only stores an index and hides or shows the mesh; no battle code reads it. Case: a Generic with Sword (ACTIVE) and Hammer picks Hammer on the wheel: the field swing is the Hammer's, the loadout screen still says Sword, the fight starts with the Sword, the parry uses the old saved priority. Fix: on a weapon pick set the active weapon through the loadout component; make `GetGuardStyle` and the parry inputs read the pick (B-503s, in progress). `FieldMovementAttack.cpp:340-354, 372-381`; `FCombatLoadout.cpp:727-733`; `LoadoutComponent.cpp:3646-3653, 3707-3711`; `ParryVisuals.cpp:139-142`; `WoRSaveTypes.h:155-178`.

**2. The Skills tab shows a different list from the one the fight uses, until the first edit.** The tab always shows the new slot list; the fight uses the old merge while `bSkillSlotsActive` is false. Case A: an F-tier weapon with three built-ins shows one slot, but the fight offers all three; replace one skill and the fight drops to one. Case B: a new dual pair shows both weapons' built-ins; the fight offers only the main hand's. Fix: make `GetSkillSlotAbilities` always use the slot list (seed picks once from the authored list), then delete the merge. `LoadoutSkillsModel.cpp:162`; `FWeaponLoadoutEntry.cpp:110-123, 150-190`; `FCombatCapabilities.cpp:60-62`; `WeaponSetRules.cpp:77-91`.

**3. Gear and skill changes to a character's own (lore) loadout are lost.** Lore loadouts are rebuilt from the asset on every spawn and never saved; the v2 Weapons and Skills tabs edit them with no check (only apparel refuses). Case: add a Hammer entry to Garrick's own loadout, travel to another map, and it is gone; a crystal socketed in the same visit stays, because that is written to the owned piece. Fix: refuse with the apparel tab's rule, or copy to a custom loadout on first edit. `LoadoutV2ScreenWidget.cpp:1757, 2454, 2556`; `LoadoutSkillsModel.cpp:376`; guard only at `LoadoutApparelModel.cpp:40`; `FCombatLoadout.h:112-115`; `InventoryComponent.cpp:2909-2916`.

**4. Which loadout is in use is not kept.** The active loadout index is reset from the asset's default on every pool draw; no save or pool field holds it. Case: set custom "Bruiser" as the one in use; after a map change or a load the character is back on its default loadout. Fix: keep the index beside the custom loadouts in the pool and the save (also needed by D-219 / B-503t). `InventoryComponent.cpp:2796, 2917-2919`; `PoolSubsystem.h:150-156`; `WoRSaveTypes.h:591-602`; `LoadoutComponent.cpp:258`.

**5. A weapon's skill picks exist on the owned piece and in every loadout's copy, and the copy can win.** Case: the same sword is in loadouts A and B; edit its skills in A, then in B; A and B now show different lists for one sword; after a reload both take whichever was written last. Fix: picks live on the owned piece only; copies always take the owned list. `InventoryComponent.cpp:1047-1085, 1299-1352`; `LoadoutSkillsModel.cpp:384`.

## Should fix

**6. The character's own evolution cannot sit beside gear.** D-210 makes it one extra line; the code needs the first entry free, because it is stored as the primary slot. Case: a Generic with two weapons gets "Free the first entry for the evolution." Fix: give the evolution its own field (needs the stored shape changed, see 7). `LoadoutV2Model.cpp:578-588, 736-740`; `FCombatLoadout.h:126-157`.

**7. The slot list is a mirror, and about ten places walk the old fields by hand, each with its own rules.** Case: the memory walkers skip the off-hand weapon; the field has its own copy of the walk. Fix: one shared walker now; later make the slot list the stored truth with an active index. `FCombatLoadout.cpp:680-709, 746-782`; `InventoryComponent.cpp:1030-1040`; `FieldMovementAttack.cpp:160-183`; `LoadoutV2Model.cpp:456-481`; `LoadoutRules.cpp:400-421`; `MemoryConditions.cpp:220-229`; `MemoryUnlock.cpp:258-265`; `LoadoutComponent.cpp:3193-3201`; `DurabilityHeaderWidget.cpp:83-100`; `BattleHUDView.cpp:376`.

**8. Switch Weapon is hidden for a primary with slot type None.** The active-weapon rule accepts Weapon or None; the command menu asks for Weapon only. Fix: `HasPrimaryWeapon() && HasSecondaryWeapon()`. `FCombatCapabilities.cpp:225-229` against `FCombatLoadout.h:302-305`.

**9. Form progress reads the old ability list.** It reads the mirror's `AssignedAbilities`, so skills slotted on the v2 Skills tab and built-ins do not count. Fix: read `GetSkillSlotAbilities`. `CharacterFormSubsystem.cpp:50-66`; `FLoadoutSlot.h:46, 68`.

**10. An off-hand weapon's sockets are ignored without a word.** Case: a sword with a Garnet goes in the off hand; the Garnet does nothing and costs no cells; nothing warns. Compare also always says the cells change by 0. Fix: refuse or warn on a socketed off hand; compute the compare's cells. `FWeaponLoadoutEntry.h:37-41`; `WeaponSetRules.cpp:15-34`; `LoadoutV2Model.cpp:521-523, 674`; `LoadoutV2ScreenWidget.cpp:2196-2199, 2354-2357`.

**11. The save stores the old shape only.** It holds `bShowPrimary`, `PrimarySlotType`, primary and secondary fields and the old ability lists, written through two conversions; no migration (any save below the latest version is wiped). Fix: drop the two ability lists after 2; bump the version when the shape changes. `WoRSaveTypes.h:511-589`; `SaveSubsystem.cpp:260-333`; `FCombatLoadout.cpp:373-397`; `WoRSaveGame.h:15-18`; `WoRSaveGame.cpp:87`.

**12. The vendor's compare reads the primary weapon, not the ACTIVE one.** `VendorScreenModel.cpp:115`. Fix: `GetActiveWeaponLoadout`.

**13. The put-away weapon is not refreshed when the loadout changes.** Set only on a pick and at map start. Fix: also call it on the loadout's gear-changed event. `FieldMovementAttack.cpp:335, 353, 369`; `FieldMovementComponent.cpp:148`.

**14. Debug commands write loadout state through side paths.** `wor.Loadout.ReplaceSkill`, `RestoreSkill`, `ReplaceSpell`, `RestoreSpell` write the primary weapon directly and skip the rules check; `wor.Loadout.WearApparel` writes the apparel array directly; `wor.Loadout.TryAbility` and the balance sim write the obsolete ability list. Fix: call `WoRLoadoutSkills::ApplySkill` or `TryChangeGear`. `LoadoutRulesDebug.cpp:230-256, 323-362, 365-, 429-, 465-, 571`; `BalanceSimRunner.cpp:342`.

**15. `OverrideAttack` is read in four places but only a test sets it, and it is not saved.** Fix: delete it, or give it a UI and a save field. `FWeaponLoadoutEntry.h:53-57`; `LoadoutComponent.cpp:1423, 1438, 2464`; `FieldMovementAttack.cpp:96`; `FieldAttackSpec.cpp:299`.

**16. Tests that cover old paths only.** `LoadoutPicksSpec.cpp:87-140`; `SharedGearSpec.cpp` (`GetSelectableWeapons`); `HubHUDSpec.cpp` (`BuildFieldAttackChip`); `RingFusionStoneSpec.cpp` and the fusion cases in `GearAttachSpec.cpp`; seven specs still set `AssignedAbilities`. No spec calls `SelectFieldAttack` or `CycleFieldAttack`, so the pick-to-store path is untested.

## Clean-up

**17. Old pick functions.** `TryAssignWeaponSpell`, `TryAssignInnateSpell` (no caller); `TryAssignRingSpell` (a spec only); `TryAssignWeaponAbility` (a debug command and a spec). `LoadoutComponent.cpp:1169-1213`.

**18. Obsolete parts of the weapon entry.** The locked / customizable ability helpers (`FWeaponLoadoutEntry.cpp:43, 329, 385`); `bUseWeaponParryAnimation`, `GetParryMontage`, `ShouldUseWeaponParry` (`LoadoutComponent.cpp:3562, 3602-3605`); `AssignedAbilities` goes after 2.

**19. Obsolete loadout getters.** `GetPrimarySlotSpells`, `GetSecondarySlotSpells`, `GetActiveSlotSpells` (only `CombatOrchestratorDebug.cpp:543, 931`); `IsShowingPrimary`, `GetActivePrimaryEvolutionCrystal`, `CreateAndConfigureLoadout` (no C++ caller); `SwitchToNextUsableRing` (a Blueprint node only); `HasRingInSecondary` (a spec only); `GetPrimarySlotType` (one caller, `CharacterDataComponent.cpp:721`).

**20. Item-slot and shared-gear leftovers.** `ApplyAutoEquip` (`FCombatLoadout.h:98-106`); `MaxSavedLoadouts` (`InventoryComponent.h:126`); `GetSelectableWeapons` / `Rings` (`InventoryComponent.h:308`); `FPoolGearClash` (`PoolSubsystem.h:158`). Keep `ItemSlots` while enemies use it; `IsWornByOtherMemberOf` is still called.

**21. Field leftovers marked obsolete.** `FieldSkillTypes.h:333, 383, 404, 422`; `HubHUDModel.h:39, 125`; `HubHUDWidget.h:31`.

**22. Retired fusion and random-roll code on equipment.** `EquipmentDataBase.h:58-70, 135, 179-218, 388`; `FFusionId.h`; `FFusionAttachment.h`; `InventoryComponent.cpp:135, 339-345`; `CrystalEffectTable.h:671`; `CrystalTypeHelpers.h:69-86` (the D-124 sweep).

**23. Comments that describe the old design.** `FCombatLoadout.h:9-12, 64-68`; `FLoadoutSlot.h:3-4`; `FWeaponLoadoutEntry.h:25-26`; `FSavedLoadout.h:3`.

**24. Two UI models stacked.** The v2 screen turns its entry back into the first screen's row to socket, set active and remove. Fold them once the stored shape changes. `LoadoutV2ScreenWidget.cpp:1577-1582, 2420, 2485`; `LoadoutV2Model.cpp:940`.

**25. The weapon-spells switch and fields.** `wor.Loadout.WeaponSpells` and the weapon's spell lists are kept only for a PIE comparison. `WeaponSpellRules.h:1-9`.

## Not checked

- Nothing was run: no build, no tests, no PIE.
- Anything in `.uasset` files: Blueprint callers of the functions in 17-19, widget Blueprints, authored loadouts on the inventory data assets, default crystals on weapon and ring assets, input assets for the wheel.
- Co-op paths beyond the field index and guard calls; enemy and AI use of the old fields.
- The apparel tab and the saved-loadouts list beyond their lore checks.
- The audit ran on a checkout from before D-219 landed.
