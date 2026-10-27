# Field Abilities and Traversal

**Status:** researched, choices open (pinned in `DesignDecisions.md` D-099, Crown 2026-10-27). Decided so far is in D-099 (command menu on the map, optional field uses, field-only skills, EP cost with no regen, 1 turn = 1 minute, time runs, free aim, a field hit starts the fight with first strike). The research is in the appendix: some sources are player opinions, guides or search snippets, and a few points are inference (marked).

## Crown's ideas to explore

- Movement through field skills: flight, speed, etc.
- Evolutions changing how a character moves (e.g. a character whose evolution lets them run at lightning speed).

## Simplified model (Crown, 2026-10-27)

- **Everyone:** fast free sprint.
- **One active field skill per character**, set by level / source, costing EP.
- **Enhanced Sprint** (a field skill): wall-running.
- **Evolutions:** a small always-on movement bonus, and they augment the field skill to a degree.
- **Sources:** a field skill can use a source; a crystal source still loses durability.
- **Dropped:** field uses on battle skills, field-only slots. Choices 3 (movement types) and 7 (slots) below shrink to "which field skills exist".
- **Enhanced Sprint:** faster than the normal sprint (Nightreign's surge sprint) and wall-runs.
- **The source decides which field skill and whether it's available; its level decides how strong** (a Generic can't fly without an elemental source that gives flight).
- **Range of movement:** dash, Enhanced Sprint, glide, flight, teleport…; **all locked except the normal dash** (everyone has it). **Double-click L3** activates (keyboard: double-tap sprint, to confirm).
- **Every element does every movement type its own way** (Crown): Lightning and Void both teleport, differently. The element sets the style; an elemental source is needed for elemental movement.
- **Swimming:** everyone can swim (base movement, like the sprint and the dash).
- **Movement types (Crown, 2026-10-27):** Dash (everyone), Enhanced Sprint (faster sprint + wall-run + **run on water**), Glide, Flight (hovers in place when stationary), Teleport, High / double jump, **Air dash** (hover in place, aim, then dash that way: air dash and hover merged), Super launch (blast up, then glide), Phase / shadow step (through thin walls or bars), Mark and recall (drop a marker, snap back), **Regroup** (teleport to a party member; co-op catch-up). **Removed:** grapple, wall climb, ride / mount, surface surf (Enhanced Sprint covers water); hover merged into air dash and flight. 11 types with Regroup. Each element does each type its own way.
- **Element styles: A** (Crown, 2026-10-27): one shared mechanic per type; each element changes the look and sound, with a few **signature** differences where it matters (Lightning teleport = chain of short blinks, Void teleport = one long vanish, Earth flight = magnetic).
- **Element styles also set strengths** (Crown, 2026-10-27): each element is better at some movement than others, e.g. **Lightning has the fastest sprint, Wind the second fastest**.
- **Unlocks: pinned** (Crown, 2026-10-27): leaning toward unlocking movement types by **tiers of certain skills / elemental skills**; up for discussion.
- **Element styles (Crown's ideas, filled in as they come):**

  | Element | Movement | Style |
  |---|---|---|
  | Earth | Flight | Electromagnetism (magnetic levitation) |
  | Lightning | Sprint | Fastest sprint |
  | Wind | Sprint | Second-fastest sprint |
  | Lightning | Teleport | Chain of short blinks |
  | Void | Teleport | One long vanish |

- **Locked field skills:** some need a specific skill equipped or learned first; once unlocked, anyone with the matching source can use it (lightning speed → any Lightning source).
- **Picking it** (Crown): choose a source you have, then a movement type you've unlocked; the source styles it.
- **Open:** each element's style per type (Crown's, close to lore); dash-into-sprint meaning.

## Choices to make (one at a time)

1. **Decided (Crown, 2026-10-27): a fast, free sprint for everyone** (Nightreign); field skills and evolutions build on top.
2. **Decided (Crown, 2026-10-27): evolutions give both**: a small always-on movement bonus (e.g. +10-20% sprint, `// TODO: tune`) **and** one active movement skill that costs EP (e.g. a lightning dash burst). Kingdom Hearts 2's forms.
3. **Decided (Crown, 2026-10-27): speed and dash first, glide next, flight last.** Plus:
   - **Dash evolves into sprint**: a dash carries on into a faster sprint (in motion or through upgrades: to look into, Crown).
   - **Wall-running at speed**: moving fast enough lets you run along walls (Prototype, Warframe-style parkour).
