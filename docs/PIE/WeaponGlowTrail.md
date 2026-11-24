# PIE — Weapon glow and weapon trail

**Branch:** `feature/weapon-glow-trail`.
**Already proven by tests:** `WoR.Presentation.WeaponTrail` (5 tests: which weapons trail, tint, fallback window, long axis, default glow lookup) and the full suite.
**Setup:** rebuild, open the editor. The trail slot and the default glows are empty, so checks 4 to 11 need the editor steps below first.

## Editor steps (art, before checks 4 to 11)

1. **Trail system:** make (or pick) one Niagara ribbon system. It reads the user parameters `TrailStart` and `TrailEnd` (Vector, the weapon mesh's local space, so set the system to Local Space) and `ElementColor` (Linear Color). Fill `WeaponTrailVFX` on `/Game/Data/Combat/DA_ImpactVisualDefaults`. `PlainTrailColor` is the uninfused tint; `TrailWindUpSeconds` (0.25) is how early a fallback trail starts.
2. **Sockets (optional):** on a weapon mesh, add sockets `TrailStart` (near the guard) and `TrailEnd` (the tip). Without them the ribbon runs along the mesh's longest axis.
3. **Default glows:** on the same asset, add a row per element to `ElementInfusionGlowVFX` (at least Fire and Water). Each glow reads `ElementColor` like any infusion glow.
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

**Known limit:** in co-op a client sees a trail only from an authored window, and plain (the fallback and the element are read on the server).

**Notes / what went wrong:**
-
