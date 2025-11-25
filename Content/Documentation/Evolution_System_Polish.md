# Session: Evolution System Polish

## Date: [Today's Date]

## Changes Made

### PassiveEffect.h
- Removed manual Description field
- Added `GeneratedDescription` (VisibleAnywhere, read-only)
- Added `RefreshDescription()` to update cached description
- Added `GetDescription()` returns cached or builds fresh
- Added `BuildDescription()` combines trigger + effect + duration
- Added `GetEffectDescription()` switch for all 30+ effect types
- Updated `GetTriggerName()` to include threshold values

### EvolutionData.h
- Added `TitleProperty = "PassiveName"` to PassiveEffects array
- Added `PostEditChangeProperty` declaration

### EvolutionData.cpp
- Added `PostEditChangeProperty` implementation (refreshes passive descriptions)
- Added element validation for ExclusiveSpells (must match evolution element or Generic)
- Added element validation for EvolutionUltimate (must match evolution element or Generic)
- Removed null spell slot validation (partial fills now allowed)

### EvolutionDataDebug.cpp
- Cleaned up passive output (removed redundant trigger line)

## Auto-Description Examples

| Setup                                   | Output                                           |
| --------------------------------------- | ------------------------------------------------ |
| Always + ModifyDamageDealt +10          | "Always Active: +10% Damage Dealt"               |
| OnTurnStart + RestoreEnergy 8           | "On Turn Start: Restore 8 Energy"                |
| OnCrit + ModifyDamageDealt +30, 2 turns | "On Critical Hit: +30% Damage Dealt for 2 turns" |
| OnHPBelowThreshold 30% + Revive 25      | "When HP Below 30%: Revive at 25% HP"            |
| OnKill + ApplyBurnToTarget 30, 3 turns  | "On Kill: Apply 30 Burn for 3 turns"             |

## Validation Rules

### Exclusive Spells
- Element must match evolution OR be Generic
- Null slots allowed (no longer required to fill all 6)
- Max 6 spells enforced

### Evolution Ultimate
- If `bOverridesUltimate` is true, ultimate must be assigned
- Ultimate element must match evolution OR be Generic

## Files Modified
- `Source/world_of_refraction/Public/PassiveEffect.h`
- `Source/world_of_refraction/Public/EvolutionData.h`
- `Source/world_of_refraction/Private/EvolutionData.cpp`
- `Source/world_of_refraction/Private/EvolutionDataDebug.cpp`

## Next Steps
- Design discussion before next implementation phase