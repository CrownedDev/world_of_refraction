# Best-practice audit — 2026-10

**Job:** B-047 (research only). **Compared against:** Epic's documentation and coding standard, and the **Lyra** sample game.

**How to read it:** each area says what we do (with `path:line`, relative to `Source/world_of_refraction/`), what the standard does (Epic URLs; Lyra details are from memory and say so), the gap in plain words, and a recommendation sized for this project. Over-engineering is pushed back on, per CLAUDE.md. Gaps already found in `ArchitectureReview_2026-10.md` and `CodebaseReview_2026-10.md` are cited, not repeated. Each fix became a backlog job, and each judgement call a `D-xxx` entry.

## Summary

| Area | Where we stand | Biggest gap | Raised |
|---|---|---|---|
| Data assets | All `UPrimaryDataAsset`; 11 `IsDataValid` overrides | Asset Manager not set up; hard-coded test-asset fallbacks (one stale); validation never runs automatically; 6 assets without Print buttons | B-118–B-120, B-090 scope; D-057 |
| Content naming and folders | Epic-style prefixes, mostly consistent | Hygiene: 549 orphaned OFPA files, duplicate game modes and input data, junk assets | B-121–B-123; D-058 |
| Save and versioning | No save system (B-090) | The dormant `SaveGame` tags would lose every item's identity; no version or upgrade hook | B-124, B-125, B-090 scope; D-059, D-060 |
| Player-facing text | Newer assets use `FText`; most text is `FString` | **Enum display names show raw names in a packaged build**; asset text can't be localised | B-126, B-127, B-105/B-045 rules; D-061, D-062 |
| Input | Enhanced Input only, no legacy bindings | Turn actions are mouse-only (B-103/B-105); no glyph assets | B-128, B-129; D-063 |
| Co-op readiness | Single-player in practice | Battle level load drops a client; all fight state in GameInstance subsystems; HP doesn't replicate; rewards go to "player 0" | B-130–B-132, B-027 scope; D-064, D-065 |
| Test coverage | 35 specs (damage, scaling, stun, hang fixes, turn order, defense clock) | Effects, buildup, infusion cost, defense matching, AI, economy, grid untested; no baseline for the "identical before/after" jobs | B-133–B-137; D-066, D-067 |
| Performance | Event-driven overall | A weapon mesh ticks to poll; per-frame lookups in the anim instance; sync VFX loads at cast | B-138–B-142; D-068 |

**Not recommended** (the standard does it, but it would be over-engineering here):
- Game Feature plugins, a folder restructure
- a custom `UAssetManager` subclass or bundles
- `FCustomVersion` or a hand-written `Serialize`
- Lyra's `ULyraInputConfig` + hero component, before GAS's real-time stage
- an editor validator for prefixes
- translations now (localisation-ready only)

**Departs from Lyra on purpose:** the ability system component sits on the character, not on PlayerState (`Design/GASMigration.md`), because one player leads up to three characters.

## Areas

### Data assets (UPrimaryDataAsset, Asset Manager, references, validation)

**What we do:**
- All 11 data-asset bases are `UPrimaryDataAsset`, as CLAUDE.md asks: `UCharacterData` (`Public/Character/CharacterData.h:111`), `UCosmeticsData` (`CosmeticsData.h:23`), `UStanceData`, `UEvolutionItemData`, `UEquipmentDataBase` (`Public/Equipment/EquipmentDataBase.h:48`, which Weapon and Ring derive from), `UInfusionDisplayData`, `UInventoryData`, `UMerchantData`, `USkillDataBase` (`Public/Skills/Definitions/SkillDataBase.h:108`, which Spell and Ability derive from), `UEffectDefinition`, `UTrialData`.
- **Asset Manager: not set up.** `Config/DefaultGame.ini:1-12` still has the template's scan list: `Map` (scanning `/Game/Maps`, but our maps live in `/Game/Level/`), `PrimaryAssetLabel`, and `GameFeatureData` (the GameFeatures plugin isn't enabled in the `.uproject`). None of our own types are registered. Only `UInventoryData` overrides `GetPrimaryAssetId` (`Private/Inventory/InventoryData.cpp:281`). `EffectDefinition.h:39-41` notes that the others deliberately keep the engine default.
- **Runtime lookups use hard-coded paths.** There's no Asset Manager or Asset Registry query anywhere in `Source/`. `CombatOrchestrator.cpp` has four `LoadObject` fallbacks to test assets (`:1970`, `:2053`, `:2391`, `:2569`). One of them points to a file that isn't in git: `/Game/Data/Spells/Fire/DA_Spell_Fireball` (`:2054`). The real file is `Data/Spells/Fire/Destruction/DA_Spells_Fire_FireBall`. `Config/DefaultGame.ini:14-26` sets config-driven soft paths for the shop window, hub level, solo pawn and CommonInput data. That's a good pattern.
- **References are mostly hard.** About 99 `UPROPERTY` hard pointers (mostly raw `U*`, some `TObjectPtr`). They include montages, meshes and Niagara VFX on data assets: `WeaponData.h:57-124`, `SpellData.h:55-81`, `CosmeticsData.h:32-75`, `SkillDataBase.h:295-310`. Soft refs are used in 19 places, in the right spots: levels (`TrialData.h:45,51`, `TrialRunSubsystem.h:99,117`), pawn classes (`Party.h:54`, `BattleGameMode.h:32-41`), and the newer VFX entries (`SkillCastEntry.h:59`, `SkillVFXEntry.h:60`, loaded with `LoadSynchronous` in `ActionExecutor.cpp:3452-3454`). The legacy hard fields `SpellData::SpellVFX/ImpactVFX/MuzzleVFX` sit alongside the soft entries as fallbacks.
- **Validation is good.** There are 11 `IsDataValid` overrides, all with `WITH_EDITOR` guards and `Super` calls: CharacterData, EquipmentDataBase→Weapon/Ring, EvolutionItemData, InventoryData, MerchantData, SkillDataBase→Spell/Ability, EffectDefinition. Missing: `UTrialData`, `UStanceData`, `UCosmeticsData`, `UInfusionDisplayData`. Nothing runs validation outside the editor. `Scripts/ue/test.sh` has no `DataValidation` step, so a bad asset only shows up when someone saves it.
- **CallInEditor buttons are partial.** Assets with Details-panel buttons: `UEquipmentDataBase` (5), `UEvolutionItemData` (2), `UMerchantData` (2, including `ValidateStock`, `MerchantData.h:117-123`), `UEffectDefinition` (1). Assets with none: `UCharacterData`, `USkillDataBase`/Spell/Ability, `UInventoryData`, `UTrialData`, `UStanceData`, `UCosmeticsData`. These rely on static `UBlueprintFunctionLibrary` debug classes (`Public/Debug/Character/CharacterDataDebug.h:16`, `Debug/Skills/SpellDataDebug.h`, etc.), which are driven from placed tester BPs (`Content/Data/Characters/BP_CharacterDataTester`, `Data/Spells/BP_SpellDataTester`, `Data/Items/BP_ItemDataTester`). That falls short of CLAUDE.md's own rule: "For data assets: CallInEditor Print/Log buttons in Details panel".
- Already found elsewhere: the TrialRun weak pointer to a data asset (`CodebaseReview_2026-10.md:115`). Stable save IDs are part of B-090.

