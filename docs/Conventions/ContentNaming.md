# Content naming and folders

One page. **New assets follow it; existing names stay** (D-058: renames create redirectors and touch LFS files, for little gain). The junk and duplicate cleanup is B-122.

Sources:
- Epic's *Recommended Asset Naming Conventions*.
- What this project's content already does. The survey is in `docs/analysis/BestPractice_2026-10.md`, under "Content naming and folders".

Where the two differ, the project's existing usage wins, so new assets sit next to their siblings.

## Pattern

`Prefix_BaseName_Descriptor_Variant`, in PascalCase, with no spaces. Examples: `DA_Spell_Bolt`, `T_Sword_Iron_D`, `AM_Sword_Slash_02`.

- Kind words are **singular**: `DA_Spell_`, not `DA_Spells_`.
- A kind is spelled the same way every time: `DA_Equipment_`, not `DA_Equpment_`.
- A variant is either a two-digit number (`_01`) or a short word (`_Fire`).

## Prefixes

**Epic** = Epic's list, as quoted in the survey. **Project** = a project choice for a type that Epic's list leaves out.

| Asset | Prefix | Source | Note |
|---|---|---|---|
| Blueprint class | `BP_` | Epic | |
| Widget Blueprint | `WBP_` | Epic | |
| Animation Blueprint | `ABP_` | Epic | |
| Animation montage | `AM_` | Epic | New montages use `AM_`, not `Anim_`. The 15 existing `Anim_` assets stay. |
| Animation sequence / blend space | `AS_` / `BS_` | Project | |
| Skeletal / static mesh | `SK_` / `SM_` | Epic | Engine mannequin assets keep `SKM_`. |
| Material / instance / function | `M_` / `MI_` / `MF_` | Epic | |
| Texture | `T_` | Epic | Add the type as a suffix: `_D` (colour), `_N` (normal), `_ORM` (packed mask). |
| Niagara system / emitter | `FXS_` / `FXE_` | Epic | |
| Enum / struct (Blueprint) | `E_` / `F_` | Epic | |
| Sound cue / wave | `SC_` / `SW_` | Project | |
| Data table / curve | `DT_` / `C_` | Epic / Project | |
| Input action / mapping context | `IA_` / `IMC_` | Project (also Lyra) | Every action name is unique (D-063, so keys can be rebound later). |
| Map | `L_` | Project (also Lyra) | New maps only, e.g. `L_Hub`, `L_Trial_Fire_01`. The current `Level/*` maps keep their names. |
| Primary asset label | `PAL_` | Project | |
| **Data asset** | `DA_<Kind>_` | Project | See below. |

To add a type that isn't listed, add its short prefix to this table in the same change.

### Data assets

- **Weapon skills** are named by weapon family, then skill kind: `DA_<WeaponFamily>_<Attack|Ability>_<Name>`. Examples are `DA_Sword_Ability_BullRush` and `DA_SAS_Attack_F`. The families in use are `Sword`, `SAS`, `Greatsword`, `Axe`, `DualAxe` and `Gauntlets`; 224 assets follow this.
- **Everything else** is named by its kind:

  | Kind | Assets |
  |---|---|
  | `DA_Spell_` | Spells |
  | `DA_Ring_` | Rings |
  | `DA_Weapon_` | Weapons |
  | `DA_Evo_` | Evolution items |
  | `DA_Equipment_` | Equipment |
  | `DA_Character_` | Characters |
  | `DA_Inventory_` | Inventories |
  | `DA_Enemy_` | Enemies |
  | `DA_Merchant_` | Merchants |
  | `DA_Trial_` | Trials |
  | `DA_Cosmetics_` | Cosmetics |
  | `DA_Stance_` | Stances |
  | `DA_Effect_` | Effects |
  | `DA_Boss_` | Bosses |

  A new kind takes the C++ class's short name, without `Data`.
- **Test and dev data** goes under `Content/Testing/`, named `DA_Test_<Kind>_<Name>`. Examples are `DA_Test_Weapon_Sword` and `DA_Test_Ability`; 23 assets already follow this.

## Folders

| Folder | Holds |
|---|---|
| `Content/Data/<Kind>/` | Design-time data assets, grouped by kind: `Characters`, `Pool`, `Skills`, `Spells`, `Trials`, `Items`, `Merchant`, `Enemy`. |
| `Content/Blueprints/` | Blueprint classes: characters, enemies and game modes. |
| `Content/Core/` | Game-wide Blueprints: game modes and hub. B-122 merges the duplicates between this folder and `Blueprints/GameModes/`. |
| `Content/UI/` | `Core/` holds CommonUI input data and styles. `UMG/<Screen>/` holds widgets. |
| `Content/VFX/` | Niagara systems and their materials. **Library assets copied from the engine keep their engine names** (`ENiagara_*`, `FN_*`). Don't copy those names for new assets. |
| `Content/Level/` | Shipping maps. New maps use the `L_` prefix. |
| `Content/Testing/` | **The one root for test and dev content.** New test maps go here too, as `L_Test_<Name>` (the three existing `Level/TestLevel_*` maps stay). Nothing in a shipping folder may reference this folder. B-122 folds `Data/Testing/` into it. |
| `Content/RPGIcons/`, `Content/MSPresets/` | Vendor icon and material-preset packs, kept as imported. Don't add to them. Project icons go next to the screen that uses them, under `UI/`. |
| `Content/UI/Vendor/`, `SourceArt/` | Planned for imported kit pieces (`UI_DeckbuilderReuse.md` §4, B-104). Nothing is tracked there yet. |

- Input assets live under `Data/Characters/Models/CharacterBase/Input/`, and new ones go there too until a move is planned. A move needs redirectors and an update to the B-205 config paths.
- Vendor packs under `Content/Imports/` stay out of git (`.gitignore`). Only the originals of assets the game uses are committed (CLAUDE.md, "LFS coverage").

## Checking

- Nothing checks names yet. An Epic `UEditorValidatorBase` validator could do it later; that is deferred (BestPractice, "Data validation").
- When you add an asset, check its name against this page.
- `Scripts/ue/test.sh` runs data validation (B-119). It checks content, not names.
