// EffectIdentity.h
// Single owner of the Space-A effect-ID packing scheme: a per-source 100-slot window
// with EffectIndex in the ones place and SubIndex/PayloadIndex in the tens place.
// Used by equipment/evolution (CreateAllFromSkillEffect), the cast apply loop
// (ActionExecutor), and the equipment-window debug reader (SkillEffectManager) so the
// packer and the window reader can never drift.
//
// NOT used by Space B (CreateFromInfusion/WeaponInfusion/PhysicalDamageType — base*10+offset);
// that is a deliberately separate ID space.

#pragma once

#include "CoreMinimal.h"

namespace EffectIdentity
{
    constexpr int32 EFFECT_ID_WINDOW = 100;
    constexpr int32 EFFECT_ID_SUBBAND_MAX = 9; // EffectIndex and SubIndex each <= 9

    // TODO(id-overflow): SourceID*100 can overflow int32 — single owner of this note.
    //   Shared hazard across all Space-A packers; widen scheme later if it bites.
    constexpr int32 PackEffectID(int32 SourceID, int32 EffectIndex, int32 SubIndex)
    {
        return SourceID * EFFECT_ID_WINDOW + EffectIndex + SubIndex * 10;
    }

    constexpr int32 WindowLoForSource(int32 SourceID)
    {
        return SourceID * EFFECT_ID_WINDOW;
    }

    constexpr int32 WindowHiForSource(int32 SourceID)
    {
        return WindowLoForSource(SourceID) + (EFFECT_ID_WINDOW - 1);
    }
}

// Compile-time parity guards (sample inputs spanning the band):
static_assert(EffectIdentity::PackEffectID(12, 3, 4) == 1243, "pack parity"); // 12*100 + 3 + 4*10
static_assert(EffectIdentity::PackEffectID(0, 0, 0) == 0, "pack parity zero");
static_assert(EffectIdentity::PackEffectID(1234, 4, 9) == 123494, "pack parity max-band");
static_assert(EffectIdentity::WindowLoForSource(7) == 700, "window lo");
static_assert(EffectIdentity::WindowHiForSource(7) == 799, "window hi");
