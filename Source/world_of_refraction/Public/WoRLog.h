// WoRLog.h
// Project log categories. Use these in new code instead of LogTemp; migrate an
// existing file's LogTemp calls when you touch it. Filter in the Output Log or
// with `Log <Category> <Verbosity>` (e.g. `Log LogWoRCombat Verbose`).

#pragma once

#include "CoreMinimal.h"

/** General / anything without a narrower home (test helpers, module-level). */
WORLD_OF_REFRACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogWoR, Log, All);

/** Turn flow, orchestrator, actions, damage. */
WORLD_OF_REFRACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogWoRCombat, Log, All);

/** Skill effects, status buildup, Broken Darkness. */
WORLD_OF_REFRACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogWoREffects, Log, All);

/** AI decisions and scoring. */
WORLD_OF_REFRACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogWoRAI, Log, All);

/** Loadouts, inventory, equipment, crystals. */
WORLD_OF_REFRACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogWoRLoadout, Log, All);

/** Widgets, HUD, menus. */
WORLD_OF_REFRACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogWoRUI, Log, All);

/** Currency, shop, merchant, dismantle/upgrade. */
WORLD_OF_REFRACTION_API DECLARE_LOG_CATEGORY_EXTERN(LogWoREconomy, Log, All);
