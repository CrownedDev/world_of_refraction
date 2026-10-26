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

    // NOTE(id-overflow): SourceID*100 overflows int32 only when SourceID > ~21.47M.
    //   SourceID is GetUniqueID() = the live UObject-slot index (GC-reused, bounded by
    //   peak live objects), which stays far below that for this game — NOT reachable, not
    //   live debt. Widen EffectID to int64 only if object counts ever approach that.
    constexpr int32 PackEffectID(int32 SourceID, int32 EffectIndex, int32 SubIndex)
    {
        return SourceID * EFFECT_ID_WINDOW + EffectIndex + SubIndex * 10;
    }

    // Full-bar (status cap) effects live in their own negative band, one ID per
    // (resolved effect type, element), so the same cap from the same source finds its
    // earlier copy (D-012). Packed IDs are never negative, so the bands can't meet.
    constexpr int32 STATUS_CAP_ID_BASE = -1000000;
    constexpr int32 STATUS_CAP_ELEMENT_SLOTS = 256;

    constexpr int32 MakeStatusCapEffectID(uint8 EffectType, uint8 Element)
    {
        return STATUS_CAP_ID_BASE - (EffectType * STATUS_CAP_ELEMENT_SLOTS + Element);
    }

    // The "who gave it" half of an effect's identity (D-012, D-053): the same EffectID
    // from the same source refreshes; from another source it is a separate effect.
    // Gear stamps its item (FGatheredEffect::SourceKey); everything else is keyed on the
    // applying actor plus the skill or reason it applied with.
    inline int32 MakeSourceKey(uint32 SourceObjectID, const FString &SourceName)
    {
        return static_cast<int32>(HashCombine(GetTypeHash(SourceObjectID), GetTypeHash(SourceName)));
    }
}

// Compile-time parity guards (sample inputs spanning the band):
static_assert(EffectIdentity::PackEffectID(12, 3, 4) == 1243, "pack parity"); // 12*100 + 3 + 4*10
static_assert(EffectIdentity::PackEffectID(0, 0, 0) == 0, "pack parity zero");
static_assert(EffectIdentity::PackEffectID(1234, 4, 9) == 123494, "pack parity max-band");
static_assert(EffectIdentity::MakeStatusCapEffectID(255, 255) < 0, "status cap band stays negative");
