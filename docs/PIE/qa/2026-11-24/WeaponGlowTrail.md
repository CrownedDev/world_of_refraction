# Weapon glow and trail (`feature/weapon-glow-trail`): agent QA (2026-11-24)

Heads played: `873573acb`, `657a57b0a`, `cbe8aeb4e`. Temp checkout without the vendor art.

- `873573acb`: did not build from a clean checkout (`MIN_PLAY_RATE` defined twice in one unity blob). Fixed at `657a57b0a` (uses `FCombatCameraRules::MinPlayRate`).
- `657a57b0a`: clean build OK. `wor.Debug.WeaponTrail` in a fight said "no actor showing a weapon": the sword mesh is a vendor asset outside git, so the art-less copy has none. Not a code fault.
- `cbe8aeb4e`: Crown's changes (one universal trail; per-element default glows plus Physical, Raw and Ice; level table for size and brightness). Writer suite 2542 passed, 0 failed, clean build.

Not checked: every in-play look (no trail or glow art yet, and no weapon mesh in the art-less copy). Crown's checks: `docs/PIE/WeaponGlowTrail.md`.
