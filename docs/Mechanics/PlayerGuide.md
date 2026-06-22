# Player Guide — Mechanics Index

Skim-sheet of everything a player **does, sees, or decides**, grouped by experience. One line each: name — status — what it does — link to its dedicated doc.

**Status tags:**
- **[Live]** — playable now; a player can reach and feel it.
- **[Built · No UI]** — coded and working, but no equip / socket / fusion / reroll screen exists yet, so it's unreachable without hand-configured loadouts. *These double as the equip/socket/fusion/reroll backlog.*
- **[Stub]** — not implemented (placeholder value, no real logic).

> Source of truth is shipped code. Where a value differs from a design doc, the code wins.

---

## 1. Taking a turn
- **Action menu** — [Live] — pick Attack / Ability / Spell / Item / Defend / Switch-weapon each turn; options gated by your loadout. *(no dedicated doc)*
- **Abilities & attacks** — [Live] — weapon-bound physical actions; an *attack* is the weapon's free basic, an *ability* is slotted; non-elemental unless infused. → [`Abilities.md`](./Combat/Abilities.md)
- **Targeting** — [Live] — TargetType (Enemy/Ally/Self) × TargetCount (Single/All) + HitCount multi-hit (per-impact defense, per-hit buildup). → [`Targeting.md`](./Combat/Targeting.md)
- **Energy (EP) cost** — [Live] — every action shows and spends EP; too little EP blocks it. → [`ResourcePools.md`](./Combat/ResourcePools.md)
- **Commit costs (HP / wear)** — [Live] — infused actions also pay HP or durability when committed. → [`Infusion.md`](./Magic/Infusion.md), [`DurabilityWear.md`](./Gear/DurabilityWear.md)
- **Turn order / speed** — [Live] — see who acts next (~10 ahead, bonus turns pinned); faster combatants act more often. → [`TurnOrder.md`](./Combat/TurnOrder.md)
- **Luck & crit** — [Live] — Luck drives your crit chance (5%→50%→100%) + wear-skip + gambling; CritDamage sets crit size. → [`Luck.md`](./Combat/Luck.md)

## 2. Defending
- **Defense window** — [Live] — a short real-time window opens when an attack lands; react or eat full damage. → [`DefenseResolution.md`](./Combat/DefenseResolution.md)
- **Block / Parry / Dodge** — [Live] — Block = **50% reduction** (no timing); Parry = **70% reduction + 30% reflected** (tight); Dodge = **full avoid on timing alone** (no attack-size gate). → [`DefenseResolution.md`](./Combat/DefenseResolution.md)
- **Per-impact + perfect timing** — [Live] — each hit of a multi-hit attack is defended separately; perfect timing is rewarded. → [`DefenseResolution.md`](./Combat/DefenseResolution.md)
- **Defense difficulty** — [Live] — some attacks have tighter windows (attacker speed vs your Reflex). → [`DefenseResolution.md`](./Combat/DefenseResolution.md)
- **Defend (brace)** — [Live] — a turn-action that buffs your Defense +50% for the round (0 EP); distinct from the reactive window. → [`Defend.md`](./Combat/Defend.md)

## 3. Positioning
- **Row modifiers** — [Live] — Front (+5% dmg / −5% def), Middle (neutral), Back (−5% dmg / +5% def). → [`CombatGrid.md`](./Combat/CombatGrid.md)
- **Auto-formation** — [Live] — startup drops one actor per column. → [`CombatGrid.md`](./Combat/CombatGrid.md)

## 4. Building my character
- **Classes & innate element** — [Live] — Generic / Refractor / Resonator + a fixed innate element; gates spells / abilities / dual-wield / rings. → [`Classes.md`](./Character/Classes.md)
- **Stats (pillars & substats)** — [Built · No UI] — Mind/Body/Spirit (0–7) drive the 14 substats and gate skill tiers; no in-game allocation screen. → [`Stats.md`](./Character/Stats.md)
- **Stat caps / gear ceilings (+50% solo → +100% gear)** — [Live] — always-on balance shape (invisible by design). → [`Stats.md`](./Character/Stats.md)