4. **Decided (Crown, 2026-10-27): flight drains EP while flying, with a height ceiling and no-fly zones** (boss arenas, some secrets). **But effects can raise these limits**: e.g. an evolution might give **unlimited flight**; one of Crown's characters uses unlimited energy (for another day). **Hidden areas in the sky** (Shangri-La Frontier style secrets) are welcome.
5. **Decided (Crown, 2026-10-27): elements change the world through a few general rules defined once** (Breath of the Wild): elements change materials (Fire burns wood and grass, Water / Ice douses or freezes, Wind lifts, Lightning conducts…), elements change each other (Water puts out Fire), materials don't change each other. Each map region gets 1-2 element shortcuts or secrets. The rule table is Crown's to confirm: `Design/ElementsOnTheWorld.md`.
6. **Decided (Crown, 2026-10-27): catching up in co-op is a spell / movement type, not a free system**: **Regroup**, a teleport to a party member (unlocked like other movement types; EP cost; not in or entering a fight). Who joins a fight: players within the party radius (D-003).
7. Where field-only skills are slotted.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created from D-099 and the research | docs/field-abilities-research |

---

# Appendix: research (2026-10-27)


These are suggestions, not rulings. Researched 2026-10-27. Sources are linked on each point. Where a source is a player opinion or a guide rather than a critic or Epic, that is noted.

---

## 1. Games where abilities change exploration: what worked, what didn't

### Metaphor: ReFantazio (closest match: field strike leads to a battle advantage)
- Hitting enemies on the map before the turn-based fight softens them up and gives a surprise advantage. Reviewers said it "gives the whole thing a huge lift in terms of overall tempo". https://purexbox.com/reviews/xbox-series-x/metaphor-refantazio
- You can hit weak enemies several times without entering the menu fight, and dodge or stun tough ones before starting your turn. https://www.shacknews.com/article/141194/metaphor-refantazio-gamescom-2024-preview
- Destructoid's hands-on: the blend of action and turn-based "works". https://www.destructoid.com/?p=536903
- Criticism: striking first makes fights too easy; the gap between "you struck first" and "you were struck first" is too big (smaller outlet). https://opencritic.com/valatrix/review/17289/metaphor-refantazio
- **Lesson:** keep the first-strike bonus clearly useful but not decisive. One free turn or a stagger works better than a big chunk of damage.

### Honkai: Star Rail Techniques (closest match to EP for field skills)
- Each character has one map-only "Technique". It either starts the fight with a special attack or gives a buff before the fight. It costs Technique Points, starting at 3 max. https://www.prydwen.gg/star-rail/guides/combat-and-exploration
- Points come back only by breaking purple refill objects placed around the world. That is the same idea as Crown's EP refilling at rest spots and from items. https://progameguides.com/honkai-star-rail/honkai-star-rail-techniques-how-do-they-work-explained/
- Examples: Sparkle's team stealth for 20 s on the map; The Herta's +60% ATK for the first 2 turns of the next battle; Phainon spends 2 points to hit every enemy in range. https://mobalytics.gg/blog/honkai-star-rail/sparkle-hsr-guide/ , https://honkai-star-rail.fandom.com/wiki/ATK
- Missing a Technique still spends the point, which teaches players to aim. https://www.prydwen.gg/star-rail/guides/combat-and-exploration
- **Lesson:** a small, visible pool (3 to 5 uses) with refills placed on the map is proven and easy to read.

### Kingdom Hearts (Drive Forms that unlock movement; Flowmotion)
- KH2: each Drive Form levels up and then grants a "growth ability" that also works in normal form: Valor gives High Jump, Wisdom gives Quick Run, Master gives Aerial Dodge, Final gives Glide (Glide is from my own knowledge; the wiki confirms the rest). A temporary transformation leaves a lasting movement unlock. https://www.khwiki.com/Drive_Form , https://www.khwiki.com/Growth_Ability
- KH3D Flowmotion (wall-kick and grind off anything) made platforming trivial. Later jump abilities felt useless, and areas had to be fenced with invisible walls. https://rpgsite.net/review/3089-kingdom-hearts-3d-dream-drop-distance-review , https://techraptor.net/content/kingdom-hearts-3d-dream-drop-distance-worked-didnt
- In KH3, Athletic Flow is more contextual. One user said it was nerfed and committal early on. https://backloggd.com/u/Scrotax/likes/ , https://inverse.com/article/52856-kingdom-hearts-3-flowmotion-not-working-guide-and-explanation
- **Lesson:** this matches Crown's evolution idea well: the form is temporary, but some movement can stay as a passive. Movement that works on every surface breaks level design.

