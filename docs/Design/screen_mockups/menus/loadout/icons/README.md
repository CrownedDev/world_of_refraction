# Loadout icons trial (2026-11-14)

The deckbuilder's icons on the loadout screen as PIE builds it today (B-484: chip rows, Cells used, rows with ACTIVE at the right end, + Add weapon, details box, key caps). Layout copied from `LoadoutV2ScreenWidget.cpp`. Stat values are placeholders. Run `python make.py` (needs the deckbuilder clone at `D:/github/wor-deckbuilder`).

- `1_weapons_icons.png`: the Weapons tab. **Pick:** a crystal is its element badge **instead of** the "(Garnet)" text, right after the name. The name stays short and the badge says the element at a glance. The details box names the crystal ("Socket 1 Garnet"). An empty socket is the socket badge.
- `2_add_gear_crystal_icons.png`: the Add gear window on Augment socket 1. Each crystal row has its element badge and tier badge. The box shows the highlighted crystal with an effect icon on each line.

## Where each icon comes from

Library = `D:/github/wor-deckbuilder/wor.game/assets/ui/library/` (made from PONETI masters, see the deckbuilder's `docs/design/UI_Asset_Picks.md`, "Crystal masters").

| Icon | File |
|---|---|
| Crystal in a socket (per element) | `badges/element_<fire, water, lightning, wind, earth, darkness>.png` |
| Empty socket | `badges/element_socket.png` |
| Tier badge (C, D, B, E, S) | `badges/tier_<c, d, b, e, s>.png` |
| Fire Amp +1 / Water Amp +1 | `icons/effect_amp_up_down_<fire, water>.png` |
| Burn (damage over time) | `icons/effect_damage_over_time_fire.png` |
| Survive a KO | `icons/effect_revive.png` |
| Water resistance | `icons/effect_resistance_up_down_water.png` |
| Weapon icon (sword) | `vendor/PONETI/ClassicFantasyRPG_UI/ARTWORKS/UIicons/Colored/13_Sword.png` (under `assets/ui/`) |

Not used: `badges/gem_<tier>.png` (a gem without a letter; the `tier_` hexagon reads better at 22 px).