## 5. Gearing up
- **Loadout** — [Built · No UI] — what you bring (primary + rings + spells/abilities/items) under a slot-cost budget; no loadout UI. → [`Loadout.md`](./Gear/Loadout.md)
- **Inventory** — [Built · No UI] — what you own (gear instances, crystals, items + quantities); no inventory UI. → [`Inventory.md`](./Gear/Inventory.md)
- **Equipment slot scaling (tier → skill capacity)** — [Built · No UI] — gear tier gates how many abilities/spells it holds (F=1 … A/S=6); enforced, but no equip UI to change it. → [`EquipmentSlots.md`](./Gear/EquipmentSlots.md)
- **Locked skills (conjured gear)** — [Live] — fixed presets that occupy slots and can't be removed. → [`EquipmentSlots.md`](./Gear/EquipmentSlots.md)
- **Weapons (types / wield / physical-type → status)** — [Live] / [Built · No UI] — physical type (Slash/Pierce/Impact) decides Bleed/Armor-break/Stun pressure [Live]; 11 types & wield modes, but equip/selection has no UI. → [`Weapons.md`](./Gear/Weapons.md)
- **Rings (Resonator)** — [Built · No UI] — element-bearing spell-carriers (element from the ring's crystal); equipped rings fit a slot-cost budget (no fixed count, no per-element cap); no ring-equip UI. → [`Rings.md`](./Gear/Rings.md)
- **Per-instance stat rolls** — [Built · No UI] — gear rolls fresh stats at pickup (good/bad drops); no view/compare/equip UI. → [`PerInstanceRolls.md`](./Gear/PerInstanceRolls.md)

## 6. The crystal economy
- **Crystal spells (cast / wear / break)** — [Live] — an attached elemental crystal grants spells; casting wears it and it can break. → [`Items/Crystals.md`](./Items/Crystals.md)
- **Socketing / attaching** — [Built · No UI] — moving crystals / fusions / evolutions into slots is data/Blueprint-only; no socket screen. → [`Socketing.md`](./Gear/Socketing.md)
- **Evolution crystals (pillar mod + spells)** — [Live] / [Built · No UI] — pillar % modifier applies live once slotted; socketing/rolls have no UI. → [`Items/EvolutionCrystals.md`](./Items/EvolutionCrystals.md)
- **Augment stones — consumable use** — [Live] — use a stone to buff/debuff/heal a target instantly. → [`Items/AugmentStones.md`](./Items/AugmentStones.md)
- **Augment stones — attach form** — [Built · No UI] — permanent weapon stat bonus; no attach UI. → [`Items/AugmentStones.md`](./Items/AugmentStones.md)
- **Elemental fusions** — [Built · No UI] — combine a gem-half + augment-half (+ a chosen bonus substat); wear/break coded, but no fusion-creation UI. → [`Items/FusionStones.md`](./Items/FusionStones.md)
- **Durability / wear / break** — [Live] — catalysts lose durability per cast and break at 0 (HUD bar, 25% warning). → [`DurabilityWear.md`](./Gear/DurabilityWear.md)
- **Per-battle repair (+10)** — [Live] — durability auto-restores between combats. → [`DurabilityWear.md`](./Gear/DurabilityWear.md)
- **Catalyst-tier vs action-tier wear** — [Live] — a lower-tier catalyst wears faster (+3/gap; ≥4-tier gap can shatter in one cast). → [`DurabilityWear.md`](./Gear/DurabilityWear.md)
- **Reroll economy / pools** — [Built · No UI] — instances carry reroll pools, but the fill-and-reroll trigger and its UI are unbuilt. → [`RerollEconomy.md`](./Gear/RerollEconomy.md)

## 7. Magic
- **Spells (cast / target / school)** — [Live] — pick a spell, choose target(s) (TargetType × Count, multi-hit); element is authored per spell (Generic = inherit from source). → [`Spells.md`](./Magic/Spells.md)
- **Infusion (hold-to-charge L0/L1/L2)** — [Live] — hold to grow size/damage/status for higher EP+HP cost (HP cost can be lethal at finalize). → [`Infusion.md`](./Magic/Infusion.md)
- **Generic-spell element inheritance** — [Live] — a Generic spell takes the element of its source ("Ball" → "Fire Ball"). → [`GenericSpells.md`](./Magic/GenericSpells.md)
- **Spell sources & consequences** — [Live] — origin sets the cost: Innate safe / Ring break-check / Weapon wear / Item consumed / Evolution. → [`SpellSources.md`](./Magic/SpellSources.md)
- **Spell pool budget** — [Live] — up to 6 spells per pool, capped by a weight budget; mastery discounts cost (breadth vs power). → [`SpellPoolBudget.md`](./Magic/SpellPoolBudget.md)
- **Spell schools** — [Live] — spells belong to schools (Destruction / Enhancement / Restoration / …) that pool separately. → [`SpellSchools.md`](./Magic/SpellSchools.md)
- **Tier-gap (action vs channel tier)** — [Live] — a tier-mismatched catalyst boosts or penalizes damage/status/effect/cost (no in-combat tooltip). → [`TierGap.md`](./Scaling/TierGap.md)
- **Requirement gap (pillar level vs skill req)** — [Built · No UI] — under/over-levelling scales that pillar's substats; **the scaling is applied (single-counted, per-pillar)** but no in-combat tooltip surfaces it. → [`RequirementGap.md`](./Scaling/RequirementGap.md)
- **Tier power (own-tier strength)** — [Live] — higher-tier skills hit and cost more (F=×1.0 … S=×4.8). → [`TierPower.md`](./Scaling/TierPower.md)
- **Elemental DAMAGE weakness/resistance** — **[Stub]** — NOT implemented; `ElementMultiplier` is hardcoded `1.0` (`DamageCalculator.cpp:208`). Element does **not** change raw damage. *(Contrast bucket 8 — element DOES affect status buildup.)* → [`Elements.md`](./Magic/Elements.md)

## 8. Status & survival
- **Status bar (buildup → proc)** — [Live] — a per-element bar fills with hits; at the cap it triggers a status effect. → [`StatusBuildup.md`](./Status/StatusBuildup.md)
- **DOTs (Burn / Chill / Bleed / …)** — [Live] — a capped bar applies damage-over-time for several turns. → [`StatusEffects.md`](./Status/StatusEffects.md)
- **Status debuffs (Stun / Armor-break / Slow / Silence / …)** — [Live] — bar caps trigger action or stat restrictions. → [`StatusEffects.md`](./Status/StatusEffects.md)
- **Buffs** — [Live] — spells/abilities grant temporary stat boosts to allies. → [`SkillEffects.md`](./Status/SkillEffects.md)
- **Elemental STATUS resistance** — [Live] — Spirit + gear + class-innate slow incoming buildup, per element & physical type. *(This is the element interaction that IS live — unlike damage in bucket 7.)* → [`Resistance.md`](./Status/Resistance.md)
- **Status gauge reduction** — [Live] — some effects clear a flat amount of a target's bar. → [`StatusEffects.md`](./Status/StatusEffects.md)
- **Last Stand / Defy Death (Sapphire)** — [Live] — a ward that survives a lethal blow (≈50% HP) or revives a dead ally (≈30%), consuming a charge. → [`Items/Crystals.md`](./Items/Crystals.md)
- **Healing (Healing Stone / drain)** — [Live] — restore HP via stone, drain, or heal effects. → [`Items/AugmentStones.md`](./Items/AugmentStones.md)
- **Heal Block** — [Live] — while active, all healing on the target returns 0. → [`StatusEffects.md`](./Status/StatusEffects.md)
- **Charges (multi-life ward / item tiers)** — [Live] — wards can hold multiple charges; consumables come in F–S tiers. → [`Items/Crystals.md`](./Items/Crystals.md)

## 9. Class identities
- **Generic** — [Live] — dual-wield, weapon-ability-only, tanky vs physical / soft vs magic, casts from equipped crystals. → [`Archetypes/Generic.md`](./Archetypes/Generic.md)
- **Refractor** — [Live] — innate-element caster and team weather leader; spell-pool budget grows with mastery. → [`Archetypes/Refractor.md`](./Archetypes/Refractor.md)
- **Reality** — [Live] — any-element caster, +10% stats, hard-counters Broken Darkness. → [`Archetypes/Reality.md`](./Archetypes/Reality.md)
- **Broken Darkness** — [Live] — absorb-to-charge EP, one rotating element pool, forbidden Light/Void cost HP, hybrid "Dark X" spells. → [`Archetypes/BrokenDarkness.md`](./Archetypes/BrokenDarkness.md)
- **Resonator** — [Live] — ring-swap flexibility, fragile (rings wear/break), soft vs physical; EP dormant until a weapon/evolution is equipped. → [`Archetypes/Resonator.md`](./Archetypes/Resonator.md)

## 10. The world reacting
- **Weather** — [Built · No UI] — sky shifts with the team leader's HP% (leadership = highest-world-stat teammate, folded in); BP consumer unverified, confirm in PIE. → [`Weather.md`](./World/Weather.md)
- **AI opponents** — [Live] — difficulty (Easy→Expert) drives think-time, targeting, survival/cleanse/offense priority, real-time defense, infusion, and Emerald lethal-DoT setups. → [`EnemyAI.md`](./World/EnemyAI.md)

---

> **Footnote — non-mechanics (omitted from this guide):**
> - **Stances** — legacy cosmetic only (`StanceData`); not a gameplay mechanic.
> - **Row movement** — the row-movement API (`MoveActorToRow` / `PushActorBack` / `PullActorForward` / `SwapActorPositions`) exists and is `BlueprintCallable`, but has no C++ caller (command-menu / AI / effect) — not reachable in play today.
