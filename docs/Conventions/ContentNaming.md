# Content naming and folders

One page. **New assets follow it; existing names stay** (D-058: renames make redirectors and touch LFS files for little gain). The junk and duplicate cleanup is B-122. Source: Epic's *Recommended Asset Naming Conventions* plus the extensions this project already uses (survey in `docs/analysis/BestPractice_2026-10.md`, "Content naming and folders").

## Pattern

`Prefix_BaseName_Descriptor_Variant`, in PascalCase, with no spaces. For example `DA_Spell_Fireball`, `T_Sword_Iron_D`, `AM_Sword_Slash_02`.

- **Singular** kind words: `DA_Spell_`, not `DA_Spells_`.
- Spell the kind the same way every time. Use `DA_Equipment_`, not `DA_Equpment_`.
- A variant is a number with two digits (`_01`) or a short word (`_Fire`).

## Prefixes

| Asset | Prefix | Note |
|---|---|---|
| Blueprint class | `BP_` | |
| Widget Blueprint | `WBP_` | |
| Animation Blueprint | `ABP_` | |
| Animation montage | `AM_` | Don't use `Anim_` for new assets (15 old ones stay) |
| Animation sequence | `A_` | |
| Blend space | `BS_` | |
| Skeletal mesh | `SK_` | Engine mannequin assets keep `SKM_` |
| Static mesh | `SM_` | |
| Material / instance / function | `M_` / `MI_` / `MF_` | |
| Texture | `T_` | Suffix the type: `_D` (colour), `_N` (normal), `_ORM` (packed mask) |
| Niagara system / emitter | `NS_` / `NE_` | |
| Sound cue / wave / class | `SC_` / `SW_` / `SCL_` | |
| Data table / curve | `DT_` / `C_` | |
| **Data asset** | `DA_<Kind>_` | The kind is the C++ class's short name: `DA_Spell_`, `DA_Ability_`, `DA_Weapon_`, `DA_Ring_`, `DA_Character_`, `DA_Inventory_`, `DA_Trial_`, `DA_Cosmetics_`, `DA_Stance_`, `DA_Effect_`, `DA_Boss_` |
| Input action / mapping context | `IA_` / `IMC_` | One unique name per action (D-063, for rebinding later) |
| **Map** | `L_` | New maps only: `L_Hub`, `L_Trial_Fire_01`. The current `Level/*` maps keep their names |
| Primary asset label | `PAL_` | |

A type that isn't listed gets a short prefix added to this table in the same change.

## Folders

| Folder | Holds |
|---|---|
| `Content/Data/<Kind>/` | Design-time data assets, grouped by kind (`Characters`, `Pool`, `Skills`, `Spells`, `Trials`, `Items`, `Merchant`, `Enemy`). |
| `Content/Blueprints/` | Blueprint classes: characters, enemies, game modes. |
| `Content/UI/` | `Core/` for CommonUI input data and styles. `UMG/<Screen>/` for widgets. |
| `Content/VFX/` | Niagara systems and their materials. |
| `Content/Level/` | Maps (new ones prefixed `L_`). |
| `Content/Testing/` | **The one root for test and dev content.** Nothing in a shipping folder may reference it. `Data/Testing/` is folded in by B-122. |
| `Content/UI/Vendor/`, `SourceArt/` | Imported kit pieces (`UI_DeckbuilderReuse.md` §4). |

- Input assets currently live under `Data/Characters/Models/CharacterBase/Input/`. New input assets go there too until a move is planned. Moving them needs redirectors and the B-205 config paths.
- Vendor packs under `Content/Imports/` stay out of git (`.gitignore`). Only the originals of assets the game uses are committed (CLAUDE.md, LFS coverage).

## Checking

- No naming validator exists yet. Epic's `UEditorValidatorBase` could add one later; it's deferred (BestPractice §"Data validation").
- When you add an asset, check its prefix against the table above.
- Data validation in `Scripts/ue/test.sh` (B-119) checks content, not names.