### Elden Ring Nightreign (same session shape: short runs, team of 3)
- Every character gets faster sprint, a wall jump, and Spectral Hawk trees that launch you to glide between regions. There is no fall damage. Reviewers said this makes Nightreign much faster than Elden Ring. https://bgr.com/entertainment/elden-ring-nightreign-is-the-co-op-soulslike-you-didnt-know-you-needed/ , https://deltiasgaming.com/?p=243859 , https://deltiasgaming.com/?p=248825
- Only walls with visible ledges can be climbed. The traversal is shared by everyone and placed in the map, not owned by one character. https://deltiasgaming.com/?p=247535
- Reviews praised the co-op rush feel ("giddy and moreish"). Criticism was about variety and multiplayer issues, not about traversal. https://videogamer.com/reviews/elden-ring-nightreign-review , https://techradar.com/gaming/elden-ring-nightreign-review , https://africa.espn.com/gaming/story/_/id/45348683/elden-ring-nightreign-review
- **Lesson:** on a timed 30 to 45 minute map, base movement must already be fast. Long-distance movement can live in the map (launch points), so co-op players keep a similar pace.

### Genshin Impact / Natlan (each character has their own traversal)
- Natlan's terrain system and character traversal (Mualani surfing "Tony Hawk style", usable anywhere) were praised for speed and verticality. https://en.wikipedia.org/wiki/Natlan , https://www.pockettactics.com/genshin-impact/natlan-preview
- Some players found the saurian-specific mechanics frustrating. https://en.wikipedia.org/wiki/Natlan
- Base-game traversal (before Natlan) was criticised as "really, really slow". https://intothebluesky.com/2021/01/04/genshin-impact-review-pt-2/
- Element puzzles: switches marked with an element need a skill of that element. Anemo wind currents lift you into the air. Elements also work in co-op. https://primagames.com/gaming/genshin-impact-combat-and-elemental-combos-guide , https://www.bluestacks.com/blog/game-guides/genshin-impact/gi-map-guide-en.html

### Zelda: Breath of the Wild / Tears of the Kingdom (element systems, open-ended tools)
- BotW's "chemistry engine" uses three rules: elements change materials, elements change each other, and materials don't change each other. For example, fire burns grass and makes an updraft, and lightning finds metal. https://gamesbeat.com/the-legend-of-zelda-breath-of-the-wild-makes-chemistry-just-as-important-as-physics/ , https://www.thumbsticks.com/gdc-17-breath-of-the-wild-science-lies
- TotK's Ascend is restricted in some places on purpose: "the open world is designed so that it doesn't work everywhere". Shrines have intended solutions, but players "breaking" them is part of the fun. https://thesixthaxis.com/2023/05/11/the-legend-of-zelda-tears-of-the-kingdom-review , https://gonintendo.com/contents/20253-zelda-tears-of-the-kingdom-devs-reveal-what-inspired-link-s-ultrahand-and-ascend
- **Lesson:** a few general element rules get far more out of 9 elements than one-off puzzles do.

### Final Fantasy XVI
- Its zones were criticised as flat and linear, with little to find. The Eikon powers mostly don't change exploration (I found no source showing field use). This shows the cost of combat powers that do nothing on the map. https://www.gamerguides.com/final-fantasy-xvi/review , https://lemmy.dbzer0.com/post/533902

### Pokemon Legends: Arceus / Scarlet-Violet (ride forms)
- Arceus: ride Pokemon split movement by job. Wyrdeer is fast on land, Basculegion swims, and Braviary glides but always sinks slowly, so flight is limited by height. https://bulbapedia.bulbagarden.net/wiki/Ride_Pok%C3%A9mon_(Hisui) , https://www.dexerto.com/pokemon/unlock-rideable-pokemon-legends-arceus-1757728/
- Scarlet/Violet: dash, swim, glide and climb unlock one at a time from Titan fights. Real flight waited until the end of the Indigo Disk DLC, so the base map never had to support it. https://www.imore.com/gaming/nintendo-switch/pokemon-scarlet-and-violet-travel-abilities-how-to-unlock-surf-glide-climb-dash-and-jump , https://primagames.com/tips/pokemon-indigo-disk-how-to-unlock-flying-with-koraidon-and-miraidon-scarlet-violet

