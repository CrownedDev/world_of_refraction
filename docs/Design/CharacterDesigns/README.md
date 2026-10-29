# Character Designs

**Status:** pinned (Crown, 2026-10-27) for a later session: creating characters, their loadouts and related quest lines.

One folder for everything visual and identity-related about characters: concept art, turnarounds, outfit / suit picks, and each character's loadouts. Story text stays Crown's own writing.

## What goes here

- **Concept art and turnarounds** (e.g. `V_turnaround.png`; image files are LFS-tracked).
- **Outfit and asset picks** per character (e.g. V's suit pack, `DesignDecisions.md` Q-144).
- **Loadouts** per character (gear, skills, evolutions).
- Later: **quest lines** tied to each character.

## Pinned until that session

- **V's clothing / suit pack** (Q-144: material masks first, then a bought UE5 suit pack; Crown picks the pack).
- Other characters' designs and loadouts.

## Starter (novice) kit (Crown, 2026-10-27)

Simple, non-technical gear and skills a novice could use, lore-wise; the base everyone can start from. **To expand later.**

- **Spells:** the 15 Generic-element spells that take the caster's element (Bolt, Missile, Lance, Wave, Storm; Might, Grace, Warding, Aura, Enhancement Aura; Mend, Restore, Renewal, Rally, Cleanse), all **F tier**.
- **Abilities:** the **3-4 simplest per weapon type** (e.g. a basic strike, a charge, a guard-break), picked from the existing sets (Sword, Sword-and-Shield, Greatsword, Axe, Dual Axe, Gauntlets).
- **Weapons:** simple ones per weapon type (e.g. Arming Sword, Longsword, Tomahawk, Combat Gauntlets: candidates from the existing set), **F tier**.
- **Rings:** simple rings (e.g. `DA_Ring_Generic`, plus one plain ring per element: candidates), **F tier**.
- Which exact abilities, weapons and rings: picked in the character design session (pinned).

## Character sheets (Crown's Google Drive, "My World of Refraction / Characters")

Source folder: https://drive.google.com/drive/folders/1-gc1wYbPD8mcoarMTvl-lqTJRZkgo_uZ (sheets as PNG; written profiles as .odt in `Character Sheet/`, drafts in `Coming up/`, alternates in `Variants/`).

**The style (from the Amethyst Prisma, Aron Citrine and Amber sheets, 2026-10-28):** anime-style turnarounds with expression sheets and a colour key (skin, blush, hair, eyes, gradation, pupil, highlight); clean line art, flat colours; **long military-cut coats with a high black collar and a white belt**, each in the character's gem colour (amethyst purple, citrine orange-red, amber ochre); slim, realistic proportions (5'9"–6'1"); ages given as apparent (19, 20) and real (100+).

**Roster on Drive (file names):** Amethyst Prisma, Orthos Prisma; Aron Citrine, Alexander Citrine, Eric Citrine, Eva Citrine; Amber; Garnet; Aurora Rite (+ Aurora variant); Sapphire Flowers; Emerald Sanctum; Peridot Crown, Historica Crown (+ young), Shadow Crown (+ young); Celina Riley (+ young), Elizabeth Riley, Ricardo Riley (+ young, + Sr); Keira Knight; Maximus Madman; Rage (Adult Love) / Love (Young Rage); Shade (Umbra Knight); Alphonso (+ hidden blade); Zandark; Vortrix; V.

Lore stays Crown's; this list is for asset and editor planning only.

## First characters: draft, under discussion (Crown, 2026-10-29; not committed)

Class, evolution and weapon per character (Crown's words; lore stays Crown's). "Adapter" = the new name for the ring loadout (D-107): a specialised tool for channelling spells, in any form (ring, pistols, trainers). Weapons stay weapons.

| Character | Class | Evolution | Weapon / adapters | Notes |
|---|---|---|---|---|
| **V** | — (the Void) | — | — | guide, mobs, hub NPCs (`Enemies.md`, `HubAndStory.md`) |
| **Orthos Prisma** | Generic | Light: **"United"** | Great axe with a B Light crystal | Evolution spells (Crown, 2026-10-29): **One for All** gives chosen allies some of his extended life and **Prismatic mode** (enhanced abilities); **All for One** takes it back, enhancing himself. Both are unique spells of the evolution, **boosted when Orthos wears it as bound gear** (D-033). Lore: United extends his life. **Mechanics (Crown, 2026-10-29: A):** One for All spends a share of Orthos's max HP to put chosen allies in Prismatic mode (stronger abilities: damage and speed up, `// TODO: tune`, for N turns); All for One ends it, returning that HP to him with a bonus. **United's passives** raise his **Spirit and Body** mainly, plus the sub-stats tied to physicality and energy (HP, EP, physical damage, Defense: exact list `// TODO: tune`). |
| **Garnet** | Generic | Fire: **"From the Ashes"** (Crown, 2026-10-29; was "Rise from the Ashes") | Halberd | Lore (Crown, 2026-10-29): it **revives him from the dead and makes him stronger**; a **breakthrough spell heals him** as an application of it. Mechanics: a once-per-fight auto-revive at a share of HP with a strength boost after (`// TODO: tune`), plus a self-heal breakthrough (**Phoenix Metamorphosis**, 2020 origin doc). **Phoenix Rising** (Crown, 2026-10-29; the 2022 webtoon's spell): a **breakthrough that gives the halberd / spear wings**: (1) in battle it raises **movement speed and Reflexes**; (2) **at S rank it unlocks Flight as his field movement**, riding the halberd / spear like a broom (the Flight movement type, `FieldAbilities.md`). Look (webtoon): golden halberd with axe head + spear tip and a pink gem; long red split coat, magenta pauldron and gauntlet. MoR 2020: "phoenix reincarnate". |
| **Sapphire Flowers** | Refractor (Water) | Water evolution (sleep) | — | lore: sleep; people near her get tired, herself included. **Mechanics (Crown, 2026-10-29: C + D):** the evolution makes her Water status buildup build a **Sleep status instead of Water's** (the bar breaks into Sleep, not the Water slow); passive: a **drowsiness aura** builds Sleep on nearby enemies each turn at a small cost to herself; breakthrough: puts one target fully to **sleep**. **Sleep break (Crown, 2026-10-29: A):** the target loses its next turn; the first hit wakes it and deals bonus damage (`// TODO: tune`). |
| **Amethyst Prisma** | Resonator | Void: "Monster Creation" | Adapters: dual pistols (casts resonance refractions through them); weapon: open | adapters may get a slot cost. **Mechanics (Crown, 2026-10-29: C):** the **passive is an uncontrolled attack** (Crown, 2026-10-29): **on a random turn a monster spawns from Amethyst** and strikes, then is gone (no lasting fourth fighter; this is the lore's "can't control his ability"; chance per turn `// TODO: tune`); and he has **named shots that create monsters with effects** on hit (examples in the 2022 "Call to Action" webtoon: list being extracted). |
| **Emerald Sanctum** | Refractor | — | An S-tier Emerald adapter | |
| **Amber** | Generic | Amber: "Infinite Stamina" | Dual chain scythes | |
| **Ares Citrine** (the Drive sheet "Aron Citrine"; Crown, 2026-10-29) | Resonator | Lightning: "Lightning Speed" | Adapters: trainers, two Lightning adapters | Sibling of Artemis Citrine; MoR 2020: Imperial Blade, Prince, "Protagonist" |

Build order once agreed: V template first, then these in Crown's order.