**What the standard does:**
- Epic, Asset Management (https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine): each primary type is registered in `PrimaryAssetTypesToScan` with a base class, directories and a cook rule. The Asset Manager can then list, load (sync or async, with bundles) and cook assets by `FPrimaryAssetId` (`Type:Name`), without path strings. A `UPrimaryDataAsset` subclass gets a default ID from its native class. Overriding `GetPrimaryAssetId` on a base groups subclasses under one type.
- Epic, Referencing Assets (https://dev.epicgames.com/documentation/en-us/unreal-engine/referencing-assets-in-unreal-engine): hard refs load the whole dependency chain. Use `TSoftObjectPtr`/`TSoftClassPtr` for heavy or optional content, and load it on demand.
- Epic, Data Validation (https://dev.epicgames.com/documentation/en-us/unreal-engine/data-validation-in-unreal-engine): `IsDataValid` runs on save by default, from Content Browser "Validate Assets", from Tools > Validate Data, and in CI via `UnrealEditor-Cmd.exe <proj>.uproject -run=DataValidation`. `UEditorValidatorBase` subclasses add project-wide rules such as naming. They're auto-discovered by `UEditorValidatorSubsystem`.
- Lyra (from memory, not re-checked against source):
  - `ULyraAssetManager` subclasses `UAssetManager` and loads a `ULyraGameData` primary asset at startup from a config path.
  - `DefaultGame.ini` registers `LyraExperienceDefinition`, `LyraUserFacingExperienceDefinition`, `LyraExperienceActionSet`, `LyraGameData`, `GameFeatureData` and `Map`.
  - Experiences are loaded by `FPrimaryAssetId`, async, with bundles.
  - Definitions like `ULyraExperienceDefinition` and `ULyraPawnData` are `UCLASS(Const)` `UPrimaryDataAsset`s with `IsDataValid` under `WITH_EDITOR`.
  - The `LyraEditor` module adds `UEditorValidator` subclasses (Blueprint compile, load-check, source-control) on top of per-asset `IsDataValid`.
  - Pickup and weapon visuals are mostly `TSoftObjectPtr`/`TSoftClassPtr`.
- UE5 coding standard: `TObjectPtr<>` for `UPROPERTY` object members.

**Gap:**
1. Our data types are invisible to the Asset Manager. Code finds assets by hard-coded string paths, and one of those paths is already stale. An asset that's reached only by a string path or by B-090's future ID lookup isn't guaranteed to be cooked.
2. Validation exists but nothing enforces it. Four types have no checks.
3. Six data-asset types have no Details-panel debug button, which breaks our own CLAUDE.md rule.
4. Visual hard refs (montages, meshes, VFX) on design data load everything up front. That's fine at today's size, and the newer soft entries already show the better pattern.
5. Raw `U*` and `TObjectPtr` are mixed in `UPROPERTY` declarations. That's only cosmetic.

**Recommendation:**
- Do the Asset Manager setup as part of B-090, not separately.
  - Register 7 types: `Character`, `Skill`, `Equipment`, `Evolution`, `Effect`, `Merchant`, `Trial`, with `Directories=/Game/Data` and `CookRule=AlwaysCook`.
  - Pin each type with a `GetPrimaryAssetId` override on the base classes, so a Spell and an Ability share one `Skill` type and a save holds `Skill:DA_Spell_Bolt`.
  - Fix the `Map` scan directory to `/Game/Level`, and drop the unused `GameFeatureData` line.
  - Don't add a custom `UAssetManager` subclass or bundles until there's a loading-time problem.
- Replace the four `LoadObject` fallbacks in the orchestrator's debug commands with a logged failure. A debug command with nothing set should be loud, not silently load a test asset. Flag the code first and remove it only after PIE.
- Add `-run=DataValidation` to the test script, so bad data fails the same gate as tests. Add `IsDataValid` to `UTrialData` (Level set), `UCosmeticsData` and `UStanceData`. Defer writing a project-wide `UEditorValidatorBase`; see the naming area.
- Add `CallInEditor` Print buttons that call the existing `*Debug` libraries. That's about one line each.
- Soft refs (pending **D-057**): recommended, no migration now; new visual or audio fields on data assets use `TSoftObjectPtr`, and the SpellData legacy hard VFX fields retire when the SkillCastEntry path is proven. Change `TObjectPtr` only when a file is already being edited.

**Raised:**
- Jobs: **B-118** (drop the hard-coded test-asset fallbacks), **B-119** (run DataValidation in `test.sh`), **B-120** (validation and Print buttons on the six assets without them). The Asset Manager setup is added to **B-090**'s scope, as its first step.
- Decision: **D-057** (hard vs soft references for heavy visuals).

### Content naming and folders

**What we do:**
- 1,525 tracked files under `Content/`.
- **Prefixes are mostly Epic-style.** The counts are `DA_` 456, `T_` 168, `BP_` 44, `WBP_` 17, `IA_` 15, `IMC_` 2, `AM_` 2, `DT_` 1, `ABP_` 1, `M_` 1, `MF_` 7. Data assets follow `DA_<Kind>_<Name>` (`DA_Spell_Bolt`, `DA_Ring_Generic`, `DA_Test_Weapon_Sword`).
- **Naming drift:**
  - `Anim_` ×15 (`Data/Skills/Weapons/Anim_Approach_Leap`) isn't in Epic's list.
  - `SKM_` ×3: UE5's own mannequin uses it; Epic's page says `SK_`.
  - Maps have no prefix: `Level/LevelFireLord`, `TestLevel_Nav`.
  - `DA_Spell_Fireball` vs `DA_Spells_Fire_FireBall`.
  - Two junk `NewBlueprint.uasset` files (`Content/`, `Content/Level/`).
  - Unprefixed vendor textures in `MSPresets/MSTextures/` (Quixel Bridge presets).
- **Folders are organised by asset type, not by feature:** `Blueprints/` (Characters, Enemies, GameModes), `Core/` (GameModes, Hub), `Data/` (Pool 321, Characters 78, Skills 44, Spells 37, …), `Testing/` *and* `Data/Testing/`, `Level/`, `UI/` (Core, UMG), `VFX/` (Niagara library), `RPGIcons/`, `MSPresets/`.
  - Input assets live under `Data/Characters/Models/CharacterBase/Input/`.
- **Duplicates:**
  - Game mode Blueprints: `BP_Hub_GameMode` in both `Core/Hub/` and `Blueprints/GameModes/`. `BP_CombatGameMode` (`Core/GameModes/`) vs `BP_Combat_GameMode` (`Level/`), with a third combat mode in `Blueprints/GameModes/`.
  - `BP_CommonInputData` in both `UI/` and `UI/Core/`; config uses `UI/Core` (`DefaultGame.ini:26`).
  - Two of these are already noted: `docs/Gaps/Roadmap.md:98` and `docs/Design/Resources_Design.md:1250`. Neither has a backlog job.
- **Orphaned One File Per Actor (OFPA) files:** 566 tracked files under `Content/__ExternalActors__/` and `__ExternalObjects__/`. They belong to maps that aren't in git: `ThirdPerson/Maps/ThirdPersonMap`, `VehicleTemplate/Maps/Vehicle*ExampleMap`, and the vendor `Imports/` packs, which are ignored per `.gitignore:15`. `.gitignore:16-17` now ignores both folders wholesale, so these files are leftovers committed before the ignore. The blanket ignore is also a trap: if a tracked level ever uses OFPA (World Partition), its actors would silently stay out of git.
- **Redirects:** 134 `CoreRedirects` lines (`Config/DefaultEngine.ini:26-233`), including chains (`WeaponData.Name` → `EquipmentDataBase.Name`, `:50,70`). Already noted in `CodebaseReview_2026-10.md:201`.

**What the standard does:**
- Epic, Recommended Asset Naming Conventions (https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects):
  - The pattern is `[Prefix]_[AssetName]_[Descriptor]_[Variant]`.
  - Prefixes: `BP_`, `WBP_`, `DT_`, `T_`, `M_`, `MI_`, `SM_`, `SK_`, `ABP_`, `AM_` (montage), `FXS_` (Niagara system), `E_`, `F_`.
  - The page gives no prefix for Data Assets, Input Actions or Maps. It says to extend the list consistently.
- Epic, Core Redirects (https://dev.epicgames.com/documentation/en-us/unreal-engine/core-redirects-in-unreal-engine): redirects are a transition tool. Once every referencing asset has been resaved (e.g. `-run=ResavePackages`), they can be removed.
- Lyra (from memory):
  - `/Game/` holds shared, feature-grouped content: `Characters/`, `Weapons/`, `Effects/`, `Environments/`, `Input/`, `System/` (experiences, default game data), `UI/`, `Audio/`.
  - Each game mode is a Game Feature plugin (`Plugins/GameFeatures/ShooterCore`, `TopDownArena`) with its own Content.
  - Lyra uses its own prefixes (`B_` Blueprints, `W_` widgets, `IA_`/`IMC_`, `L_` maps) and applies them consistently. The lesson is consistency over the exact list.
  - Lyra keeps developer and test content out of shipping folders.

**Gap:** The prefix discipline is good. The real issues are hygiene:
- duplicate game modes and input data
- junk `NewBlueprint` assets
- 549 orphaned OFPA files, plus a blanket `.gitignore` that would hide a real level's actors
- test content split across two folders
- a long redirect list that's never pruned

The folder layout is by asset type rather than by feature. That's workable at this size and not worth a big move. Mass moves risk `.uasset` breakage, per CLAUDE.md's git-discipline note. Game Feature plugins aren't worth it for a single-mode RPG.

**Recommendation:**
- Don't restructure folders and don't adopt Game Feature plugins.
- Write a one-page naming and folder convention: Epic's list plus our additions (`DA_<Kind>_`, `IA_`/`IMC_`, `L_` for new maps, `FXS_`/`NS_`, `A_` or keep `Anim_`), with test content in one `Testing/` root. Apply it to new assets only.
- Clean up the duplicates in the editor: use the Reference Viewer, delete the unused copy, then Fix Up Redirectors.
- Untrack the orphaned OFPA files with `git rm --cached` (the files stay on disk). Narrow the ignore to the vendor and template map paths.
- Prune CoreRedirects only after a full resave, and leave that until a quiet week. The risk is real and the payoff is small.

**Raised:**
- Jobs: **B-121** (untrack the 549 orphaned OFPA files, narrow `.gitignore`), **B-122** (duplicate and junk content cleanup), **B-123** (one-page naming and folder convention). The redirect prune is noted under Later.
- Decision: **D-058** (rename existing drift, or apply the convention to new assets only). An editor validator for prefixes is not recommended now.

### Save and versioning

**What we do:**
- There is no save system. The source has no `USaveGame` subclass and no calls to `SaveGameToSlot`, `AsyncSaveGameToSlot` or `FCustomVersion`. `docs/analysis/CodebaseReview_2026-10.md:54,93` already reports this, scores it 0/5, and notes that the `PersistentID` is minted again on every spawn (`Private/Inventory/InventoryComponent.cpp:305,441`, and again in `Private/Pool/PoolSubsystem.cpp:258,270`). `docs/analysis/ArchitectureReview_2026-10.md` §1.5 already covers three things: stable IDs, two-phase load, and keeping Blueprint structs out of saved data. This section doesn't repeat them.
- The future save root is `UPoolSubsystem`, which is account-wide and GameInstance-scoped (`Public/Pool/PoolSubsystem.h:54-230`). Its storage is `SaveGame`-tagged but described as "DORMANT" (`:23-26`, `:170-229`). Gold is deliberately left untagged (`:206-211`).
- **New finding: the `SaveGame` tags don't cover the data that makes an item what it is.** With `ArIsSaveGame`, which is how `USaveGame` and the proxy-archive pattern serialise, only tagged fields are written.
  - `FWeaponInventoryEntry` tags only `PersistentID`, `Tier` and `Quality` (`Public/Loadout/Entries/FWeaponInventoryEntry.h:59-72`).
  - Left untagged in that struct: the `Weapon` asset itself (`:41-42`), `AttachedItem` (`:75-76`), `AssignedSpells` (`:79-80`), the per-instance rolls `StatBonus` and `ResistanceBonus` (`:87-95`), and the four roll pools (`:103-113`).
  - `FRingInventoryEntry` has the same gaps (`Public/Loadout/Entries/FRingInventoryEntry.h:45-116`).
  - `FRuntimeAttachedItem` tags its inner fields (`Public/Equipment/FRuntimeAttachedItem.h:30-45`), but the entry field holding it is untagged, so those inner tags never take effect.
  - `FSpellInstance` does tag its asset pointer (`Public/Loadout/Entries/FSpellCollection.h:34-48`), so the convention is inconsistent.
  - Result: if you saved the pool today, every weapon would come back as a GUID with a tier and a quality, and no weapon.
- Saved references to assets are raw `UObject*` pointers: `USpellData*`, `UWeaponData*` and `URingData*` throughout `Public/Loadout/FSavedLoadout.h:37-38,106,187,200,211`.
- No save data carries a version field.
- `Config/DefaultEngine.ini:197-217` already uses CoreRedirects so that renames don't break name-based saved data. That habit is good and worth keeping.
- `FPrimaryAssetId` is used only by `UInventoryData` (`Private/Inventory/InventoryData.cpp:281-283`).
- Transitions wipe actor-held state: `Public/Trial/TrialRunSubsystem.h:12-17` carries a "STATE WIPE" warning. The trial subsystem holds only hard pointers to the pending party (`:107-111`).
- There is no settings save: no `UGameUserSettings` subclass, and nothing like it in `Config/`.

**What the standard does:**
- **Epic, async saves.** `UGameplayStatics::AsyncSaveGameToSlot` serialises on the game thread, writes on a worker thread, and calls a delegate when done. Epic recommends it for most saves because it avoids hitches and a certification issue on some platforms. The synchronous call is fine for small saves or saves made from a menu. Sources: https://dev.epicgames.com/documentation/unreal-engine/saving-and-loading-your-game-in-unreal-engine and https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UGameplayStatics/AsyncSaveGameToSlot
- **Epic, built-in versioning.** `ULocalPlayerSaveGame` (UE 5.3 and later) is a `USaveGame` with versioning built in. `GetLatestDataVersion()` is stamped on every write, `HandlePostLoad()` (the signature is from memory) is where old saves get upgraded, and `AsyncLoadOrCreateSaveGameForLocalPlayer` / `AsyncSaveGameToSlotForLocalPlayer` do the loading and saving. Source: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/ULocalPlayerSaveGame
- **`FCustomVersion`** (`FCustomVersionRegistration` plus `Ar.UsingCustomVersion`) is the engine's own way to version binary formats. Writing a custom `Serialize` for gameplay saves is more than most games need. A single integer version on the save object, plus per-step upgrade code, is the common game-level pattern.
- **Lyra (from memory).**
  - `ULyraSettingsShared` derives from `ULocalPlayerSaveGame`. It holds per-player preferences such as subtitles, colour-blind mode, gamepad sensitivity and language, and is saved to its own slot through the async local-player API.
  - `ULyraSettingsLocal` derives from `UGameUserSettings`. It holds per-machine settings such as graphics, audio volumes and frame rate, and is stored in `GameUserSettings.ini`.
  - Lyra keeps settings and progress apart. It has no inventory save to copy.
- **Stable IDs.** Saved data should refer to designer assets by `FPrimaryAssetId` or a soft path, and to owned instances by a GUID minted once.
  - The `ArIsSaveGame` proxy archive (`FObjectAndNameAsStringProxyArchive`) does turn raw `UObject*` pointers into object paths and loads them back. They survive, but only as long as the asset is never moved or renamed without a redirector.
  - `FPrimaryAssetId` survives folder moves and lets the Asset Manager resolve the asset (from memory, the usual community guidance).

**Gap:**
1. No save exists. This is known and is B-090.
2. The `SaveGame` tags are already wrong for item instances, so the dormant groundwork would silently lose every item's identity and its rolls.
3. Saved data has no version number and no upgrade hook, in a project that renames enums and structs often. The CoreRedirects cover renames but not changes of meaning, such as D-027 removing weapon spells.
4. References to assets are raw pointers that only survive while paths stay stable.
5. Settings have nowhere to live.

**Recommendation:**
- Build B-090 on `ULocalPlayerSaveGame`, not on bare `USaveGame`. It gives the version stamp, the upgrade hook and the async load/save for free, with no custom archive code.
- Use one progress save (`UWorSaveGame`) that holds a copy of the pool's data struct, so the pool and the save don't keep two layouts. Upgrade old saves in `HandlePostLoad` with a `switch` on the saved version, one case per step.
- Use the async save when leaving a fight or a trial, and the synchronous save from menus.
- Before B-090 starts, fix the tag coverage, or choose to give up on tag-driven saving: for a pool-sized save, an explicit `USTRUCT` copied wholesale (no `ArIsSaveGame` filter) is simpler than auditing 88 tags.
- Store asset references in saved data as `FPrimaryAssetId`, or as `TSoftObjectPtr` at least. Keep the CoreRedirects habit.
- Don't use `FCustomVersion` or a hand-written `Serialize`. That's over-engineering for this game.
- Leave settings until there's an options screen: a `UGameUserSettings` subclass for per-machine settings, and a second small `ULocalPlayerSaveGame` for per-player preferences, the same split Lyra uses.

**Raised:**
- Jobs: **B-124** (save tags cover item identity, plus a round-trip test), **B-125** (settings save split, after the settings screen). B-090's scope gains `ULocalPlayerSaveGame` versioning, async save, `FPrimaryAssetId` references and `wor.PrintSave`.
- Decisions: **D-059** (tags or explicit save structs), **D-060** (wipe or upgrade old dev saves).

### Player-facing text

**What we do:**
- **The core data assets keep their display text in `FString`.**
  - `UEquipmentDataBase::Name` / `Description` (`Public/Equipment/EquipmentDataBase.h:64,70`)
  - `USkillDataBase::Name` / `Description` (`Public/Skills/Definitions/SkillDataBase.h:125,128`)
  - `UCharacterData::Name` / `Description` (`Public/Character/CharacterData.h:119,146`)
  - `UStanceData` (`Public/Character/StanceData.h:29`), `UInfusionDisplayData` (`Public/Infusion/InfusionDisplayData.h:26,29`), `UEvolutionItemData` (`Public/Equipment/Crystals/EvolutionItemData.h:75`)
  - Status names are hard-coded English `TEXT()` in a 367-line header (`Public/Skills/Effects/SkillEffectDisplayNames.h:13-17,27+`). So are crystal type names (`Public/Equipment/Crystals/ItemIdentity.h:191`).
- **Newer code already uses `FText`:** `UTrialData::Name` / `Description` (`Public/Trial/TrialData.h:30,33`), `UParty::DisplayName` (`Public/Party/Party.h:99`), `UEffectDefinition::DisplayName` (`Public/Skills/Effects/EffectDefinition.h:27`), and `FPieMenuButtonData` (`Public/UI/Combat/PieMenuButtonData.h:101,105`).
- **Player-facing `FText` is nearly all `FText::FromString`.** There are 110 calls in total. About 70 of them are player-facing UI; most of the rest are data-validation messages, which is fine.
  - Button labels: `Button.DisplayName = FText::FromString(TEXT("Attack"))` (`Private/UI/Combat/CombatCommandMenuSubsystem.cpp:597,608,620,643,656,673`).
  - Numbers built with `FString::Printf`: `"%d abilities"` (`:609-610`, which also can't handle plurals) and `"%s:%d/%d"` (`Private/UI/Combat/CommandMenu/DurabilityHeaderWidget.cpp:196-265`).
  - The shop rows pass `FString` names straight through (`Private/UI/Shop/ShopRowWidget.cpp:51-67`).
- **Localisable text keys are rare.** The C++ has only 11 `NSLOCTEXT` uses, all in `ShopWindowWidget.cpp:328-529` and `PartySessionSubsystem.cpp:489-493`, which is also the only `FText::Format`. It has no `LOCTEXT_NAMESPACE` and no String Tables, and `Config/` has no localisation target.
- **Names are built by joining words.** `USpellData::GetDisplayName` glues the element onto the spell name: `"Reality %s"`, plus an element prefix (`Private/Skills/Definitions/SpellData.cpp:160-192`). The description generators in `Public/Equipment/Crystals/CrystalDescription.h` (`:72,77`, with `.ToLower()` on a name) and `FSkillEffect::GetDescription` (`Public/Skills/Effects/FSkillEffect.h:191-290`) build sentences with `FString` and `Printf`.
- **Enum display names are shown to players.**
  - `GetDisplayNameTextByValue` is used for the element name in the combat panel (`Private/UI/Combat/CharacterPanelWidget.cpp:624-629`), in spell names (`SpellData.cpp:176-179`), and as a fallback for status names (`SkillEffectDisplayNames.h:281-284`).
  - The 404 `UMETA(DisplayName=...)` entries are otherwise editor-facing.

**What the standard does:**
- **Epic: `FText` for anything the player sees.**
  - Literals go through `LOCTEXT` / `NSLOCTEXT`, so the gather step picks them up.
  - Runtime text goes through `FText::Format` with named or ordered arguments and `|plural(one=…,other=…)`, plus `FText::AsNumber` / `AsPercent` for numbers.
  - `FText::AsCultureInvariant` / `INVTEXT` are for text that must never be translated, such as player-typed names.
  - `FString` and `FName` are for identifiers and logs.
  - Converting `FText` to `FString` throws away its history, so the text no longer updates when the language changes.
  - `FText` properties on assets are gathered automatically. String Tables hold shared, reused text (from memory).
  - Sources: https://dev.epicgames.com/documentation/en-us/unreal-engine/text-localization-in-unreal-engine and https://dev.epicgames.com/documentation/en-us/unreal-engine/ftext-in-unreal-engine
- **Enum display names in shipped games.** Epic's API page says `UEnum::GetDisplayNameTextByIndex`/`ByValue` "normally" return the short name in a cooked build, because the metadata is stripped. A player would see something like the raw enum name in place of the friendly one. Source: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/CoreUObject/UEnum/GetDisplayNameTextByIndex
- **Lyra (from memory)** keeps player text in `FText`: `ULyraExperienceActionSet` / `UCommonActivatableWidget` titles, `ULyraInventoryItemDefinition::DisplayName` and the settings registry's `SetDisplayName(LOCTEXT(...))`. Each .cpp has its own `LOCTEXT_NAMESPACE`, and the project ships a `Lyra` localisation target in `Config/Localization/`.

**Gap:**
1. The main asset text (equipment, skill and character names and descriptions) is `FString`, so it can never be gathered or translated, and it can't take part in `FText::Format`.
2. UI text is all `FromString`. The literal strings in C++ aren't gatherable, and the numbers in labels don't follow the player's language or plural rules.
3. Generated text (spell names now, D-031's description tree in B-045) joins English words in a fixed order, which breaks in languages that order words differently.
4. Enum display names will show raw internal names in a packaged build. This is a real bug for the first cooked build, not just a localisation issue.
5. There's no localisation target. That's fine today.

The cost of fixing this rises with every screen built on `FString`, and the CommonUI rebuild (B-103 onwards) and B-045 are about to add many.

**Recommendation:**
- Don't localise now: no translations and no culture switcher. Recommended (pending **D-061**): be **localisation-ready**: all new player text is `FText`, and nothing new adds to the old `FString` habits.
- If D-061 is answered that way, concretely:
  - **(a)** B-045 produces `FText` from day one. Its tree generator should assemble each line with `FText::Format` patterns held in one String Table or one `LOCTEXT_NAMESPACE("Descriptions")` file, so the deckbuilder's text grammar becomes a set of format patterns, not `Printf`.
  - **(b)** The CommonUI screens take `FText` only.
  - **(c)** Change the asset `Name`/`Description` fields from `FString` to `FText` in one pass. The data should carry over: I believe `FTextProperty` loads an old `FString` value under the same property name (`ConvertFromType`, from memory), but verify on one asset in the editor before doing the rest.
  - **(d)** Replace the enum display-name lookups with an explicit `FText` table, such as a `switch` returning `LOCTEXT` or a String Table, the same as `SkillEffectDisplayNames` does, but in `FText`.
- Skip String Tables for asset text: an `FText` property on the asset is already gathered. Use one String Table only for shared words: element names, status names, trigger names, and the button verbs Attack, Abilities, Resonate.

**Raised:**
- Jobs: **B-126** (enum names that work in cooked builds, plus status and type names on the same table; waits on D-062), **B-127** (asset `Name`/`Description` to `FText`). The menu and HUD text rule is added to **B-105**, and the `FText` output rule to **B-045**.
- Decisions: **D-061** (localisation scope), **D-062** (where shared game words live).

### Input (Enhanced Input + CommonUI)

**What we do:**
- Enhanced Input is the engine default (`Config/DefaultInput.ini:81-82` sets `EnhancedPlayerInput` / `EnhancedInputComponent`). There are no legacy `ActionMappings`/`AxisMappings` and no `BindAxis`/legacy `BindAction` anywhere in Source. The only `BindAction` calls are Enhanced (`Private/Combat/CombatPlayerController.cpp:64-79`).
- Combat: `ACombatPlayerController` adds `IMC_Combat` at a literal priority `0` in BeginPlay and removes it in EndPlay (`Private/Combat/CombatPlayerController.cpp:18-49`). It binds 4 actions (Block, Parry, DodgeLeft, DodgeRight) on `Started`, straight into the GI subsystem `UDefenseSystem` (`:95-145`). The Infusion / Infusion-level actions are commented-out placeholders (`Public/Combat/CombatPlayerController.h:48-73`). B-093 fills them.
- Turn actions (Attack / Skill / Defend …) come from the command menu, whose buttons are plain `UButton` + `OnClicked` (`Public/UI/Combat/CommandMenu/CombatCommandButtonWidget.h:44-45`, `Private/UI/Combat/CommandMenu/CombatCommandButtonWidget.cpp:19`). That means mouse only: no focus or gamepad navigation path in C++.
- Hub/trial: `AWoRHubController` only builds the CommonUI root layout (`Private/Player/WoRHubController.cpp:18-46`). Its BP adds `IMC_ThirdPersonChar` and handles `IA_Interact` / `IA_ToggleMenu` in the graph (header comment `Public/Player/WoRHubController.h:7-11`). The native replacement `AExplorationPlayerController` lives on `refactor/bp-logic-to-cpp`, unmerged (B-003). It takes its default `IA_Interact` from a hard-coded `ConstructorHelpers` path.
- CommonUI: the plugin and modules are wired (`world_of_refraction.Build.cs:15-19`), with `CommonGameViewportClient` (`Config/DefaultEngine.ini:235`), `bEnableEnhancedInputSupport=True`, and `InputData=/Game/UI/Core/BP_CommonInputData` (`Config/DefaultGame.ini:24-26`). `UWoRActivatableWidget::GetDesiredInputConfig` returns `ECommonInputMode::Menu` (`Private/UI/WoRActivatableWidget.cpp:31`). Its header bans `SetInputMode` in derived screens (`Public/UI/WoRActivatableWidget.h:6-14`).
- Still on the UMG-era path: `UMerchantShopSubsystem` calls `SetInputMode(FInputModeUIOnly)`, `SetShowMouseCursor` and `SetGamePaused` (`Private/Shop/MerchantShopSubsystem.cpp:62-66, 86-92`).
- Assets: two copies of `BP_CommonInputData` (`Content/UI/BP_CommonInputData.uasset` and `Content/UI/Core/…`; config points at `Core`). There is a `DT_CommonInputActions` data table (`Content/UI/Core/`), but no `UCommonInputBaseControllerData` assets (keyboard / gamepad glyph sets) and no platform settings in `Config/`. Input actions live under a character-model folder: `Content/Data/Characters/Models/CharacterBase/Input/{Combat,OpenWorld}/`.
- Player-mappable keys: none. `UEnhancedInputUserSettings` is not enabled and no IMC is registered with it.

**What the standard does:**
- Enhanced Input: one mapping context per mode, priorities chosen deliberately, and `UEnhancedInputUserSettings` + `UEnhancedPlayerMappableKeyProfile` for rebinding. Mappings flagged "player mappable" with a unique name. `RegisterInputMappingContext`, `MapPlayerKey`, `SaveSettings` (https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/EnhancedInput/UEnhancedInputUserSettings, https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/EnhancedInput/UserSettings/MapPlayerKey). Epic's Parrot sample notes that runtime remapping is still marked experimental (https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-parrot-for-unreal-engine).
- CommonUI: buttons are `UCommonButtonBase` with a triggering Enhanced `UInputAction`, and `UCommonUIInputData` names the Click and Back actions (`EnhancedInputClickAction` / `EnhancedInputBackAction`). Glyphs come from `UCommonInputBaseControllerData` assets listed in `UCommonInputPlatformSettings`, whose `DefaultGamepadName` must exactly match the gamepad name in a controller-data asset. `UCommonInputSubsystem` switches keyboard ↔ gamepad automatically (https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonInput/UCommonInputPlatformSettings, https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/CommonInput/UCommonUIInputData, https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-quickstart-guide-for-unreal-engine). Screens own input config through `GetDesiredInputConfig`. Nobody calls `SetInputMode` by hand.
- Lyra (from memory): `ULyraInputConfig` is a data asset mapping GameplayTag → `UInputAction` (native and ability actions). `ULyraHeroComponent::InitializePlayerInput` adds the pawn data's mapping contexts (with `bRegisterWithSettings`) and binds by tag through `ULyraInputComponent`. `ULyraSettingsLocal` / `ULyraSettingsShared` persist key profiles. Glyphs come from per-platform controller data in `CommonInput` platform settings.

**Gap:**
1. The command menu, the main turn input, cannot be driven by keyboard or gamepad. It has no focus path and no CommonUI action binding. This is the largest real gap, and B-103 / B-105 already cover it (CommonButtonBase rebuild of the combat HUD).
2. CommonUI is configured, but there are no glyph assets or platform settings. Gamepad prompts will show nothing until they exist.
3. One live screen (the shop) fights the CommonUI input-config model with `SetInputMode` + pause. When the root layout and an activatable screen are both up, the last caller wins.
4. No rebinding path, and keys are not flagged mappable. That is cheap to add later as long as action names stay unique.
5. Small hygiene points: a duplicate `BP_CommonInputData`; a literal priority `0` with no named constant (CLAUDE.md "no magic numbers"); input assets stored under a character-model folder (the Content-naming area may cover this).
6. Lyra's tag-keyed `InputConfig` + hero component is built for ability-heavy, pawn-swappable real-time games. We have 4 combat keys and about 10 exploration keys, so it would be over-engineering now. It becomes relevant at GAS stage 4 (real-time field abilities, `GASMigration.md`).

**Recommendation:**
- Keep the current shape: one PC per mode, each owning its IMC. Name the priorities (`constexpr int32 COMBAT_IMC_PRIORITY = 0;`). Don't adopt `ULyraInputConfig` until field abilities exist.
- Let B-103 / B-105 / B-109 carry the menu and shop onto CommonUI. Make sure B-103's "input routing for keyboard + gamepad" includes glyph controller-data assets and the `DefaultGamepadName` match, and that the shop's `SetInputMode` path is retired by B-109 → B-106 rather than left as a second model.
- B-093 should bind Defend / Infusion through Enhanced Input on the combat PC (as written), not through `DT_CommonInputActions`. Data-table actions are the pre-Enhanced CommonUI path, and with `bEnableEnhancedInputSupport` the `UInputAction` is the single source of truth.
- Only if **D-063** is answered A (rebinding now): when B-108 (settings screen) lands, enable Enhanced Input user settings, register both IMCs, and flag their mappings player-mappable with stable names.

**Raised:**
- Jobs: **B-128** (CommonUI glyph assets and platform settings), **B-129** (input tidy-up: priority constants, no hard-coded asset path). The duplicate `BP_CommonInputData` is in **B-122**. The command menu going keyboard and gamepad is already **B-103/B-105**, and the Defend/Infusion keys are **B-093**.
- Decision: **D-063** (key rebinding in the first playable build).

### Co-op readiness (replication model)

**What we do:**
- Authority helpers: `UCharacterDataComponent::HasServerAuthority` (`Private/Character/CharacterDataComponent.cpp:450-458`) and `UCurrencyComponent::HasServerAuthority` (`Private/Currency/CurrencyComponent.cpp:231-238`) are copies (`NM_Standalone || Owner->HasAuthority()`). `UEconomyService` uses raw `Owner->HasAuthority()` 10× (`Private/Currency/EconomyService.cpp:41,103,…,1034`). B-027 unifies these.
- Replicated state: `UCurrencyComponent` really replicates (`SetIsReplicatedByDefault` `CurrencyComponent.cpp:41`, FastArray `Public/Currency/CurrencyTypes.h:48-85`). `UCharacterDataComponent` has `DOREPLIFETIME` for HP / EP / alive / BD (`CharacterDataComponent.cpp:178-185`) but never calls `SetIsReplicatedByDefault`; its constructor is `:20-29`, which CodebaseReview §4 already found. Its "`Server*`" functions are plain `BlueprintCallable`, not RPCs (`Public/Character/CharacterDataComponent.h:182-202`). There are no `Server` / `Client` / `NetMulticast` RPCs anywhere, and no `AGameState` / `APlayerState` subclass or use.
- Per-fight authority lives in `UGameInstanceSubsystem`s, which exist per process and never replicate: `TurnManager`, `ActionExecutor`, `DefenseSystem` (`Public/Combat/Defense/DefenseSystem.h:235`), `SkillEffectManager`, `StatusBuildupManager`, `CombatCommandMenuSubsystem`, and others (ArchitectureReview §1.1).
- Single-player assumptions: `GetFirstPlayerController` / `GetPlayerController(this, 0)` at `Private/Combat/CombatOrchestrator.cpp:873`, `Private/Currency/EconomyService.cpp:1603,1634`, `Private/Pool/PoolSubsystem.cpp:449,496,519`, `Private/Trial/BattleGameMode.cpp:77`, and `Private/Combat/Encounter/EncounterComponent.cpp:107`. `UDefenseSystem::GetActiveDefenderForLocalPlayer` picks the first non-AI open window (`Private/Combat/Defense/DefenseSystem.cpp:250-253`). Travel is `OpenLevelBySoftObjectPtr` (`Private/Trial/TrialRunSubsystem.cpp:46,70,104,133`). Random numbers come from a global `FMath` RNG in about 28 places; B-010 adds a seeded stream.
- Currency (including the account-level Prisms / Diamond) sits on the pawn (`Private/Character/CombatCharacter.cpp:23`). `GASMigration.md:182-189` decided that PlayerState holds player-level data (Prisms / Diamond, which party you lead) and that the ASC lives on the character with `Mixed` / `Minimal` replication (`GASMigration.md:36-44`).
- D-003 (`docs/DesignDecisions.md:81-93`): co-op players join a fight through the encounter sphere; party members are spawned from the list.

**What the standard does:**
- Epic: the server is authoritative. GameMode exists only on the server; GameState and PlayerState replicate to everyone. Clients ask through `Server` RPCs on objects they own (PlayerController, PlayerState, possessed pawn), and the server replicates the results through properties and OnReps (https://dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine, https://dev.epicgames.com/documentation/en-us/unreal-engine/remote-procedure-calls-in-unreal-engine). Subsystems are not replicated. Multiplayer travel uses `ServerTravel` with seamless travel, and `PlayerState::CopyProperties` carries player data across it.
- Lyra (from memory): `ALyraGameState` hosts replicated `UGameStateComponent`s for match state (for example `ULyraExperienceManagerComponent`). `ALyraPlayerState` owns the ASC in `Mixed` mode, plus team and stat tags. Input → ability activation is client-predicted through GAS. Cross-system events use `UGameplayMessageSubsystem` (local only, not replicated).
- Turn-based pattern (ArchitectureReview §1.6): a replicated GameState holds the turn order and the active combatant; clients send intent RPCs and the server validates them.

**Gap:** what breaks first if co-op were switched on today, in the order a player would hit it:
1. **Entering a battle drops the client.** A non-seamless `OpenLevel` on the host (`TrialRunSubsystem.cpp:104`) disconnects clients instead of taking them along.
2. **Even in the same map, the client has no fight.** `TurnManager`, `ActionExecutor` and the menu run only in the host's GI subsystems. The client's copies are never initialised, so its command menu never opens and its HUD reads empty local subsystems.
3. **Client input goes nowhere.** `ACombatPlayerController::OnBlock` and the others call the *client's* `UDefenseSystem` (`CombatPlayerController.cpp:97-104`), which has no open windows, and there is no Server RPC to forward the press. Menu picks have the same problem.
4. **HP doesn't replicate** (the missing `SetIsReplicatedByDefault`), and on clients the `Server*` mutators silently do nothing because `HasServerAuthority` is false. That is safe, but it hides the divergence.
5. **"Player 0" code credits the host.** Pool rewards and economy grants (`PoolSubsystem.cpp:449`, `EconomyService.cpp:1603`) and debug pool apply (`CombatOrchestrator.cpp:873`) go to the host for everyone.
6. Smaller items that come after: currency on the pawn rather than PlayerState; global RNG (B-010); defense timing on `FPlatformTime` (ArchitectureReview §1.4).

Items 1–3 are already the "Before co-op" list (ArchitectureReview §1.6 / §1.9, Backlog "Later"). The new findings are items 4–5, plus the fact that nothing currently stops more per-fight state from landing in GI subsystems. CodebaseReview §5 (line 235) asked for a state-ownership note, and no backlog row tracks it.

**Recommendation:**
- Don't build replication now (co-op is "Later"). Do stop the debt growing, and keep the cheap truths honest. B-131 (the one replication fix) waits on **D-065**.
- Write a one-page state-ownership map. For each per-fight subsystem, give its future home (GameState component / replicated combat actor / PlayerState / stays GI service) and the intent RPC that will feed it. Use the map when B-030 and B-071 touch those systems.
- Make `UCharacterDataComponent` actually replicate (constructor flag; decide whether MaxHP / MaxEP replicate or derive), alongside B-027.
- Route reward and grant code to the instigating controller or leader instead of `GetFirstPlayerController`. That is cheap now and painful later.
- Leave GAS replication as `GASMigration.md` decided (ASC on character, `Mixed` / `Minimal`). It is a deliberate, documented divergence from Lyra's PlayerState-owned ASC, and it is correct for one player leading 3 characters.

**Raised:**
- Jobs: **B-130** (co-op state-ownership note), **B-131** (`UCharacterDataComponent` actually replicates), **B-132** (no more "player 0" lookups). **B-027** now also covers `EconomyService`'s raw `HasAuthority`. Moving the wallet to a PlayerState waits on **D-046** (one wallet per player) and B-090.
- Decisions: **D-064** (stop adding per-fight state to GameInstance subsystems), **D-065** (when co-op gets scheduled).

### Test coverage gaps

**What we do:**
- 6 Automation Spec files, 35 `It` cases, all under `WoR.` (`Scripts/ue/test.sh` runs `Automation RunTests WoR.` headless with `-nullrhi`, then reads `index.json`). Flags on every spec: `EAutomationTestFlags_ApplicationContextMask | ProductFilter` (e.g. `Private/Tests/DamageCalculatorSpec.cpp:28-29`).
  - `WoR.Damage` (11): Pattern-P stat cap, defense clamp, `MIN_DAMAGE`, crit damage cap/gating (`DamageCalculatorSpec.cpp:75-189`).
  - `WoR.Scaling` (8): tier-gap damage/cost, requirement gap (`TierGapScalingSpec.cpp:25-76`).
  - `WoR.TurnManager` (3): tie order, alternation, speed (`TurnManagerSpec.cpp:62-83`).
  - `WoR.Combat.HangFixes` (4): stun turn, win cleanup, invalid AI/player action (`CombatHangFixesSpec.cpp:100-186`).
  - `WoR.Combat.StunRule` (5): stun-allowed actions (`StunRuleSpec.cpp:43-71`).
  - `WoR.Defense.Clock` (4): defense clock only, not input matching (`DefenseClockSpec.cpp:62-102`).
- Shared harness `Private/Tests/WoRTestWorld.h:18-88`: wraps `FTestWorldWrapper`, real `UGameInstance` (every subsystem exists), hand-ticked, bare combatants (no loadout, grid or effects).
- Legacy manual `[PASS]/[FAIL]` test actors still exist: `CombatOrchestratorTestActor.cpp` (1242 lines), `SkillEffectManagerTestActor.cpp` (1000), `TurnManagerTestActor.cpp` (458), plus 44 `Content/` test assets (`git ls-files Content | grep -i test`). CodebaseReview §Testing already flagged them.
- No functional tests (`AFunctionalTest`), no CQTest, no Gauntlet, no `FunctionalTestingEditor` plugin in `world_of_refraction.uproject`.
- Doc drift: `docs/Architecture/AutomationTests.md` table lists 4 spec files / "26 tests". `StunRuleSpec` and `DefenseClockSpec` are missing from it.

**What the standard does:**
- Epic's Automation Test Framework uses specs/simple tests for unit logic, functional tests (`AFunctionalTest` placed in a test map) for level and flow tests, and Gauntlet for packaged-build sessions on CI/devices. https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine , https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-spec-in-unreal-engine
- CQTest (`TEST_CLASS`, fixtures with before/after, latent commands; add the `CQTest` module) is Epic's newer, fixture-friendly layer over `FAutomationTestBase`. https://dev.epicgames.com/documentation/en-us/unreal-engine/cqtest-test-framework-for-unreal-engine
- Test flags: `SmokeFilter` for very fast tests that run on every editor start/commit, `ProductFilter` for project tests, `EngineFilter` for engine tests.
- Lyra (from memory): ships few gameplay unit tests. It relies on functional-test maps and Gauntlet-driven bot/automation runs (`LyraGame` `Build/` scripts, `ULyraBotCheats`-style bots) rather than wide unit coverage. Lyra is not a coverage benchmark. Epic's guidance for game logic is "unit-test the pure rules, functional-test the flow".
- ArchitectureReview §1.7 (`docs/analysis/ArchitectureReview_2026-10.md:118-125`) already recommends Spec/CQTest for pure logic and functional tests for level flows. B-007 delivered the first part. Not repeated here.

**Gap:** The harness is good, and the maths core is tested. But the riskiest stateful systems have no test, and several jobs (B-042, B-062, B-063) promise "tests identical before/after" while no baseline exists yet. Untested systems with real logic, ranked by risk (bug likelihood × player impact × upcoming refactor churn):
1. **SkillEffectManager**, 2563 lines (apply/stack/merge, tick-down, triggers, clear at combat end). Known array-mutation bug (CodebaseReview #4). GAS stage 3 (B-042) rewrites it. No characterisation test exists. **High.**
2. **StatusBuildupManager** (`AddStatusBuildup`, decay, trigger at cap, resistances). Suspected double multiplier (B-113). **High.**
3. **Infusion cost / commit costs** (`ActionExecutor::ApplyCommitCosts`, deferred HP cost `ActionExecutor.cpp:6747`, `InfusionChargeManager::CalculateLevelFromTime`). Players pay HP/EP here. **High.**
4. **Loadout validation**: three validators disagree. **High.** Covered by B-028.
5. **Defense matching** (`DefenseSystem::MatchAndConsumeInput`, `CalculateDefenseResult`, parry reflect). Player-facing timing; only the clock is tested. **Medium-high.**
6. **AI decisions** (`AIDecisionManager.cpp`, 2399 lines: target score, survival/cleanse branches). Estimator parity is covered by B-029 and B-114. Branch choice is untested. **Medium.** Easy-difficulty randomness needs B-010.
7. **Economy** (`EconomyService`: `BuildCartCost`, level-up/downgrade refunds, dismantle). Currency loss is bad, but the code is simple and runs out of combat. **Medium.** Rewards are covered later by B-073.
8. **Grid** (`CombatGridSubsystem`: row damage/defense modifiers, push/pull/swap). Pure, cheap to test. **Low-medium.**
9. **Broken Darkness** (strain, break roll; the roll needs B-010), **Weather blend** (`ComputeBlendValue`), **Crystal wear** (carry-over test in B-115). **Low-medium.**

**Recommendation:** Stay on Automation Spec plus `FWoRTestWorld`. It works, it's headless, and the Stop hook already runs it. Don't adopt CQTest, functional tests or Gauntlet now; that's over-engineering for one developer before a playable demo. Revisit functional tests once a fixed "test arena" map is worth keeping (after B-105). Add a few **characterisation specs** that pin today's behaviour before the refactors that promise "identical before/after" (B-020, B-021, B-028, B-032, B-041, B-042). Extend `FWoRTestWorld` with one helper that gives a combatant a loadout from `DA_Test_Inventory`; without it, no gear path can be tested (AutomationTests.md "Known Limitations").
Existing jobs that already add tests (don't duplicate): B-010 (seeded RNG, which unblocks crit/AI/BD rolls), B-020, B-021, B-026, B-028, B-029, B-030, B-032, B-042, B-062, B-063, B-064, B-064a, B-065, B-067, B-073, B-080..B-084, B-091, B-095, B-096, B-112, B-113, B-115.

**Raised:**
- Jobs: **B-133** (test harness helper and `AutomationTests.md`), **B-134** (`WoR.Effects` baseline spec, needed before B-042/B-062), **B-135** (buildup, defense-matching and grid specs), **B-136** (infusion cost spec), **B-137** (economy spec).
- Decisions: **D-066** (when to add functional/level tests), **D-067** (retire the legacy test actors).

### Performance

**What we do:**
- **Tick:** almost everything has tick off (`bCanEverTick = false` in ~20 constructors, e.g. `Combat/CombatOrchestrator.cpp:43`, `Loadout/LoadoutComponent.cpp:32`). CodebaseReview found "no gameplay Tick". Three exceptions it missed or allowed:
  - `Equipment/Weapons/WeaponMeshComponent.cpp:14-31` ticks at 10 Hz on every combat character to **poll** `GetActiveWeapon()` for a change. Each poll costs a `FindComponentByClass<ULoadoutComponent>` (`Character/CharacterDataComponent.cpp:564-571`). `ULoadoutComponent::OnLoadoutChanged` exists (`Public/Loadout/LoadoutComponent.h:46`, broadcast `LoadoutComponent.cpp:107`), but no weapon-toggle event does.
  - `Combat/CombatAnimInstance.cpp:34-68`, per frame per character: a subsystem lookup, `FindComponentByClass<ULoadoutComponent>` (`:119`), stance resolution, and every 60 frames an `FString::Printf` whose log line is commented out. It counts frames with a function-local `static int32 LogCounter` shared by every instance (`:46`). That's mutable static state, which CodebaseReview says doesn't exist.
  - `Combat/Projectile/SkillProjectile.cpp:47,97-113` ticks for flight. That's fine and short-lived.
  - `Infusion/InfusionVFXComponent.cpp:81-84` has an empty `TickComponent` override with tick off. Dead code.
- **Timers:** 17 `SetTimer` calls, mostly one-shot (AI think delay, defense window, action timeout). Looping: the burst projectile (`ActionExecutor.cpp:3818`) and a **60 Hz** charge timer while the player holds infuse (`Infusion/InfusionChargeManager.cpp:71-76`, body `:158-185`). It's bounded to the hold and only broadcasts on a level change. The Shop toast uses `FTSTicker` because world timers pause under the shop (`UI/Shop/ShopWindowWidget.h:19`). That's correct.
- **World queries:** no `GetAllActorsOfClass`. `TActorIterator` only in debug commands (`CombatOrchestrator.cpp:907`). `GetAllActorsWithTag` at `CombatOrchestrator.cpp:2632,2643`; low frequency.
- **`FindComponentByClass`:** 366 call sites, mostly uncached per call (EconomyService 49, CombatOrchestrator 39, AIDecisionManager 26). Cheap with ~10 components per actor and turn-based call rates. It breaks CLAUDE.md's "find once, cache" rule, but that's a style issue, not a perf issue.
- **Logging:** 1816 `UE_LOG`, nearly all still on `LogTemp` (619 Warning, 571 Display, 456 Log). Categories exist (`Public/WoRLog.h:11-29`, B-006) but are only used by new code. Per-action/per-hit lines are logged at Warning/Display: `"[ActionExecutor] OnDefenseWindowClosed CALLBACK FIRED"` at Warning per defended hit (`ActionExecutor.cpp:1744`), `BindDefenseSystemEvents` at Warning (`:2635-2644`), `[TierGap] ... (applied)` at Display per action (`:584-591`). CodebaseReview §hygiene noted the spam in general; these are the per-hit offenders.
- **Asset loading:** skill data hard-references montages and Niagara (`SkillDataBase.h:295-310`, `SpellData.h:63-81`, `WeaponData.h:75-84`). The newer per-entry VFX are soft (`SkillVFXEntry.h:60`, `SkillCastEntry.h:59`) but are `LoadSynchronous()`'d **at cast time** (`ActionExecutor.cpp:3452-3454, 3511, 3620, 5320, 5827`). The first cast of each skill can hitch on a cold load. `LoadObject` fallbacks in `CombatOrchestrator.cpp:1970-2569` are debug-only functions.
- ACharacter-based combatants keep the default actor + `CharacterMovementComponent` tick (`Character/CombatCharacter.cpp:17-38` doesn't disable it). That's negligible at party sizes.

**What the standard does:**
- Epic's tick guidance: disable tick by default, prefer events and timers, use `TickInterval` only when polling is unavoidable, and never poll for state that can broadcast. https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine
- Anim: move per-frame work to `NativeThreadSafeUpdateAnimation`, using property access / cached values set by events, so the game thread stays free. Lyra's `ULyraAnimInstance` (from memory) does this: it initialises in `NativeInitializeAnimation` and keeps game-thread lookups out of the update. https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-optimization-in-unreal-engine
- Asset loading: soft references plus async preload before use (`FStreamableManager` / `UAssetManager` bundles). Lyra (from memory) preloads through `ULyraExperienceDefinition` / `ULyraAssetManager` before gameplay starts. https://dev.epicgames.com/documentation/en-us/unreal-engine/asynchronous-asset-loading-in-unreal-engine
- Logging: hot-path detail at `Verbose`/`VeryVerbose` under a named category; Warning is for "something is wrong". https://dev.epicgames.com/documentation/en-us/unreal-engine/logging-in-unreal-engine
- Measure before optimising: `stat unit`, `stat game`, `stat anim`, `stat startfile`, and Unreal Insights (`-trace=cpu,frame,loadtime`). https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine . The Significance Manager is for many-actor open worlds; it isn't relevant at party scale.

**Gap:** The overall shape is right: event-driven, no gameplay tick. Three real issues: (1) weapon-mesh polling instead of an event; (2) per-frame game-thread lookups and dead diagnostic code in the anim instance; (3) synchronous loads at cast time. Log noise at Warning in per-hit paths makes real warnings hard to see and costs string formatting every hit. Nobody has measured anything yet: no Insights capture or `stat` baseline is on record.

**Recommendation:** Small, targeted fixes. No Significance Manager, no tick-manager rework.
1. Add an `OnActiveWeaponChanged` delegate on `ULoadoutComponent` (fired on toggle/equip/loadout switch). `UWeaponMeshComponent` binds to it and turns its tick off.
2. Anim instance: delete the dead counter/Printf block; cache `ULoadoutComponent` with `CharacterDataComponent`. A thread-safe update can wait; it isn't worth it until profiling says so.
3. Preload the soft VFX of every combatant's equipped skills at `InitializeCombat` with one `FStreamableManager::RequestAsyncLoad`. Hold the handle until `ReleaseCombatResources` (pairs with B-030).
4. Logging: B-006 already migrates `LogTemp` file by file. Add one rule to `docs/Architecture/Logging.md`: per-hit/per-action lines go to `Verbose`. Downgrade the three named offenders when ActionExecutor is next touched.
5. Capture one Insights trace of a full fight to `docs/` as a baseline (Crown, editor). Only optimise further from evidence, so the async preload (B-140) waits on it.

**Raised:**
- Jobs: **B-138** (weapon mesh driven by a delegate, no tick), **B-139** (anim-instance cleanup), **B-140** (async VFX preload at combat start, after the B-142 trace), **B-141** (per-hit logs to Verbose), **B-142** (an Unreal Insights baseline, Crown).
- Decision: **D-068** (a demo frame budget).

