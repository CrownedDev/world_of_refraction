// WoRInteract.h
// B-256 (DRY audit A16): the shared part of the walk-up interactables (trial doors, merchants, trial map
// features): the nearest one of a class overlapping the player's pawn. Range is each actor's trigger volume, so
// there's no distance parameter. The party interaction keeps its own (it filters by component and view cone).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

namespace WoRInteract
{
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
		float NearestDistSquared = TNumericLimits<float>::Max();
		const FVector PawnLocation = Pawn->GetActorLocation();
		for (AActor *Actor : Overlapping)
		{
			const float DistSquared = FVector::DistSquared(PawnLocation, Actor->GetActorLocation());
			if (DistSquared < NearestDistSquared)
			{
				NearestDistSquared = DistSquared;
				Nearest = Cast<TActor>(Actor);
			}
		}
		return Nearest;
	}
}
