# PIE — Weapon glow and weapon trail

**Branch:** `feature/weapon-glow-trail`.
**Already proven by tests:** `WoR.Presentation.WeaponTrail` (9 tests: which weapons trail, tint, fallback window, long axis, element / Physical / Raw / Ice glow lookup, the level table, the trail's level-0 tint) and the full suite.
**Setup:** rebuild, open the editor. The trail slot and the default glows are empty, so checks 4 to 15 need the editor steps below first.

## Editor steps (art, before checks 4 to 15)

1. **Trail system:** make (or pick) one Niagara ribbon system. It reads the user parameters `TrailStart` and `TrailEnd` (Vector, the weapon mesh's local space, so set the system to Local Space) and `ElementColor` (Linear Color). Fill `WeaponTrailVFX` on `/Game/Data/Combat/DA_ImpactVisualDefaults`. `PlainTrailColor` is the uninfused tint; `TrailWindUpSeconds` (0.25) is how early a fallback trail starts.
2. **Sockets (optional):** on a weapon mesh, add sockets `TrailStart` (near the guard) and `TrailEnd` (the tip). Without them the ribbon runs along the mesh's longest axis.
3. **Default glows:** on the same asset, add a row per element to `ElementInfusionGlowVFX` (at least Fire and Water), and fill `PhysicalInfusionGlowVFX`, `RawInfusionGlowVFX` and `IceInfusionGlowVFX`. Each glow reads `ElementColor` (brightened by the level) and `Scale_All` (Float, the level's size), and may read `InfusionLevel` (Int, 0/1/2). `InfusionLevelVisuals` is the one level table: row 1 is level 1 (intensity 1.0, size 1.25), row 2 level 2 (intensity 1.5, size 1.5).
4. **Windows (optional):** on an attack or weapon-skill montage, add the notify state **Weapon Trail** over the swing. Without one the trail runs from the first Impact's wind-up to the last Impact.

| # | Do this | You should see | Result |
|---|---|---|---|
| 1 | Use a weapon whose infusion display is set to Weapon; attack with an elemental infusion (L1) | The glow sits on the weapon itself, not floating at the hand | [ ] |
| 2 | Set the infusion display to Body or Aura and attack infused again | The glow is still on the weapon (an attack's infusion always rides the weapon) | [ ] |
| 3 | Swap weapon (two-hander, then a left-hand piece), then attack infused | The glow follows the new weapon each time; a spell's infusion still shows where the display says | [ ] |
| 4 | Console: `wor.Debug.WeaponTrail` | The log line names the weapon, that it has a trail, the slot's system and the ribbon ends (sockets or long axis); a plain trail plays for a second | [ ] |
| 5 | Console: `wor.Debug.WeaponTrail Fire` | The same trail, tinted fire | [ ] |
| 6 | Attack with a sword on a montage with no Weapon Trail window | A trail from just before the hit to the hit; no trail lingers after | [ ] |
| 7 | Add a Weapon Trail window to that montage and attack again | The trail plays exactly over the window | [ ] |
| 8 | Attack infused with Fire, then Physical, then Raw | Fire tints the trail; Physical and Raw show the plain trail | [ ] |
| 9 | A bow or gun attack | No trail; hit effects unchanged | [ ] |
| 10 | Use an infusion display with no VFX system (or clear it), attack infused with Fire, then Water | Fire's default glow, then Water's, on the weapon; a display with its own system still plays that one | [ ] |
| 11 | A sword, then a hammer, then a spear attack | The same trail system on all three | [ ] |
| 12 | With a display with no VFX system, attack infused Physical (a source with no element), then Raw | Physical's default glow, then Raw's, on the weapon (before this change: no glow) | [ ] |
| 13 | Attack infused with Fire at L1, then at L2 | The L2 glow is bigger and brighter; the same for Physical and Raw | [ ] |
| 14 | Watch the trail on those two Fire attacks | The L2 trail is brighter than the L1 trail; an uninfused trail is the plain colour | [ ] |
| 15 | Cast a Water spell marked Deals Ice, infused from a Water source; then a plain Water spell | Ice's default glow in Ice's colour, then Water's; the next pick in the menu starts without the Ice glow | [ ] |

**Known limit:** in co-op a client sees a trail only from an authored window, and plain (the fallback and the element are read on the server).

**Notes / what went wrong:**
-
