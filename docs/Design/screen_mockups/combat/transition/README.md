# Battle transition storyboards (B-490, D-212)

Crown, 2026-11-14: touching an enemy plays a short designed transition with no loading screen, and any loading hides behind it. Below are three options, each about one second long. Each sheet (`A_sheet.png`, `B_sheet.png`, `C_sheet.png`) shows the frames with the timing, sound and loading notes beside them. The single frames are `<option>_<n>_<name>.png`, and `strike_variants.png` shows who struck first. The field and battle stage are stand-ins. Run `python make.py` to redraw them.

## Picked: option D, the target for B-500 to B-500d (Crown, D-212)

`D_sheet.png` (frames `D_1_hit` to `D_5_battle`) mixes A and B. A's shatter stays **in the world**, as in Expedition 33's encounter: the world is always on screen, never a black card. B's prism plays inside it:
1. **0.00–0.08 s:** the hit flashes white, the camera starts a small push in, and the first cracks appear.
2. **0.08–0.25 s:** the light runs through the cracks, white at the hit and splitting into the spectrum further out.
3. **0.25–0.45 s:** the frame breaks into **thick, lit, bevelled shards**, each catching a colour. The world behind stays visible, out of focus.
4. **0.45–0.80 s, hold:** coloured shards drift toward the camera, the near ones soft (depth of field). This frame holds as long as the load needs. The world behind is the captured frame, drawn above the level.
5. **0.80–1.00 s:** the shards blow past the camera with motion blur onto the battle stage, with bloom on the last edges.

The look needs **real 3D shards with bloom, motion blur, depth of field and a small camera push**. If the rough version still looks flat, a Fab glass or crystal shatter pack becomes the base (D-212). The effect starts at contact, and the 2-second join window runs underneath it, unchanged. Build steps:
- **B-500:** rough shards and motion
- **B-500b:** the prism light
- **B-500c:** sound and timing
- **B-500d:** first strike and ambush (`strike_variants.png`)

Options A to C below are the first round, kept for reference.

## How it works today (dev, c925ab4fb)

1. **Contact.** `UEncounterComponent` (`Combat/Encounter/EncounterComponent.cpp:244`) starts a **2-second join window** (`JoinWindowSeconds = 2.0f`, `EncounterComponent.h:60`) so nearby teams can join the fight. It also works out who struck first (`EFirstStrike`: Party, Enemy or None; a field hit always gives Party, B-214f).
2. **Hand-off.** `OnJoinWindowExpired` → `UTrialRunSubsystem::EnterEncounter` (`Trial/TrialRunSubsystem.cpp:298`) → `TravelTo` (`:987`).
3. **A separate map.** The fight loads the trial's own `EncounterLevel`: `OpenLevelBySoftObjectPtr` when playing alone, and `ServerTravel` with seamless travel in co-op (`BattleGameMode.cpp:60`). The trial map, its actors and any widget on it are destroyed.
4. **No effect at all.** Nothing covers the change. There's no fade, widget, Niagara effect or loading-screen module, so the player sees a 2-second pause and then a hard cut through a load hitch. That cut is the part Crown doesn't like.

**What that means for the effect:** loading a whole map blocks the game for a moment, and the trial world is gone afterwards. So the effect needs:
- **A pose that can hold still** while the load blocks, so a frozen frame looks intended (frame 4 in A and B, frame 3 in C).
- **Something drawn above the level**, not a widget in the trial world, so it survives the map change. Either a viewport-level overlay holds the hold frame, or the engine's loading-screen hook shows that same frame. Both keep the picture on screen without anything that looks like a loading screen.
- **A load that starts early.** Reading the battle map in the background during the 2-second join window leaves only the swap itself for the effect to cover.

## The options

| | A. Refraction shatter | B. Prism flash | C. Facet turn |
|---|---|---|---|
| What you see | The frozen frame cracks from the touch point, breaks into tinted shards that bend the colour, the shards fall away over a violet void, and the stage is there | A white flash, the picture splits into its colours, folds into a beam, a prism fans it into a spectrum, then the beam opens onto the stage | Crystal facets spread from the touch point, the whole screen turns faceted, then the facets flip over to show the stage |
| Frame where the load hides | 4: shards drifting over the void | 4: the prism beam (the easiest still frame to hold) | 3: the faceted screen |
| Feels like | Persona 5's shatter, in crystal | Final Fantasy's flash and swirl, made from light | Its own thing: the game's hex and tier-badge shapes |
| Sound | Tink, then splinter, then a crash with a choir swell, then a shimmer loop, then the battle downbeat | Ping, then a rising whoosh that snaps shut, then a crystal hum loop, then the downbeat | Tink, then a spreading crackle, then a shimmer loop, then quick flip ticks, then the downbeat |
| Build cost | Medium: a captured last frame cut into about 100 shards (a material or a Niagara mesh burst) | Low: a post-process colour split and squash, plus one UMG beam picture | Medium: a hex-mask material on the captured frame, with the flip wave as a material parameter |
| Risk | The best known of the three, and close to Persona 5 if overdone | Simple, but less distinct, and the flash can be harsh | A long flip wave can feel slow; the faceted screen is busier to look at |

## Who struck first (cheap on any option)

`EFirstStrike` already reaches the fight, so the effect can read it. `strike_variants.png` shows this on option A:
- **Player first strike:** violet cracks start at the leader's blade, and a "FIRST STRIKE" banner shows on arrival, with a bright rising sting.
- **Plain clash:** white cracks from the touch point, and no banner.
- **Surprise attack:** red cracks come in from the screen edge behind the player, the frame jolts and tints red, and an "AMBUSH" banner shows, with a low thud instead of the tink.
Only the colour, the start point, a small jolt, the banner and one sound change. The frames themselves stay the same.

## First-round recommendation: A, the refraction shatter (Crown then picked D, above)

- **It is the game's name.** Breaking the field into light-bending shards says "refraction", and the shards match the crystal badges on the loadout screen.
- **The shatter is a proven pattern.** Persona 5's glass-shatter battle start is one of its most praised UI moments: fast, readable and on-brand, and players called it stylish rather than slow. Final Fantasy's battle swirl and Expedition 33's swirl into its battle arena do the same job: the world breaks, so the change of map reads as a story beat rather than a load. A gives that beat without copying any one of them (crystal shards over a violet void, not red glass).
- **It hides the load well.** Shards drifting over a void look natural when frozen, so a hitch can't be seen.
- **Fallback:** B is the cheapest to build, and its still beam frame holds a long load best. Pick B if A's cost is too high for the first build.

## Open points for the build job (not decided here)

- **The 2-second join window** is dead time before the effect starts, which is most of what feels slow today. Two choices: start the clash frame at contact and let the join window run under it, or shorten the window. That's a rules choice for Crown or the build job.
- **Co-op:** seamless travel has its own transition map, and every player should see the hold frame until all of them have arrived.
- **Spinner (D-212):** a small turning crystal shows only if the load passes about 2 seconds (shown in each hold frame).