### Monster Hunter Wilds (Seikret mount)
- The mount can auto-run to a marked target while you steer, heal or sharpen. It was praised on big maps. https://blog.playstation.com/?p=395148 , https://automaton-media.com/en/tips/monster-hunter-wilds-three-important-uses-of-the-seikret/
- Some players disliked auto-explore and found that too much automation feels linear (player and small-outlet opinions). https://theouterhaven.net/?p=289164 , https://steamcommunity.com/app/2246340/discussions/0/4630356669572905429/?ctp=8

### Metroid / Hollow Knight / Ori (movement that opens new routes)
- Standard gating: double jump reaches ledges, dash crosses gaps, and so on. Good practice is for each ability to open at least two gates and to make old areas faster to cross. https://bugnet.io/blog/how-to-design-a-metroidvania-map , https://skills.cat/skills/thedivergentai/gd-agentic-skills/godot-genre-metroidvania
- A 2026 thesis found that abilities with several modes made returning to an area feel like seeing it in a new way. https://odr.chalmers.se/items/d79d64a8-8f42-4b85-a4f8-5b1b259962b0
- Ori's Bash (launching off enemies and projectiles) is the most praised move, but it lets players skip fights entirely. https://www.gameskinny.com/k7qc0/ori-and-the-blind-forest-review , https://caneandrinse.com/forum/viewtopic.php?p=36856

### Prototype / Infamous / Warframe (fast superhero movement)
- Prototype's leaps, air dashes and glides felt "exhilarating", but fast upgrades made earlier upgrade choices redundant. https://avclub.com/prototype-1798216830 , https://www.gadgetguy.com.au/prototype/
- Warframe players love its parkour. The common complaint is that Bullet Jump is the fastest move everywhere, so play turns into "bullet jump spam", and clutter snags you mid-move. https://forums.warframe.com/topic/1058627-rationale-for-a-parkour-overhaul-with-suggested-system/ , https://wiki.warframe.com/w/Maneuvers , https://forums.warframe.com/topic/1089424-smooth-down-level-geometry/
- General point on flight: it opens areas the level designer meant to keep closed, so limit its duration or height and give places to land. https://sandboxgame.medium.com/abilities-part-two-flight-use-cases-implementation-guide-d39cdc3754f3
- Insomniac's GDC talk on Spider-Man traversal covers using the camera to sell speed. https://gdcvault.com/play/1026084/Concrete-Jungle-Gym-Building-Traversal

### Speed and flight: fun or level-breaking? The common thread
- Fun when it is the *main verb* and the map is built for it (Nightreign, Natlan, Spider-Man).
- It breaks things when it works on every surface (KH3D Flowmotion), when one move beats all others (Warframe Bullet Jump), or when it skips content (Ori Bash, which skips fights).
- Most games keep it in check with: slow sinking (Braviary), launch points placed on the map (Nightreign hawks), surfaces that are marked as usable (Nightreign walls), unlocking it late (Scarlet/Violet flight), or banning it in some areas (TotK Ascend).

---

## 2. Design patterns

**Always-on (passive) vs activated with a cost**
- Passive from gear or form: KH2 growth abilities, Nightreign sprint and wall jump. It needs no input and no decision, and is best for the basic feel.
- Activated with a resource: Star Rail Techniques (points that refill from objects on the map). Each use is a decision, which fits Crown's EP.
- Cooldown vs resource: a cooldown invites spamming whenever it is ready. A resource that only refills at chosen spots creates planning ("do I spend EP to reach that cache?"). Enshrouded players asked for cooldowns plus combat restrictions on teleports, which shows how often both are mixed. https://enshrouded.featureupvote.com/suggestions/642669/comment/782830

**Ways to gate flight**
- Slow sinking or fixed glide only (Braviary, Nightreign hawks).
- Duration or EP drain, with height limits and safe landing spots. https://sandboxgame.medium.com/abilities-part-two-flight-use-cases-implementation-guide-d39cdc3754f3
- Areas where the ability doesn't work, fenced by design or by a volume (TotK Ascend; KH3D needed invisible walls).
- Launch points placed on the map instead of free flight (Nightreign).

