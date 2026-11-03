// WoRInteract.h
// B-256 (DRY audit A16): the shared part of the walk-up interactables (trial doors, merchants, trial map
// features): the nearest one of a class overlapping the player's pawn, and (B-256b) the on-screen interact hint. Range is each actor's trigger volume, so
// there's no distance parameter. The party interaction keeps its own (it filters by component and view cone).

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"

namespace WoRInteract
{
	/** A hint stays up until it is hidden. */
	constexpr float HINT_DURATION = 1e9f;

	/** The hint's message key: the owner's id, so the hint updates in place and is removable. int32 cast:
	 *  GetUniqueID() is uint32, ambiguous between the int32 / uint64 AddOnScreenDebugMessage overloads. */
	inline int32 HintKey(const UObject &Owner)
	{
		return static_cast<int32>(Owner.GetUniqueID());
	}

	/** Shows (or replaces) Owner's hint; an empty Hint hides it. */
	inline void ShowHint(const UObject &Owner, const FColor &Color, const FString &Hint)
	{
		if (!GEngine)
		{
			return;
		}
		if (Hint.IsEmpty())
		{
			GEngine->RemoveOnScreenDebugMessage(HintKey(Owner));
			return;
		}
		GEngine->AddOnScreenDebugMessage(HintKey(Owner), HINT_DURATION, Color, Hint);
	}

	inline void HideHint(const UObject &Owner)
	{
		if (GEngine)
		{
			GEngine->RemoveOnScreenDebugMessage(HintKey(Owner));
		}
	}

	/** The TActor overlapping Pawn closest to it, or null. */
	template <class TActor>
	TActor *FindNearestOverlapping(APawn *Pawn)
	{
		if (!Pawn)
		{
			return nullptr;
		}
		TArray<AActor *> Overlapping;
		Pawn->GetOverlappingActors(Overlapping, TActor::StaticClass());

		TActor *Nearest = nullptr;
		double NearestDistSquared = TNumericLimits<double>::Max();
		const FVector PawnLocation = Pawn->GetActorLocation();
		for (AActor *Actor : Overlapping)
		{
			const double DistSquared = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
			if (DistSquared < NearestDistSquared)
			{
				NearestDistSquared = DistSquared;
				Nearest = Cast<TActor>(Actor);
			}
		}
		return Nearest;
	}
}
