# Visual Direction (Rematch model) — research and draft

**Status:** research and a draft one-pager for Crown's visual-direction session (pinned). Crown's lean (2026-10-28): **Rematch (Sloclap)**: stylised, painterly / impressionist, rich colour, vibrant light, no cel shading, no outlines. Nothing here is decided yet. Fab prices are from search snippets (the listing hosts couldn't be fetched): check each listing before buying.

---


Date: 2026-10-28. Web-only research, no code touched. **Everything here is a suggestion; Crown decides.**

**Caveat on sources.** In this cloud session fab.com, unrealengine.com, dev.epicgames.com, artstation.com, 80.lv and tomlooman.com could not be opened directly (hosts not allowed), so every Fab detail below comes from search-engine snippets of the listing pages. Prices are given only where a snippet showed one; everything else is marked "price: check listing". Fab prices move with sales, so re-check before buying anyway. Licence for every Fab item is assumed to be the Fab Standard License unless the snippet said "legacy UE Marketplace license".

---

## 1. How Rematch and Sifu get their look, and a UE 5.8 recipe

### What Sloclap has said (primary sources)
- **Rematch art blast (ArtStation, 2025)** — Art Director: the look is "impressionist": "the beauty of impressionism, rich colors, and vibrant light", built on three pillars (Embodiment, Connection, Tension) and a "Futuristic, Feel Good, Optimistic" vision. https://magazine.artstation.com/?p=62101
- **Rematch × Unreal Engine interview (Sept 2025)** — UE5's "lighting, material, and environment tools gave us a solid foundation to achieve the stylized, vibrant look"; they "adapted the lighting and rendering pipeline to match our art direction" while artists kept native tools. The Chinese version of the same page names **Virtual Shadow Maps and TSR** as the features that let them hold **60 fps across hardware**. Animation is hand-keyed, weight and momentum first. https://www.unrealengine.com/en-US/developer-interviews/rematch-reinvents-football-using-unreal-engine-5
- **Sifu, GDC 2023 — "Kung-fu and Speed Painting: Sifu's Art Direction and World Building Approach"** (Paul-Emile Boucher, Sloclap). The talk: Sifu's visuals come from Chinese martial arts, action cinema and *digital speed painting*; speed painting shaped "the game's volumes, shapes, and textures". https://gdcvault.com/play/1028808/Kung-fu-and-Speed-Painting (Vault login needed for the video)
- **Sifu, Edge interview (Dec 2021)** — Boucher: "Stylisation means that you are controlling the level of detail of your image." Absolver was low-poly; Sifu is "based on the 2D speedpaint style" — "Where Absolver's smallest detail was a large polygon, Sifu's smallest detail is a small brushstroke." https://www.pressreader.com/australia/edge/20211202/282673280591848
- **Second-hand technique** (an artist who rebuilt the Sifu look after the GDC talk): painterly **tiling brush-stroke masks painted in Photoshop, blended inside tiling shaders**, plus Substance Painter base maps. https://jakebenbow.artstation.com/projects/m8VKLv

Nothing in the sources calls Sifu/Rematch "cel-shaded" or mentions outlines; the look is **lit PBR with simplified, hand-painted inputs** — the opposite of a toon post-process. Crown's "not cel-shaded, not realistic" brief matches that.

### Comparable breakdowns worth reading
- **Season (Scavengers Studio) — "Creating the illustrative look"**: a painter's workflow in UE — choose a *local colour* (albedo) per material, then a *shadow tint* per object type, limit texture detail. https://www.unrealengine.com/en-US/tech-blog/creating-the-illustrative-look-of-season-using-unreal-engine
- **Stylized character shader in UE5** (80.lv) and **Ghibli-style shader in UE5** (80.lv): gradient-map colouring, soft shadow terminators, no outlines. https://80.lv/articles/setting-up-a-stylized-character-shader-with-unreal-engine-5 · https://80.lv/articles/ghibli-style-stylized-shader-made-in-unreal-engine-5/
- **Hand-painted 2D illustration → 3D env** (80.lv): paint greyscale, colour with gradient maps. https://80.lv/articles/turning-a-2d-illustration-into-a-3d-environment-with-a-hand-painted-look
- **Naughty Dog, GDC 2026 "Shader Like a Painter"**: oil-painting principles for character shaders (not Sifu, but the same philosophy). https://gdcvault.com/play/1035689/Shader-Like-a-Painter-Building
- **Expedition 33 (Sandfall)**: Lumen let 30 people get big-studio lighting, but switching to Lumen "completely ruined all of the carefully calibrated lighting" and had to be retuned; UE5 "can be a trap" because players then expect AAA everywhere. Lesson: pick Lumen/Lumen Lite *first*, then light. https://80.lv/articles/clair-obscur-expedition-33-dev-shares-how-ue5-shaped-the-game · https://creativebloq.com/3d/video-game-design/using-unreal-engine-5-can-be-a-trap-says-the-dev-behind-clair-obscur-expedition-33

### Solo-dev recipe for UE 5.8 (suggestion)
1. **Shading model: Default Lit, deferred.** No toon shader, no outline pass. Stylisation lives in the *inputs*, not a post filter.
2. **Textures = "speed paint" rules.** Albedo is flat local colour with painted gradients (light top/warm, dark bottom/cool); roughness mostly flat (0.6–0.9), tiny or no normal detail; **one tiling brush-stroke mask** (grayscale, 512–1024, painted or from a pack) multiplied into albedo/roughness at 2–3 scales so surfaces read as strokes up close. Few, small textures (Sifu's "control the level of detail").
3. **Master materials (3–4 total):** `M_Painted_Opaque` (albedo tint, gradient top/bottom, stroke mask, roughness scalar, optional emissive for crystals), `M_Painted_Foliage` (same + wind), `M_Crystal` (emissive colour from the element, fresnel rim, cheap fake refraction via refraction node or none), `M_Void` (for V — see §2b). Everything else is a Material Instance.
4. **Lighting:** one strong directional key with warm colour + sky light with cool tint (impressionist warm/cool split); **Lumen Lite** (new in 5.8, ~2× cheaper than Lumen HQ, built for 60 fps on consoles) with VSM + TSR, the exact trio Sloclap cites. Fall back to baked only if a map is static *and* profiling demands it; trial maps with weather that changes colour favour dynamic. Weather = change directional light colour/intensity, sky-light tint, fog colour and a global emissive tint toward the winning element.
5. **Post-process volume (global):** Filmic tonemapper, slightly raised **Saturation (1.1–1.2)** and **Contrast (~1.05)**, mild **Bloom** (intensity 0.3–0.5, large size) so crystals glow, **Vignette 0.3**, **no chromatic aberration**, **no film grain**, no SSAO (Lumen handles it), **Sharpen off**. Optional: a very light Kuwahara/painterly PP material at low radius as a *finishing* touch, never as the style (packs in §2a).
6. **Nanite:** on for static environment meshes from packs that ship Nanite-enabled (free LOD savings); **off for skeletal characters** (5.8 support still has gaps: no morph targets/cloth/translucency, little gain for a handful of characters). https://forums.unrealengine.com/t/nanite-skeletal-meh-lods/2810626 · https://medium.com/@sarah.hyperdense/understanding-nanite-when-to-use-it-and-when-to-skip-it-b7fcfadc3058
7. **VFX style:** flat-colour shapes with hard-ish edges, 2–3 tone gradients, element colour as the main hue, sparse particles, mesh-based bursts over sprite clouds (see §4).

---

## 2. Asset packs that fit (grouped)

### (a) Stylised shading / post-process (painterly, no outlines)
| Pack | Link | Price | UE | Why / notes |
|---|---|---|---|---|
| Stylized Arcane Process — painterly PP (oil / ink / watercolor), 1 master material, 5 presets, 70 settings, outlines optional per character | https://www.fab.com/listings/9d77553a-2ce3-4f83-b512-9c94c7cbddc3 · forum: https://forums.unrealengine.com/t/tiny-rpg-stylized-arcane-process-painterly-post-process-ink-oil-watercolor/2837154 | check listing | 5.x | Closest "Rematch finish" as a light PP layer; run at low strength. |
| Hand Painted — Stylized Post Process Pack (3 MIs: soft brush, depth-aware strokes, ink variant) | https://www.fab.com/listings/1812cfeb-7a15-48c9-b288-8b766898f748 | check listing | built 5.5, listed as 5.6/5.7/5.8-compatible | Explicitly 5.8-compatible; cheap way to test the look. |
| Kuwahara Painterly Stylization PP (classic / generalised / anisotropic Kuwahara) | https://www.fab.com/listings/46919baf-3914-44a1-a950-b81ee538d0ba | check listing | 5.x | Anisotropic Kuwahara is the "oil paint" filter; GPU cost scales with radius — keep radius small. |
| Hand-Drawn Stylized PP Material Pack (10 presets, line/texture) | https://www.fab.com/listings/ddcb832b-7825-4dbc-8cee-81a0f2665866 | check listing | 5.x | More ink than paint; lower priority. |
| Lightweight Toon Shader (No Post Processing), Anterill | https://www.fab.com/category/material/art-traditional | from $14.99 | 5.x | Only if Crown wants a soft two-tone on characters; no outlines. Not recommended for the Rematch brief. |
| Cel Shader Pro (cel + separate toggleable outline, Lumen/Nanite-compatible) | https://www.fab.com/listings/5e93dcf3-36d1-42bf-9e10-cb529d9f1f78 | check listing | 5.x | Listed for completeness; comic look, not painterly. |

**Suggestion:** buy one (Arcane Process or Hand Painted pack), use it at 20–40 % strength; do the real work in the master materials.

### (b) Characters: V's suit, faceless/void head, galaxy material
| Pack | Link | Price | UE | Why / notes |
|---|---|---|---|---|
| Male clothing collection rigged to UE5 Manny/Quinn (55 pieces, incl. formal/utility wear; MetaHuman-compatible) | https://www.fab.com/listings/252b167f-154f-401e-9dd0-5ffd4ba8e475 (seller Vu.: https://www.fab.com/sellers/Vu./about) | $9 per asset | 5.3+ | Pick a suit jacket + trousers + tie; retint to black in the painted master material; scale Manny 1.15× for V's 7 ft. Realistic meshes read fine once re-textured flat. |
| AAA Modular Sweat Outfit (Manny/Quinn + MetaHuman skeleton) | https://www.fab.com/listings/c2b7816e-3a9a-41fc-9739-0fa3ac028eed | check listing | 5.x | Example of the modular-clothing-on-Manny pattern for party members; not a suit. |
| Galaxy parallax material tutorial (UE5 community tutorial, free) | https://forums.unrealengine.com/t/community-tutorial-galaxy-parallax-material-in-ue5-tutorial/610906 | free | 5.x | Recipe for V's head/hands: 2–3 star/nebula layers offset by `BumpOffset` with view-dependent parallax, violet base, slow panner; apply to Manny's head/hand material slots with the face mesh kept but unlit-black under it. No pack needed. |
| Free VDB Nebula project (Pixel Lab VDB + material setup) | https://www.fab.com/listings/668b1f95-9db2-4a7b-b2d2-46a626890fcf | free | 5.3+ | Nebula texture source for the Void Realm sky / V's body swirl (bake a 2D slice; don't run volumetrics at runtime for 60 fps). |
| Procedural Volumetric Nebula Generator (Blueprint, uses Volumetric Cloud, 4 quality presets) | https://www.fab.com/listings/171266c0-78f3-4521-87ed-4783640ad49c | check listing | 5.x | Void Realm hub skybox; has Low preset for perf. |
| Gold tie/star, smoke wisps | — | — | — | Emissive gold MI + a 2-emitter Niagara ribbon (see Niagara Examples Pack §3). |

**Suggestion for V:** no "faceless" pack exists; the look is a material job: Manny head mesh with `M_Void` (Unlit or Default Lit with black base, emissive galaxy parallax, soft fresnel violet rim), Quinn/Manny body in black suit from the clothing pack, gold tie MI. All inside the existing mannequin skeleton so every animation pack still works.

### (c) Environments — Void Realm hub and elemental trial maps
| Pack | Link | Price | UE | Nanite/Lumen | Why / notes |
|---|---|---|---|---|---|
| Stylized Crystallis Expanse — Floating Isles Pack (AnalogUp Studios; 27 floating islands, shards, crystal chips; snaps to the Crystallis cliff/ground kits) | https://www.fab.com/listings/51edd359-1cce-4c3f-9d2e-c346cc64bb13 · pathways pack: https://forums.unrealengine.com/t/analogup-studios-llc-stylized-crystallis-expanse-pathways-pack/2742864 | check listing (family of small packs) | 5.x | check | Best match for the **Void Realm hub**: floating crystal islands; pale-cyan crystal matches the UI palette. |
| Modular Customizable Stylized Crystals (97 meshes: cubic/faceted/hex/gems, shared material) | https://www.fab.com/listings/4a62a8ee-71bb-4a85-a035-9fdcddafc42a | check listing | 5.x | — | "Crystals everywhere": one pack, retint per element via MI. |
| Crystal Cave (24 clusters, 8 large pieces, spline cave builder, Lumen 5.0+) | https://fab.com/listings/9ace93dd-6361-42ca-900b-b1e4d6c6d171 | check listing | 5.0+ | Lumen yes | Earth / dark trial caves. |
| Sharur's Stylized Bundle (6 packs, 740+ meshes: ruins, viking, dungeon, fantasy map, **volcanic "Disaster Island"** with fire-spread Blueprints, painterly forest/cave; Nanite+Lumen only, no static lighting) | https://www.fab.com/listings/a2948ed0-3d54-4313-85a4-7516b0479686 | check listing (bundle) | 5.x | Nanite + Lumen required | Fire + earth + forest (wind) trials in one buy; painterly already. Big download. |
| Stylized Floating Islands: 4 Biomes + Underwater (Spring, Jungle, Snow, Volcanic) | https://www.fab.com/listings/fc1837a8-696f-4b23-aea5-49b435f3fe91 | check listing | 5.x | check | Cheaper multi-biome alternative: ice (Snow), fire (Volcanic), water (Underwater), wind (Spring) islands. |
| Abyssal Hollow — Dark Fantasy Cave Pack V2 (bioluminescent cracks, runes) | https://www.fab.com/listings/509c4974-6bd7-4fac-baa2-38abcdf0569a | check listing | 5.x | check | Dark / void trial; note listing says made "using advanced AI techniques" — Crown may care. |
| Dark Fantasy Ruins (stylised gothic, + snowy variant; Nanite/Lumen 5.0+) | https://www.fab.com/listings/f750844d-0743-411c-8f1b-bb5057cd2fb8 | check listing | 5.0+ | yes | Dark / ice trials. |
| Weather Effects — Snow, Blizzard, Rain & Thunder (10 Niagara systems, stylised variants, runtime params, spline-driven) | https://www.fab.com/listings/bb2a4e45-a173-482c-a98a-2b0c4b332a83 | check listing | 5.x | — | **Weather shifting to the winning element**: expose density/colour per element; no sandstorm/fog included. |
| Synty POLYGON (Dungeon, Sci-Fi Worlds, etc.) | https://syntystore.com/products/polygon-dungeon-pack · https://syntystore.com/en-gb/products/polygon-sci-fi-worlds | Dungeon ~£73 on sale (reg. £146); Sci-Fi Worlds £489; sub from $30/mo; 5 seats per copy | 5.3+ (varies) | no Nanite | Low-poly flat-colour — **too chunky for the Rematch brief**; only as grey-box/placeholder. |
| KitBash3D (DieselPunk kit normally $245; kits $120–245; "Unlimited" sub, commercial rights) | https://digitalproduction.com/2025/11/07/kitbash3ds-dieselpunk-kit-free-till-18-nov-retro-futurism-ahoy/ · https://kitbash3d.com/a/blog/more-ways-to-build-big-updates-to-our-products-and-plans | $120–245/kit | 5.x, no LODs | huge poly counts (Nanite needed) | Realistic kitbash, not stylised; skip unless retextured. |
| Quixel Megascans | https://80.lv/articles/megascans-no-longer-free-after-2024/ · https://forums.unrealengine.com/t/access-legacy-megascans-on-bridge-and-quixel-com/2142664 | mostly paid since 2025; legacy library stays free if claimed before end-2024; a free tier remains on Fab | 5.x | Nanite | Realistic; only useful as *stroke-mask sources* (bake a rock albedo to greyscale and posterise). |

Gap: no Fab pack found for "void realm / cosmic / astral" by name; the hub is best built from **Crystallis floating isles + nebula skybox + crystals pack** with the dark-stone palette applied through MIs.

### (d) Stylised VFX (Niagara)
| Pack | Link | Price | UE | Why / notes |
|---|---|---|---|---|
| Unique Elemental Projectile VFX Pack — 11 stylised projectiles: fire, ice, lightning, **light, poison, stone, water, wind**; 44 Niagara systems (projectile + muzzle + channel + impact each), scalable | https://www.fab.com/listings/526c5afc-ee43-4d70-a4a7-483b32f2ed36 | check listing | 5.x | **Best single fit for the 9 elements** (map stone→Earth, poison or extra tint→Dark/Void). |
| Niagara Magic Projectiles — 30 base / 90 variants in 10 categories (Fire, Ice, Lightning, Nature, Water, Shadow Lands, Holy…) | https://www.fab.com/listings/4930337a-74e3-48dc-8784-bcce8b1e65f4 | check listing | 5.x | Broad alternative; Shadow Lands + Holy cover Dark/Light. |
| Magic Projectiles VFX: Ultimate Niagara (fire, ice, lightning, arcane, poison, holy, shadow; exposed colour/glow/size params) | https://fab.com/listings/71cc68f1-3626-4227-b61f-8650275f8892 | check listing | 5.x | Also fine; has a Unity twin (ignore). |
| Aura Stylized VFX (25+ Niagara auras, per-layer colour) | https://www.fab.com/sellers/StylizedFX | check listing | 5.x | Element auras / stance buffs. |
| Buff and Debuff VFX (DrGame, ~20 named status effects, colour/size params, auto fade) | https://www.fab.com/listings/631a313d-cf25-44d5-98f9-ba03b1881aad | check listing | 5.x | Status buildup / break markers. |
| 100 Buffs, Auras and Magic Circles VFX Pack (100 Niagara systems, CPU+GPU) | https://docs.unrealengine.com/marketplace/ko/product/100-buffs-auras-and-magic-circles-vfx-pack | check listing | listed 4.26–5.4 | Older; may need 5.8 re-save. |
| Stylized FX Pack (42 Niagara: slashes, hits, splashes, 7 buffs) | https://www.fab.com/listings/5ba9dd9a-9b08-4406-b426-eb07b7b21fac | check listing | 5.x | Melee hits for Generic class. |
| Buff VFX (kcStudio, 20 buffs) | https://kcstudioasia.itch.io/unreal-engine-niagara-buff-vfx | check (itch) | 5.0–5.6 | Cheap filler. |
| Magic Water: Elemental VFX Pack (28 water effects) | https://www.fab.com/listings/6561c587-98bd-4243-ac6d-9fca17ef93dc | check listing | 5.x | Water element depth. |
| Niagara Examples Pack (Epic, free, 50+ systems incl. player/weapon buffs-debuffs, lightning, hit dissolves, impacts, mist) | https://www.unrealengine.com/en-US/news/discover-over-50-free-niagara-systems-ready-to-use-in-unreal-engine-5-7 | **free** | 5.7 (opens in 5.8) | Start here; recolour to element hues. |

### (e) Animation packs (UE5 mannequin, turn-based friendly: in-place)
| Pack | Link | Price | Why / notes |
|---|---|---|---|
| Spell/Magic Combat Animation V1 — 227 anims on UE5 mannequin, **in-place and root-motion versions**, 17 attacks + reaction per attack, hits, idles, dodges | https://www.fab.com/listings/32822325-1ac2-4417-aa76-4563b1d9a438 | check listing (seller bundle exists) | Best fit for Caster/Resonator: in-place + per-attack reactions = turn-based ready. Matches the project's In-Place default. |
| Caster — Animation Pack (Enchanting Mage Casting Collection, V2 for 5.1+, casting loops + transitions, Epic skeleton) | https://www.fab.com/listings/32f1a245-a2bb-4b0c-8086-a657dcf47ded | check listing | Casting loops for channel/charge states. |
| Elemental Mage — Motion Capture (25 mocap: conjure/attack with four elements; UE5/UE4/Unity/Mixamo rigs) | https://www.fab.com/listings/78980143-2b8f-436c-8815-2937004b22e2 · itch: https://rapamotion.itch.io/elemental-mage | $19.99+ on itch | Cheap, element-flavoured casts. |
| Feminine Mage — Motion Capture (25: elemental casting, heal, shield, charge-up) | https://fab.com/s/7c295810f5ac | check listing | Confirm skeleton before buying. |
| Sword and Shield Animations (V2: 84 anims, UE5 mannequin; dodges, rolls, **defense/guard**, attacks, hit reacts, deaths) | https://www.fab.com/listings/4d04590f-f714-4df4-866d-479ddd829dc4 | check listing (9CG sword packs "from $49.99") | Guard / parry / dodge for Generic class. |
| RPG Animations (bow etc.; Attack A–E, Dodge, Skill, Hit, Roll, defence idle; UE5 mannequin) | https://www.fab.com/listings/4060ede0-1a0b-4e48-95d2-61726d87f27d | check listing | Ranged archetype if needed. |
| Free: Lyra / Stack O Bot / Epic mannequin anims (§3) | — | free | Idle/locomotion filler. |

---

## 2f. The galaxy material as the Void Realm's template (Crown, 2026-10-28)

V's galaxy parallax material (the free tutorial above) is also **the template for the Void Realm**: V *is* the realm (`HubAndStory.md` §2), so the hub wears his skin.
- One master material, two looks by parameters: **skin** on V's head and hands; **dark stone with the galaxy showing through** cracks, seams and crystal faces on the hub's ground and walls.
- The sky is the same nebula, so V, ground and sky share one look; the slow parallax drift makes the realm feel alive.
- Hidden clue (pillar 10, suggestion): the galaxy shows through more, or matches V's exact pattern more closely, as memories are collected.
- Cheap: one parallax material reused everywhere; no runtime volumetrics.

## 3. Free / Epic-provided options
- **Fab free drops**: three (sometimes more) commercial packs free every two weeks since Nov 2024, kept forever once claimed; mostly Fab Standard License, a few Personal-only. Check Fab each fortnight. https://www.cgchannel.com/2024/11/epic-games-to-share-new-free-assets-on-fab-every-two-weeks · https://gamefromscratch.com/massive-changes-to-the-unreal-engine-fab-marketplace-giveaways/
- **Epic free content index** (Paragon, Infinity Blade, samples; filter Price > Free on Fab). https://dev.epicgames.com/documentation/unreal-engine/free-epic-games-content-for-unreal-engine
- **Paragon** (39 AAA characters, 1,500+ env assets, custom AnimBPs; free incl. commercial, **UE-only licence**). Realistic-stylised; good for enemy bosses if run through the painted material; UE4 skeleton → IK Retargeter to Manny. https://www.unrealengine.com/en-US/blog/final-round-of-free-paragon-assets-released
- **Infinity Blade packs** (Village etc.): not migrated to Fab, Vault-only via Epic Launcher, UE-only licence; stylised-fantasy props still usable. https://www.unrealengine.com/marketplace/en-US/product/infinity-blade-village
- **Stack O Bot** (rebuilt for 5.6; clean stylised-ish robot, physics puzzles) — https://www.fab.com/listings/b4dfff49-0e7d-4c4b-a6c5-8a0315831c9c ; **City Sample** (realistic, heavy; useful only to study Nanite/Lumen/World Partition) — https://fab.com/s/5e8f5eda64d8 ; **Lyra** (mannequins, animation setup, UI patterns) — https://dev.epicgames.com/documentation/en-us/unreal-engine/sample-game-projects-for-unreal-engine
- **Niagara Examples Pack 5.7** (free, see §2d). **Free VDB Nebula** (see §2b).
- **Megascans**: claim legacy library if Crown already did before 2025; otherwise only the free tier on Fab. https://www.cgchannel.com/2024/10/epic-games-has-made-megascans-free-to-all-but-only-until-the-end-of-2024/
- **KitBash3D** occasionally gives a kit away on Fab (DieselPunk, Nov 2025) — watch for a stylised one. https://digitalproduction.com/2025/11/07/kitbash3ds-dieselpunk-kit-free-till-18-nov-retro-futurism-ahoy/

---

## 3b. Humble Bundle and bundle sales (check before buying singles)

Humble runs an Unreal asset bundle every month or two; keys redeem on Fab. Fab's own free drops come every two weeks.
- **Live (June 2026):** [65-Item Bigger Better Unreal Engine Bundle](https://slickdeals.net/f/19615476-65-item-bigger-better-unreal-engine-assets-bundle-12-000-assets-22): 12,000+ assets (Meshingun, Dekogon), stylised / fantasy / modular themes, tiers $5 / $15 / $22, redeem by May 2027.
- **Past bundles that may repeat:** [Legendary Game Dev Environments (Leartes)](https://gamefromscratch.com/legendary-game-dev-environments-humble-bundle/): stylised fantasy, steampunk, VFX; [Unreal Ultimate Game Dev Assets (Sidearm)](https://gamefromscratch.com/unreal-engine-ultimate-game-dev-assets-humble-bundle/): stylised dungeons, forest, Niagara magic projectiles; [Unreal Ultra Asset Megapack](https://gamefromscratch.com/unreal-ultra-asset-megapack-humble-bundle/): stylised crypt, DEKO fog / rain / water VFX; [Hivemind Unreal + Unity](https://gamefromscratch.com/massive-unreal-unity-hivemind-asset-humble-bundle/): includes Stylized Nature.
- Habit: check Humble and the Fab free drops before each single-pack purchase.

## 4. Performance notes for this style (60 fps @ 1080p, mid-range PC; consoles later)
- **The style is cheap by construction**: few/small textures, flat roughness, no normal maps → low bandwidth; it's the lighting and VFX that cost.
- **Lumen Lite + VSM + TSR** (5.8): Lumen Lite "preserves much of the visual impact at a significantly lower GPU cost", ~2× faster than Lumen HQ, built for 60 fps on Switch 2-class hardware; still marked Beta. Sloclap used VSM + TSR for its 60 fps. https://gamefromscratch.com/unreal-engine-5-8-released/ · https://www.guru3d.com/story/unreal-engine-58-debuts-lumen-lite-and-productionready-megalights/ · https://tomlooman.com/unreal-engine-5-8-performance-highlights/
- **Lumen can hit 60 fps on mid-range with tuning** (scalability: Lumen quality "High" not "Epic", software ray tracing, lower final-gather, fewer reflections passes). https://www.strayspark.studio/blog/ue5-lumen-optimization-60fps
- **Expedition 33 warning**: decide GI method before lighting; switching later wipes tuned looks.
- **Avoid:** large translucent/additive sprite clouds (overdraw is the no. 1 stylised-VFX killer — prefer mesh particles, ribbons, and opaque-masked cards); Kuwahara/painterly PP at large radius (full-screen cost, scales with resolution); Volumetric Cloud nebulae at runtime (bake to skybox); volumetric fog beyond one low-res layer; many dynamic lights per spell (use one light per element burst, short lifetime; MegaLights in 5.8 is production-ready if Crown wants many small lights). 
- **Nanite:** on for pack environment meshes already authored for it (Sharur's, Dark Fantasy Ruins), off for characters and small props; measure in Crown's scene (`stat gpu`, Unreal Insights) rather than assume. https://forums.unrealengine.com/t/nanite-skeletal-meh-lods/2810626
- **Scalability plan:** ship three presets (Low: Lumen off → skylight + SSAO, VSM off → CSM, TSR 50 %; Medium: Lumen Lite, VSM, TSR 67 %; High: Lumen HQ, VSM, TSR 100 %). Keep the painterly PP strength as a scalability-driven parameter (0 on Low). Do the milestone 60 fps check (D-068) on Medium.
- **Turn-based advantage:** nothing needs to tick; VFX can be spawned on events and pooled; camera is predictable so Lumen screen-space artefacts are easy to hide.

---

## 5. Proposed visual-direction one-pager (draft for Crown's session)

**Pillars (suggested)**
1. **Painted, not rendered** — every surface reads as brushstrokes (Sifu's "smallest detail is a small brushstroke"); simplified volumes, bold silhouettes, controlled detail.
2. **Lit by colour** — impressionist warm key / cool fill, rich saturated hues, vibrant light (Rematch). Light carries the element: the winning side's element tints the world.
3. **Dark stone and steel, lit by crystal** — the UI's existing rule extended to the world: neutral dark-stone grounds and platinum/steel structure so the nine element colours and crystals pop.
4. **Readable at a glance** — Hades' lesson: warm = player agency/opportunity, cool/dark = threat; element colour on anything that matters in combat, nothing else gets saturated.

**Palette (ties to UI)**
- Base world: dark stone (near-black blue-grey), steel/platinum structure, pale cyan crystal light (the UI palette, unchanged).
- Element colours: the existing 9 element colours are the *only* strong hues in a scene; they live on crystals, spells, status bars, weather tint, and V's violet/gold accents.
- Shadows are tinted toward the sky colour (cool violet-blue in the Void Realm), never grey-black; highlights warm.
- V: black suit (near-black with cool sheen), violet galaxy body, gold tie/star as the single warm accent, smoke wisps in desaturated violet.

**Shading recipe (short)**: Default Lit, deferred; flat painted albedo + gradient + tiling stroke mask; flat roughness; no normals/outlines; Lumen Lite + VSM + TSR; filmic tonemap, saturation 1.1–1.2, mild bloom, vignette; optional painterly PP at ≤40 %; Nanite only on authored env meshes.

**Reference games**: Rematch (overall target), Sifu (brushstroke textures, controlled detail), Expedition 33 (lighting mood, Lumen in a small team), Genshin (clean readable colour on characters), Hades (palette as gameplay information).

**Best pack picks (suggested, 7) and rough cost** — prices unverified except where noted; expect **~US$150–350 total** at Fab list prices, less during sales:
1. Hand Painted — Stylized Post Process Pack *or* Stylized Arcane Process (PP finish) — est. $15–40.
2. Modular Customizable Stylized Crystals (97 meshes) — est. $20–40.
3. Stylized Crystallis Expanse — Floating Isles (+ ground/pathways packs as needed) — est. $20–60 for 2–3 packs.
4. Sharur's Stylized Bundle (fire/earth/forest trials, painterly, Nanite+Lumen) *or* Stylized Floating Islands: 4 Biomes — est. $50–120.
5. Unique Elemental Projectile VFX Pack (9-element projectiles) — est. $30–60.
6. Spell/Magic Combat Animation V1 (227 in-place caster anims) — est. $30–60.
7. Male clothing collection (suit pieces for V) — $9 per piece (~$30–45 for jacket, trousers, shirt, tie).
Free on top: Niagara Examples Pack, Free VDB Nebula, Paragon (bosses), fortnightly Fab drops, Weather Effects pack if it appears in a drop (else est. $20–40).

**Licence reminder**: Fab Standard License allows commercial use, modification and shipping in a game; forbids redistributing the asset itself; Personal tier under US$100k revenue/12 months, Professional above, same rights. Some migrated items still carry the legacy UE Marketplace licence (UE-only). Paragon/Infinity Blade are UE-only. Synty is per-seat. https://dev.epicgames.com/documentation/en-us/fab/licenses-and-pricing-in-fab · https://www.fab.com/cla

**Next step suggestion**: Crown opens the 7 listings, confirms prices/versions, buys 1 + 2 + 5 first (cheapest proof of the look), and a 1-day "look-dev" session builds `M_Painted_Opaque`, `M_Crystal`, `M_Void` and one PPV on a grey-box trial map before any environment pack is bought.