**Elements and the world**
- Use general rules, not one-off scripts (BotW's chemistry rules). Examples: fire burns vines and grass and makes an updraft; ice freezes water into a bridge; wind lifts gliders (Genshin Anemograna); lightning powers metal switches; water douses fire walls.
- Mark switches with an element icon (Genshin).

**Co-op and one player's movement power**
- Guild Wars 2 offers "Teleport to Party Leader" in a mission. https://wiki.guildwars2.com/wiki/Teleport_to_Party_Leader
- Diablo 4 players complained loudly about having no teleport to a teammate. https://us.forums.blizzard.com/en/d4/t/which-intern-forgot-the-add-the-teleport-to-player-feature/28143
- Gerty requires everyone near a teleporter so one player can't move the group alone. https://www.moddb.com/games/gerty/features/designing-co-op-gameplay
- Game Developer suggests remote help with a long channel or a cost. https://www.gamedeveloper.com/design/co-op-revive-mechanics
- Common answers: party-wide buffs (Sparkle's team stealth), shared traversal placed on the map (Nightreign), or a regroup with a channel time that you can't use in combat.

---

## 3. Unreal Engine 5 implementation

**GAS basics**
- Gameplay Effects can be Instant, Infinite or Has Duration. Duration is in seconds and can be set on the spec before it is applied, so "1 battle turn = 60 s on the map" can be one SetByCaller or SetDuration value. https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-effects-for-the-gameplay-ability-system-in-unreal-engine
- Overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine
- Community reference (prediction, tags, attribute sets): https://github.com/tranek/GASDocumentation

**Move speed from an attribute**
- Pattern: write a custom CharacterMovementComponent whose `GetMaxSpeed()` reads a `MoveSpeed` attribute from the ASC while walking, and falls back to the parent in other modes. https://zomgmoz.tv/unreal/Gameplay-Ability-System/Controlling-Character-speed-using-the-Ability-System
- Epic: `GetModifiedMaxSpeed = GetMaxSpeed() * GetMaxSpeedModifier()`, and `GetMaxSpeed` is per movement mode. https://docs.unrealengine.com/4.26/en-US/API/Runtime/Engine/GameFramework/UMovementComponent/K2_GetMaxSpeedModifier/index.html
- Co-op warning: changing MaxWalkSpeed on the server only, or changing it often, makes the client rubber-band. Change it on both client and server, and debug with `p.NetShowCorrections 1`. https://answers.unrealengine.com/questions/885714/view.html , https://answers.unrealengine.com/questions/814703/view.html
- (My inference:) a Duration GE applied with prediction from a locally predicted ability changes the attribute on both sides. For timed buffs that is usually fine. For frequent toggles (sprint), the robust route is a CMC compressed flag or a saved-move flag.

**Flight, dash and custom modes**
- Custom modes: `SetMovementMode(MOVE_Custom, YourEnum)` and override `PhysCustom()`. CustomMovementMode replicates. https://forums.unrealengine.com/t/custom-movement-mode-creation/309530 , https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/EMovementMode
- For proper client prediction, subclass `FSavedMove_Character` (Clear, SetMoveFor, PrepMoveFor, GetCompressedFlags, CanCombineWith). https://openskillindex.com/skills/comeonoliver-skillshub-ue-character-movement
- Dash: `UAbilityTask_ApplyRootMotionConstantForce` takes direction, strength, duration and a curve, has an OnFinish event, and works with prediction through root motion sources. https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/GameplayAbilities/UAbilityTask_ApplyRootMotionCons-?lang=en-US , https://docs.unrealengine.com/4.27/en-US/BlueprintAPI/Ability/Tasks/ApplyRootMotionConstantForce/index.html
- Flight can start as built-in `MOVE_Flying` plus a GE that drains EP or has a duration. Write a custom mode only when you need a glide or sink.

**Mover vs CharacterMovementComponent**
- Epic calls Mover experimental, with APIs and data formats subject to change. It will eventually replace CMC, and it uses Network Prediction or Chaos networked physics with rollback. https://dev.epicgames.com/documentation/unreal-engine/mover-in-unreal-engine , https://dev.epicgames.com/documentation/unreal-engine/mover-examples-in-unreal-engine
- A third-party blog says it was still late-experimental or beta in 5.7 and not production-ready. https://www.strayspark.studio/blog/mover-2-0-vs-character-movement-component-ue5-7-2026
- **Suggestion:** stay on CMC. Put speed behind the attribute and keep movement-mode logic in one CMC subclass, so a later move to Mover touches one class. Check Mover's status again when co-op is scheduled (D-065).

**Input**
- Lyra's pattern is Enhanced Input action, then InputTag, then the granted ability whose tag matches. It suits fixed keys now and rebinding later (D-063). Lyra source, community version: https://github.com/intrxx/Enhanced-Input-Ability-Activation

**Structure (suggestion)**
- One ability class per skill with two activation branches: battle mode (the existing turn pipeline) and field mode (the GE on the map). Choose the branch with a tag, for example `State.InCombat`. Use `ActivationBlockedTags` to block field-only skills in battle, and block field use of battle-only skills outside battle. This keeps one data asset as the source of truth for each skill.
- Separate classes only for field-only skills (flight, regroup).
- Tags: `State.InCombat`, `State.Field.Flying`, `Zone.NoFly` (applied by a volume as a loose tag), `Cooldown.Field.*`.
- Per-fight and per-run state lives on the PlayerState or ASC, not on a subsystem (D-064).

---

## 4. Practical recommendations for Crown's game

**Movement to start with (in order)**
1. **Faster sprint as the baseline** for everyone (Nightreign lesson: a timed map needs fast base movement). It is free and has no EP cost.
2. **Haste (speed spell, field mode):** a Duration GE of +X% MoveSpeed, 1 turn = 60 s. It is the cheapest to build and proves the battle-to-field pipeline.
3. **Dash / blink:** root motion constant force. It also gives an aggro-dodge tool.
4. **Glide** (for example a Wind evolution), with a slow sink, before any true flight.
5. **True flight last:** EP drain, a height ceiling, and `Zone.NoFly` volumes over arenas and the boss.

**How evolutions grant traversal**
- Use the KH2 model: equipping the evolution gives a *passive* movement trait (the lightning evolution gives a permanently faster sprint, or sprint leaves a short spark trail), plus one *active* field skill that costs EP (a lightning burst: a short, very fast dash line).
- The passive should be modest (+10 to 20%). The big moment is the active skill. This avoids the Warframe problem of one move beating every other.

**EP tuning (starting values to playtest)**
- Copy Star Rail: a small pool counted in uses. Give about 3 to 5 field uses per refill, and place 2 to 3 rest spots or refill caches per 30 to 45 minute map.
- Prices: haste costs 1, dash costs 1 (or a cooldown with no EP if it is the basic dodge), glide costs 1 per launch, and flight drains continuously (about 1 per 10 s).
- Missing a first-strike attack still spends EP (Star Rail does this; it rewards aim with free aim).
- Keep the first-strike reward to one free turn or a stagger, not a decisive opener (Metaphor criticism).

**Keeping 30 to 45 minute maps working**
- Design for the fastest movement from day one. Treat speed as "get there sooner", never as "skip content".
- Put objectives behind fights or interaction, not behind distance.
- Mark climbable or launch surfaces clearly, and use no-fly volumes around set pieces.
- Use 9 element rules once, at system level (burn, freeze, wind lift, conduct, douse...), and place 1 to 2 element shortcuts per map region.

**Co-op**
- Party-wide field buffs (haste affects the whole party, Sparkle-style) remove most "one player is fast" problems.
- Add a regroup or "warp to leader" with a channel time, blocked in combat, and possibly costing EP. Followers in solo snap to the leader the same way.
- A fight started by one player pulls in nearby teammates (needs a ruling).

**Build order**
1. `MoveSpeed` attribute plus a CMC subclass `GetMaxSpeed` override, with a debug print (CLAUDE.md debug rule).
2. A field-mode branch on one battle skill (haste), using a tag-selected branch and a 60 s per turn duration. Headless spec on duration and value.
3. EP spend and refill at a placeholder rest spot.
4. Dash via root motion task. Field-attack first strike if not done yet.
5. Evolution passive trait (a GE granted on equip) plus its one active.
6. Glide, then flight with no-fly volumes.
7. Element world interactions (start with burn and freeze).
8. Co-op: check prediction with `p.NetShowCorrections`, then regroup and party buffs.
