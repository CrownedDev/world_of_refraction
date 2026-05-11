// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * ActionUtils — pure inline helpers shared by ExecuteSpellAsync /
 * ExecuteAbilityAsync / ExecuteAttackAsync. Stateless; no UObject lifecycle.
 */
namespace ActionUtils
{
	/**
	 * If raw mode is active, fold the action's StatusBuildup value into BaseDamage.
	 *
	 * Locked design: a raw-mode action deals its buildup amount as additional damage
	 * and the status bar doesn't move. The fold happens at the orchestrator boundary
	 * so the downstream defense pipeline and ApplyHit see only normalised inputs —
	 * neither needs to know about raw mode.
	 *
	 * (Session Y note: the third parameter — InOutStatusToBuild — was removed once
	 * StatusBuildupManager took over trigger resolution from Element + PhysicalType.
	 * No StatusType is plumbed through the pipeline anymore, so there is nothing
	 * for the redirect to zero on the trigger side.)
	 *
	 * @param bIsRawMode               Read from the action's data asset
	 * @param InOutBaseDamage          Receives the buildup value when raw
	 * @param InOutBaseStatusBuildup   Zeroed when raw
	 */
	inline void ApplyRawModeRedirect(
		bool bIsRawMode,
		int32 &InOutBaseDamage,
		int32 &InOutBaseStatusBuildup)
	{
		if (bIsRawMode)
		{
			InOutBaseDamage += InOutBaseStatusBuildup;
			InOutBaseStatusBuildup = 0;
		}
	}
}
