# Visuals and gameplay plan

**Crown, 2026-11-09** (desktop operator session). This is the next phase, now that the systems work is merged. It covers how the game gets its UI, animations, spells and effects. Feel is judged against `FeelGuide.md`, the look against `VisualDirection.md`, and the skill timing marks against `SkillPresentationPipeline.md`.

## Order

1. **Placeholder UI, rewritten for the game as it is now.** Many systems have changed since the old HUD: the six schools, apparel, curses, Amp, resistance by type, co-op seats and rest spots. Build each screen fresh as plain C++ widgets (simple buttons and text, no `.uasset`), **as the vertical slice needs it**, not all up front. The list of screens is `UI_ScreenInventory.md`. CommonUI replaces them later (D-024).
2. **Animations, spells and abilities from scratch, one at a time.** Start with the one-fight vertical slice (`FeelGuide.md` "How it's used"): entry, turns, hits, defence, victory, return. Each new animation, spell or ability gets its own job, so Crown sees every piece as it goes in.
3. **Claude adds the marks.** Once Crown is happy with an animation, Claude adds its cast and impact notifies (`SkillPresentationPipeline.md`), VFX and sounds, then checks them with the notify validator (B-368).
4. **Feel refinement.** Crown plays the slice and gives notes, and each note becomes a `FeelGuide.md` row or a value. Feel checks on the PIE sheets wait for this phase.

## Effects and parry rules (D-170)

| Thing | Rule |
|---|---|
| **Weapon attack VFX** | Fixed per weapon; it doesn't change. |
| **Adapter and spell-cast VFX** | Each adapter type has a default (rings are the default). Adapters have **no** per-item override; casters use the character's own setting (below). |
| **Generic with both a weapon and an adapter** | The class preference decides which VFX shows. |
| **Parry animation** | There are two guards. The **unarmed guard** is the body, used by adapter users, Refractors and Generic by default, and it can be customised per character. The **weapon guard** is that weapon type's own and can't be customised. A per-character setting picks which is used: with a weapon equipped, either parry with it or put it away and parry unarmed. Equipping a weapon switches to the weapon guard by default. |
| **Landing a parry (VFX)** | All weapons share the default sparks. **Only a weapon with a crystal attached** adds its element's own parry effect (e.g. a magic circle). |
| **Parry timing** | The same window for both guards; the choice is looks only. Reference: Expedition 33's parry plays the same for every character, while each one's wind-up looks different. |

References: Elden Ring and Dark Souls (the parry animation follows the item held, one per shield or weapon type); Sekiro (one readable deflect for everyone).

## Jobs

B-391 to B-395 in `Backlog.md`.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-11-09 | Created from Crown's plan (UI first, animations from scratch, VFX and parry rules) | docs/visual-gameplay-plan |
