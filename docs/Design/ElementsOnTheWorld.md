# Elements on the World

**Status:** model decided (`FieldAbilities.md` §5, B-215: Breath of the Wild's general rules); the rule table below is a **suggestion** being confirmed with Crown one element at a time (lore decides). The research is in the appendix; claims marked "(search summary)" came from search results only.

## Confirmed rules

**General rules (Crown, 2026-10-27):**
1. **An element hitting something changes it** (Fire on a wooden door: it burns; Ice on a lake: walkable). The **field attack** (the standard out-of-combat attack) and field skills carry your source's element.
2. **Two elements together make a reaction** (Fire + Water = steam; Water + Lightning = the whole puddle is charged).
3. **Objects don't change each other on their own**, but some **carry** an element (a metal fence carries Lightning; Wind spreads fire).
- **Never needed on the main path:** elements open shortcuts and secrets; an item route always exists.
- **Co-op:** world reactions don't hurt teammates.

**Field attack (Crown, 2026-10-27):**
- A field hit on an enemy also gives **status buildup** of its element, carried into the fight it starts (with first strike).
- **Holding the field attack with a Water source turns it into an Ice attack** (Ice is Water's sub-element).

**Movement (Crown, 2026-10-27): A, movement leaves its element lightly** on what it touches (Genshin's elemental sprints): a Fire dash lights the grass it crosses, a Lightning sprint through water charges it, an Ice sprint freezes a path on water. Small, short effects only; never harms teammates.

**Per element (Crown, 2026-10-27):**
- **Fire:** sets wood / grass burning (spreads a little, burns away); melts ice to water; ignites gas / mist. With Water: steam (blocks sight briefly); with Ice: melt; with Wind: spreads further and faster. Secrets e.g. burn a vine wall to a hidden cave; light a brazier ring to open a door.
- **Water:** puts out fire; makes things wet (wet carries Lightning); fills dry basins (floats objects). **Ice (Water sub-element):** freezes water (walkable), freezes wet things, puts out fire, makes metal brittle and breakable. With Fire: **steam**, which blocks sight briefly, puts the fire out, **and lifts you like an updraft** (glide on it). Water + Lightning: the whole wet area is charged; Ice + Lightning: frozen things shatter; Water + Earth: mud (slows enemies; freezes into a bridge). Secrets e.g. freeze a waterfall into a climbable column; fill a basin to float a crate to a ledge.
- **Earth:** raises / moves stone (platforms, steps, blocks); sand / soil into walls or pillars; shatters cracked walls. With Water: mud. With Lightning: **magnetism** (pull / push metal; ties to Earth flight). With **Fire: magma** (burns, melts ice and metal; **cools into new stone with Water or Ice**, e.g. a bridge or platform). With **Wind: a fast projectile** (wind-hurled stone; hits far targets, e.g. distant switches or cracked walls). Secrets e.g. raise pillars across a gap; break a cracked floor into a hidden room.
- **Wind:** blows away fog / mist; pushes light objects; makes an **updraft over fire or steam** (lifts you). With Fire: spreads it further and faster; with Earth: the fast projectile; **with any other element or state: carries and spreads it** (Genshin's Swirl). Secrets e.g. blow away a fog bank hiding a path; fan a fire up a cliff for an updraft.
- **Lightning:** charges metal, water and wet things (powers mechanisms, carries along the line); crystals store charge. With Water: the wet area is charged; with Ice: shatter; with Earth: magnetism. Secrets e.g. charge a wet floor to power a lift; cross a collapsing bridge with the fastest sprint.

## Changelog

| Date | Change | Branch |
|---|---|---|
| 2026-10-27 | Created from the research | docs/element-world-research |

---

# Appendix: research (2026-10-27)


Web research, 2026-10-27. Everything here is a **suggestion, not a ruling**. Crown's lore decides what each element means.
Source caveat: several primary pages (GamesBeat, Thumbsticks, Tom's Guide, Fextralife) would not load from this session. Where a claim comes from a search summary of those pages, it is marked "(search summary)".

---

## 1. What well-liked games do, and what players praised or criticised

### Zelda: Breath of the Wild / Tears of the Kingdom (the model Crown picked)
- GDC 2017 talk "Breaking Conventions with The Legend of Zelda: Breath of the Wild" (Fujibayashi, Dohta, Takizawa). Fujibayashi called it **"multiplicative gameplay"**: objects react to the player and to each other, instead of hand-making hundreds of one-off puzzles. https://zeldauniverse.net/features/ocarinas-image-freedom-in-breath-of-the-wild/ , https://www.aol.com/news/2017-03-12-breath-of-the-wild-gdc-talk.html
- Dohta: the physics engine (Havok) is a "rule-based movement calculator", the chemistry engine a "rule-based **state** calculator". https://gamesbeat.com/the-legend-of-zelda-breath-of-the-wild-makes-chemistry-just-as-important-as-physics/ (search summary)
- The three rules as reported by GamesBeat: (1) elements can change the state of materials, (2) elements can change each other's states, (3) materials cannot change each other's states (a burning tree can't change another tree; fire does that). Same source (search summary).
  - **Caution:** Tom's Guide's write-up instead says the engine covers element-element, element-material *and* material-material (example: chained metal blocks carry a shock). Read it this way: metal doesn't change metal, but metal *conducts* the electric element, so the element moves and the materials stay passive. https://www.tomsguide.com/uk/us/zelda-breath-wild-design,news-24586.html (search summary)
- "Elements" = fire, water, ice, electricity, wind; "materials" = trees, rocks, characters, weapons. Same Tom's Guide source.
- Typical states and examples players remember: metal draws lightning in storms, fire melts ice, rain makes rock slippery, wind spreads fire and updraft from fire lifts the glider. https://the-artifice.com/systemic-games-philosophy/
- Game Maker's Toolkit "The Rise of the Systemic Game" (2018): the praise is "consistent and universal rules of interaction"; the warning is that many systemic games "fail to truly encourage players to find these emergent solutions". https://amara.org/v/C3BEi
- **Praise:** many answers to one puzzle. Fujibayashi said they wanted to fix Skyward Sword's "one answer per puzzle" problem. https://www.zeldadungeon.net/?p=202875 ; TotK shrines solved by elegant builds or brute force, both count. https://automaton-media.com/en/news/20230530-19231/
- **Criticism (weak sources):** "multiple solutions" can mean fewer clear, satisfying "aha" puzzles; some players miss tighter classic puzzle design. https://geekchamp.com/the-legend-of-zelda-tears-of-the-kingdom-reviews/

### Divinity: Original Sin 2 (surfaces and clouds, turn-based)
- Praise: "an abundance of combinations and synergy" in turn-based fights. https://classic.rpgfan.com/previews/Divinity_Original_Sin_2/index.html
- Larian kept unintended combos unless they "break the fun". https://www.pcgamer.com/uk/an-infinite-damage-combo-has-been-discovered-in-divinity-original-sin-2
- **Criticism:** visual clutter in combat https://www.senses.se/en/?p=2336 ; "oops, all surfaces" chaos, "more annoying than engaging" in practice; Baldur's Gate 3 toned surfaces down and was praised for it. https://www.pcgamer.com/uk/how-baldurs-gate-3s-compares-to-divinity-original-sin-2 , https://sg.news.yahoo.com/divinity-original-sin-2-isnt-173058138.html
- Opposite complaint: armour made surfaces feel too weak. https://steamcommunity.com/app/435150/discussions/0/1519260397773929233
- Co-op: Larian called it "competitive co-op" — players can betray each other mid-fight; optional but intended. https://kotaku.com/what-i-saw-of-divinity-original-sin-2-1726655377 , https://co-optimus.com/article/14568/page/2/divinity-original-sin-2-early-access-co-op-impressions.html
- Lesson: surfaces are loved as a *tactical layer* but get tiring when every fight is covered in them. Keep the world layer readable and fairly rare.

### Genshin Impact (open world, co-op)
- World reactions: burn grass, freeze water to make paths, wind currents lift gliding, Pyro lights torches. https://www.bluestacks.com/blog/game-guides/genshin-impact/gi-map-guide-en.html , https://www.icy-veins.com/genshin-impact/elements-and-elemental-reactions
- Burning on grass spreads to nearby grass and sets enemies on fire; wooden objects also burn. https://commonsensegamer.com/genshin-impact-3-0-all-dendro-reactions-and-how-to-trigger-each/ , https://www.icy-veins.com/genshin-impact/burning-elemental-reaction
- Anemo "Swirl" picks up an element and spreads it to an area — wind as a *carrier*, not a damage type. https://genshinimpact.wiki.fextralife.com/Swirl
- Hydro gives "Wet", which makes Electro stronger. https://www.thegamer.com/genshin-impact-elements-guide/
- Element locks: "Stone Keys" etc. need one specific element. https://www.icy-veins.com/genshin-impact/elements-and-elemental-reactions
- Lesson: Genshin's world use is mostly **"element key opens element lock"** — easy to read, but shallower than BotW. Good for shortcuts; the secrets want the BotW-style systemic part.

### Magicka (co-op)
- Combine elements into new ones (fire + water = steam); friendly fire always on; the accidental team kills are part of the fun. https://www.co-optimus.com/review/678/page/1/magicka-co-op-review.html , https://www.gameinformer.com/games/magicka/b/pc/archive/2011/01/27/not-a-review.aspx?amp
- Criticism: the chaos "fizzles after the first few levels"; beams everywhere except where they should go. https://www.co-optimus.com/review/1486/magicka-2-co-op-review.html , https://gameinformer.com/games/magicka_2/b/pc/archive/2015/06/02/friendly-fire-fulminations.aspx

### Noita (every pixel simulated)
- Simple local rules: fire looks at a neighbour pixel and ignites it if flammable; oil burns, water doesn't, and fire turns water into steam. https://80.lv/articles/noita-a-game-based-on-falling-sand-simulation/
- Praise: "glorious chaotic energy where every run truly feels unique". https://bit-tech.net/reviews/noita-review/1/
- Criticism: randomness "can get totally out of hand"; repetitive loop. https://opencritic.com/game/10298/noita/reviews
- Lesson: a full simulation is great for roguelites, too chaotic for a designed map with fixed secrets. Use a few rules, not a simulation.

### Immersive sims (Prey, Dishonored)
- Arkane: give "tons of tools that modify the rules" and "there always have to be multiple solutions, including ones that the player might make for themselves". https://wnhub.io/news/game-design/item-42892
- Prey's GLOO gun: meant for enemies, also builds stairs and plugs leaks. https://videogamegeek.com/videogamegenre/3107/immersive-sim
- Lesson: one tool, several uses. Each field skill should have a combat use and a world use.

### Okami (Celestial Brush)
- Brush powers are both combat moves and field-puzzle tools (e.g. wind brush puts out a burning enemy; Veil of Mist slows time). https://okami.fandom.com/wiki/Celestial_Brush_techniques , https://www.gamespot.com/articles/okami-video-update-wielding-the-celestial-brush/1100-6154093/
- Closest single-player model for "skills learned in combat also open the map".

### Pokémon field moves / HMs (the warning)
- HMs were disliked because they cost a battle move slot and forced "HM slave" party members; Sun/Moon replaced them with Ride Pokémon. https://www.themarysue.com/pokemon-sun-moon-no-hms-yay/ , https://www.dittobase.com/sun-moon/hms
- But fans also missed optional field moves (Dig, Sweet Scent, Teleport) once all of them were removed. https://www.fandom.com/articles/1353-6-pokemon-sun-and-moon-features-the-community-is
- Lesson: **don't make the party build depend on a field element**. If a gate needs Fire, give a non-character way too (a fire item, a torch, a nearby brazier), or keep element gates on optional secrets only.

### Light and hidden things (for Light / Darkness)
- Zelda's Lens of Truth shows invisible platforms, fake walls and pits; costs magic; never required (trial and error also works). https://zeldadungeon.net/wiki/Lens_of_Truth
- Alan Wake: light burns the darkness off enemies before they can be hurt — praised as the game's most interesting idea. https://en.wikipedia.org/wiki/Alan_Wake , https://www.stripes.com/living/alan-wake-sense-the-suspense-1.102717

### Hidden secrets with no hints (Crown's pillar)
- Elden Ring's 50-hit wall: huge community excitement and discussion, then letdown because it only led to a shortcut. https://www.pushsquare.com/news/2022/03/elden-ring-community-shocked-by-new-fake-wall-revelation , https://www.pcgamesn.com/elden-ring/illusory-walls
- Lesson: no-hint secrets are loved when the *payoff* is worth it, and when the rule that finds it is a rule players already know (consistent system), not a random action.

### Games with no world elements (Octopath Traveler, Persona)
- Not researched in depth. Their maps use fixed path actions (Octopath's "Path Actions" are talk/steal/fight per character), not element physics. Fine for menu-driven exploration; no systemic secrets. No source fetched — treat as background only.

### Avatar / bending games
- No well-received example found worth citing; licensed Avatar games are generally not cited as models. Skip.

---

## 2. Design patterns that work

- **Two lists, one small table.** Materials (things in the world) and elements (things you throw). Elements change materials and each other; materials only sit there or carry an element. (BotW rules above.)
- **Material tags that cover most maps:** Wood/Grass (burns), Metal (conducts), Water (wets, conducts, freezes), Ice (melts, slippery), Stone (blocks most, Earth moves it), Crystal (this game's — could focus/split Light), Shadow (hides; Light dispels), Mist/Gas (wind moves it, fire may ignite it), Rune/Seal (one element opens it).
- **Element states (keep to ~5):** Burning, Wet, Frozen, Charged, plus one or two for abstract elements (Revealed / Shrouded). DOS2 shows that many surface types turn into clutter.
- **Propagation:** fire spreads to touching flammable tags (Noita/Genshin style); lightning spreads through Wet and Metal; wind carries what it touches (Genshin Swirl). Use short ranges and a cap so it doesn't run away.
- **Timers and resets:** states end after a time (Genshin auras run out) or on an opposite element (water puts out fire). Big changes (burned bridge, melted wall) stay until the run ends — 30–45 min runs make that natural.
- **Readability:** one colour + one sound per state, the same everywhere. Elden Ring's fake-wall debate was about *missing* cues; DOS2's complaint was about *too many* overlapping visuals.
- **How many rules:** BotW: 3 rules, ~5 elements. Genshin: 7 elements, ~10 reactions but world use is mostly key-lock. Sweet spot for a 30–45 min map: **about 3 general rules, ~10 materials, ~5 states, each element having 2–3 world effects.**
- **Abstract elements need a "physical stand-in":** Light = reveals and dispels (Lens of Truth, Alan Wake). Darkness = hides, makes shadow paths. Void = removes/phases (erase a barrier, pass through). Reality/Unknown = restores or rewrites (undo a change, make a shown illusion real). Each still follows the same rules: it changes a material's state.
- **Co-op:** decide early what friendly reactions do. Magicka/DOS2 show chaos is fun on purpose but can grief. For a turn-based RPG with a real-time map, suggest: world reactions hurt enemies and the world, not partners (or only a small, non-lethal effect), and permanent world changes are server-owned.
- **No HM trap:** every required path has a non-element way through; element routes are shortcuts and secrets.

---

## 3. Suggested rule table for Crown's elements (SUGGESTION ONLY — lore decides)

### General rules (once, whole game)
1. An element hitting a material changes the material's state (from the table).
2. An element meeting another element (or a state) makes a reaction.
3. Materials never change each other; some only **carry** an element (Metal and Water carry Lightning; Wind-touched things carry their element).

### Element → material
| Element | Reaction with materials |
|---|---|
| Fire | Wood/Grass → Burning (spreads, burns away, leaves ash); Ice → melts to Water; Mist/Gas → ignites; Shadow → lit for a time |
| Water | Burning → put out; Stone/Earth → Wet (Lightning carries); dry riverbed/basin → fills (raises floating objects) |
| Ice (Water sub) | Water surface → Frozen (walk on it); Wet thing → Frozen; Burning → put out; Metal → brittle, breakable |
| Earth | Stone → moved/raised (platform, stairs, block); Sand/Soil → wall or pillar; cracked walls → shattered |
| Wind | Mist/Gas → blown away; Burning → spreads fire further; light objects → pushed; updraft over Burning lifts the player |
| Lightning | Metal/Water/Wet → Charged (powers mechanisms, carries along the line); Crystal → stores charge |
| Light | Shadow/Hidden → Revealed (fake walls, invisible platforms); Crystal → beam focused/split; Darkness state → dispelled |
| Darkness | Light-lit area → Shrouded; Shadow → becomes a walkable "shadow path" for a time; hides the player from patrols |
| Broken Darkness | Like Darkness but unstable: Shrouded areas collapse, opening cracks (could break Seals Darkness can't) — Crown's lore first |
| Void | Barrier/Seal/thin wall → erased for a time (pass through); objects → vanish then return; ignores most states |
| Reality ("Unknown") | Changed material → restored to its start state (unburn a bridge, unfreeze); a Revealed illusion → made real or unmade |

### Element → element (reactions)
| Pair | Result (suggestion) |
|---|---|
| Fire + Water | Steam: blocks sight briefly, puts fire out |
| Fire + Ice | Melt: ice gone, Water left |
| Fire + Wind | Fire spreads farther and faster |
| Water + Lightning | Conduct: whole wet area Charged |
| Ice + Lightning | Shatter: frozen things break |
| Earth + Water | Mud: slows enemies, can be frozen into a bridge |
| Earth + Lightning | Magnetise: metal pulled/pushed (fits Earth flight via electromagnetism) |
| Wind + any state | Carry: spreads that state (Genshin Swirl) |
| Light + Darkness | Cancel: both end; a hidden thing behind darkness shows |
| Light + Broken Darkness | Flare/rupture — lore decides |
| Void + any | Erase: removes the other element's state |
| Reality + any | Restore: returns to the start state (also undoes a Void erase) |

### Example shortcuts / secrets per element (1–2 each)
- **Fire:** burn a vine wall to a hidden cave; light a brazier ring to open a door.
- **Water/Ice:** freeze a waterfall into a climbable column; fill a basin to float a crate to a ledge.
- **Earth:** raise stone pillars across a gap; break a cracked floor into a lower room.
- **Wind:** blow away a fog bank hiding a path; fan a fire up a cliff to make an updraft.
- **Lightning:** charge a wet floor to power a lift; send a sprint (fastest movement) across a breaking bridge before it falls.
- **Light:** reveal invisible platforms over a chasm; focus a beam through crystals to open a seal.
- **Darkness:** walk a shadow path along a wall only when the area is Shrouded; hide from a sentinel to reach a locked area.
- **Broken Darkness:** crack a sealed tomb that normal Darkness can't open.
- **Void:** long-vanish teleport through a thin wall; erase a barrier for a few seconds.
- **Reality / Unknown:** restore a ruined bridge to its old state; make a "fake" (revealed) wall real to climb it. Best kept rare — the most hidden secrets.

### Class note (suggestion)
- Field hits apply the weapon's/character's element for everyone; Casters and Resonators get field skills with bigger reach (area freeze, long reveal). Generic needs items (fire flask, light crystal) so no class is locked out (HM lesson).

---

## 4. Unreal Engine 5 notes (implementation suggestions)

- **Material and state as GameplayTags:** e.g. `World.Material.Wood`, `World.State.Burning`. `FGameplayTagContainer` matches parent tags (`World.State` matches any state). Actors expose tags via `IGameplayTagAssetInterface::GetOwnedGameplayTags`. https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GameplayTags , https://docs.unrealengine.com/4.27/API/Runtime/GameplayTags/FGameplayTagContainer/index.html
- **One interface for every reactive thing:** `UINTERFACE` + `IElementReactive` with a `BlueprintNativeEvent` `ApplyElement(ESpellElement Element, AActor* Instigator)`; C++ default in `ApplyElement_Implementation`, designers can override in Blueprint. https://dev.epicgames.com/documentation/en-us/unreal-engine/interfaces-in-unreal-engine
- Simplest form: a `UElementReactiveComponent` (ActorComponent) that implements the logic and holds the state tags, so any actor gets it by adding the component (fits the project's "mutable runtime state on an actor = UActorComponent" rule).
- **Reaction table as data:** a `UDataTable` with an `FTableRowBase` row (Element, MaterialTag/StateTag → ResultStateTag, Duration, SpreadRadius, VFX soft pointer). https://dev.epicgames.com/documentation/en-us/unreal-engine/data-driven-gameplay-elements-in-unreal-engine , https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FTableRowBase . (The project's rule prefers `UPrimaryDataAsset` for design data; a data asset holding an array of rows works the same.) New VFX fields should be `TSoftObjectPtr` (project rule D-057).
- **VFX:** Niagara per state (looping burn, frost, sparks). https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-visual-effects-in-niagara-for-unreal-engine . If states are GAS effects, `GameplayCueNotify_Actor` keeps a looping Niagara alive while the tag lasts. https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/GameplayCueNotify_Actor
- **GAS fit:** for characters, states can be GameplayEffects granting tags (e.g. `World.State.Wet`). For plain world props, a full AbilitySystemComponent per prop is heavy; a light component with a tag container is enough. (Engineering judgement, no source.)
- **Co-op replication:** the server decides reactions; the state tag container (or a small enum/bitmask) is `ReplicatedUsing=OnRep_State`, and the OnRep starts the matching Niagara/sound on clients. https://dev.epicgames.com/documentation/en-us/unreal-engine/replicate-actor-properties-in-unreal-engine
- **Many props, little traffic:** keep reactive props net-dormant and call `FlushNetDormancy()` before changing state. https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-network-dormancy-in-unreal-engine
- **No Tick:** apply on hit/overlap events; timers (`FTimerManager`) end states and step spreading (one hop per short interval, max hops). Matches project rule "Avoid Tick".
- **Reset on run end:** each reactive component stores its starting tags; a run-end event (delegate on the map's GameState) restores all. Same restore function can power the Reality/"Unknown" element.
- **Debug (project rule):** `ElementReactionDebug` with `GetReactionString()` and a CallInEditor "Print reactions" button on the data asset.

---

## 5. Short take
- BotW's 3 rules are the right model and are well-reviewed; keep the state list small (DOS2 clutter warning).
- Element locks (Genshin style) for shortcuts; systemic rules (BotW style) for hidden secrets — players accept no-hint secrets when the rule is consistent and the reward is worth it.
- Don't gate the main path on one element (Pokémon HM lesson); give Generic an item route.
- In co-op, make world reactions server-owned and gentle on teammates.
