# Combat Tier

## Overview
Every combatant has a Combat Tier, F to S (D-118). It is the average tier of the gear that counts and of the skills the loadout offers, rounded down (Q-325, Destiny 2's Power). On an enemy it sets the tier-power HP (B-224b) and its level drops (B-223). On a player it is shown only: no stats, and the pillars are unchanged.

## Architecture
- `WoRCombatTier` (`Public/Combat/Tier/CombatTier.h`), free functions:
  - `GetAverageTier(Tiers, Fallback)` rounds down, and returns Fallback when nothing counts.
  - `GatherTiers(Actor, Out)` collects: `ULoadoutComponent::ForEachContributingGear` (the adapter in use and the weapon in hand, at instance tiers), the primary evolution, and `GetAvailableAbilities` / `GetAvailableSpells`.
  - `GetCombatTier(Actor)` falls back to `UCharacterData::EnemyTier`, then F.
  - `GetCombatTierText(Tier)` returns "Tier C".
- No cache. The tier is computed when read, so it follows every loadout change.

## Integration Points
- `TrialGate::GetEnemyTierHealthMultiplier` reads it at spawn, inside `ApplyEnemyRankHealth`. The enemy's loadout must be set up before that call.
- `ACombatOrchestrator::OnCombatantDied` reads it for the level-drop tier.
- `UCharacterPanelWidget::ApplyStaticText` shows it on a live panel's class line. The panel refreshes on `OnLoadoutChanged` and `OnActiveWeaponChanged`.
- Debug: `UCombatTierDebug::GetCombatTierString` / `PrintCombatTiers`, and the console command `wor.Combat.PrintTiers`.

## Known Limitations / TODOs
- Each item and each skill counts once, with no weighting. Armour and rings that aren't the adapter in use don't count, following B-306's rule.
- The roster view (out of a fight) shows no tier, because it has no actor.
- An enemy whose gear is given after `ApplyEnemyRankHealth` keeps the HP of its earlier tier.

- **Enemy content (D-118, desktop note):** an enemy asset with seeded gear or skills reads its Combat Tier from them; a seed instance's `Tier` defaults to F, so a tiered enemy asset loses the HP its `EnemyTier` used to give until its seed gear carries a tier. `EnemyTier` is the fallback only (no gear, no skills).
- **D-120 / B-327:** enemy HP = own × rank × Combat Tier; the zone scales nothing. The Combat Tier is the only tier in that product.
- **Panel label:** refreshes on `OnActiveWeaponChanged` / `OnLoadoutChanged`; an in-place tier upgrade of the worn weapon (same asset) does not redraw it until the next loadout change.

## Changelog
| Date | Change | Branch |
|---|---|---|
| 2026-11-03 | B-325 (D-118, Q-325): created. Enemies' HP and level drops read it; panels show it; spec `WoR.Combat.CombatTier`. Unbuilt (cloud). | feature/B-325-combat-tier |
