# PIE - Equipment tab: weapon entry edit (Crown's pass, 2026-11-18)

**Branch:** `feature/equip-edit-polish` (built and tested on the desktop 2026-11-18). **Built unseen:** not played yet.

**What changed:**
- **A new main hand takes the off hand off when the two no longer go together.** Before, picking a weapon that can't be dual wielded (the Claymore), or one of another type (an axe over two swords), was refused while an off hand was set. Now the main hand takes it and the off hand comes off, back to the inventory. The line says so before you pick ("the off hand comes off"), and a message names the piece after.
- **A shield still only goes in the off hand,** and its line now says so: "A shield goes in the off hand." It used to say "Twin only: needs two, one item here." when no off hand was set. The shield data is right as it is (see below).
- **Under a main hand that can't be paired** (the Claymore), every Off hand line now reads "Claymore takes no off hand." in place of "This weapon can't be dual wielded."
- **Each weapon line shows its type after its name:** "Simple Sword · Sword". The details box keeps the bare name (its type is already on the quiet line above).
- **A filled slot leads with its piece:** the edit view's row reads "Arming Sword" with its tier badge, and the slot's word ("Main hand") is the quiet text beside it. An empty slot leads with the slot's word, as before.
- **A long list scrolls inside its panel** and is cut at the panel's edge. The mouse wheel and the keys move the highlight, and the list follows it.

**About the shields' data:** a shield's Handling is "Dual only". For a shield that means it pairs and never stands alone, which is what a shield is, so the Round Shield, Plain Shield and Guard Shield need no change. Only the wording was wrong.

**Not built (part 2, reported to the brain):** slots and choices side by side, the hover tooltip, icons in place of text: the approved mockup `menus/loadout/v2/2_add_gear_weapon.png`.

**Editor steps:** none. `wor.Debug.GiveTestWeapons` in the hub gives the weapons used below.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Equipment tab, A on a weapon entry, open Main hand | Every weapon line reads name, a dot, its type ("Claymore · Greatsword"); the box on the right shows the bare name | [ ] |
| 2 | Build a pair: a sword in the main hand and a second sword in the off hand, Y Confirm | The entry is the two swords | [ ] |
| 3 | A on that entry, Main hand: look at the Claymore's line | Not greyed; its line ends "the off hand comes off" | [ ] |
| 4 | Pick the Claymore | Main hand: Claymore. Off hand: empty. A message says the second sword came off the off hand | [ ] |
| 5 | Y Confirm, then the Inventory tab | The entry is the Claymore alone; the second sword is in the inventory | [ ] |
| 6 | Two swords again; A on the entry, Main hand, pick an axe | The axe is the main hand, the off hand is empty, with the same message | [ ] |
| 7 | Two swords again; Main hand, pick a third sword | The main hand changes; the off hand stays | [ ] |
| 8 | Sword and shield; Main hand, pick another sword | The shield stays in the off hand | [ ] |
| 9 | Sword and shield; Main hand, pick the Claymore | The shield comes off (the Claymore can't be paired) | [ ] |
| 10 | Main hand list, with and without an off hand set: look at a shield's line | Greyed: "A shield goes in the off hand." both times (never "Twin only"); picking it changes nothing | [ ] |
| 11 | Claymore as the main hand; open Off hand | Every line, the shields too, reads "Claymore takes no off hand." | [ ] |
| 12 | A sword as the main hand; open Off hand | The shields are open to pick; a second sword too | [ ] |
| 13 | The edit view with a main hand set | Its row leads with the weapon's name and tier badge, "Main hand" quiet beside it; the empty Off hand row leads with "Off hand" | [ ] |
| 14 | The same with a crystal in Augment socket 1 | That row leads with the crystal's name and badge, "Augment socket 1" beside it; the empty socket 2 row leads with "Augment socket 2" | [ ] |
| 15 | With 12 or more weapons owned, open Main hand | The list stops at the bottom of its panel; no row is drawn outside it | [ ] |
| 16 | Press Down past the last line you can see, then Up to the top | The list moves so the lit line is always in view | [ ] |
| 17 | The mouse wheel over the list | The highlight moves a line at a time and the list follows; a click lights the line under the mouse | [ ] |
| 18 | The other tabs (Inventory, Skills, Apparel) and a short list | They look as before: the panel is only as tall as its lines | [ ] |
| 19 | A long weapon name in the Main hand list | The name and type fit the line; note here if one is cut off with "..." | [ ] |
