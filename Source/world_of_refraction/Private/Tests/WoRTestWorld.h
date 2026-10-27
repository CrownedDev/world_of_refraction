// WoRTestWorld.h
// Headless game world for automation specs: a real UGameInstance (so every WoR
// GameInstanceSubsystem exists) and a manually ticked world with BeginPlay run.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "GAS/WoRTurnEffectTracker.h"
#include "WoRLog.h"

class UWeaponData;

class FWoRTestWorld
{
public:
	/** Creates the world and begins play. False (with the wrapper's errors logged) on failure. */
	bool Create()
	{
		if (Wrapper.CreateTestWorld(EWorldType::Game) && Wrapper.BeginPlayInTestWorld())
		{
			return true;
		}
		TArray<FString> Errors;
		Wrapper.AppendErrorMessages(Errors);
		for (const FString &Error : Errors)
		{
			UE_LOG(LogWoR, Error, TEXT("[WoRTestWorld] %s"), *Error);
		}
		return false;
	}

	UWorld *GetWorld() const { return Wrapper.GetTestWorld(); }

	template <class TSubsystem>
	TSubsystem *GetSubsystem() const
	{
		UWorld *World = GetWorld();
		UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<TSubsystem>() : nullptr;
	}

	/** Runs world ticks — timers (including SetTimerForNextTick) fire here. */
	void Tick(int32 Frames = 1)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Wrapper.TickTestWorld();
		}
	}

	/** Transient character asset with every stat at its default (0 points, world level 0). */
	static UCharacterData *NewCharacterData()
	{
		return NewObject<UCharacterData>(GetTransientPackage());
	}

	/** Spawns a bare combatant carrying only a UCharacterDataComponent seeded from Data.
	 *  bAIControlled spawns an unpossessed pawn (the orchestrator routes it to AI);
	 *  otherwise a plain actor (routed to the player path). */
	AActor *SpawnCombatant(UCharacterData *Data, bool bAIControlled = false)
	{
		UWorld *World = GetWorld();
		if (!World || !Data)
		{
			return nullptr;
		}

		AActor *Actor = bAIControlled ? World->SpawnActor<APawn>() : World->SpawnActor<AActor>();
		if (!Actor)
		{
			return nullptr;
		}

		UCharacterDataComponent *CharComp = NewObject<UCharacterDataComponent>(Actor);
		CharComp->CharacterData = Data;
		Actor->AddInstanceComponent(CharComp);
		// The world has begun play, so registering runs the component's BeginPlay (HP/EP seed).
		CharComp->RegisterComponent();
		return Actor;
	}

	/** Adds and registers a component on an actor that has already begun play. */
	template <class TComponent>
	static TComponent *AddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	/** Gives Actor an ability system component (unless it has one), a turn tracker and an effect mirror
	 *  (B-042c, B-042h), so its stored effects are mirrored onto GAS and `wor.Effects.UseGAS` can use them. */
	static UWoREffectMirrorComponent *AddEffectMirror(AActor *Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}
		if (!UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
		{
			AddComponent<UAbilitySystemComponent>(Actor)->InitAbilityActorInfo(Actor, Actor);
		}
		// As on ACombatCharacter: with UseGAS 1 the tracker owns the effects' lifetime (B-042h).
		if (!Actor->FindComponentByClass<UWoRTurnEffectTracker>())
		{
			AddComponent<UWoRTurnEffectTracker>(Actor);
		}
		return AddComponent<UWoREffectMirrorComponent>(Actor);
	}

	/** SpawnCombatant plus an inventory and a loadout initialised from Data. With PrimaryWeapon,
	 *  the active saved loadout's primary slot holds it. The actor is the loadout's owner. */
	ULoadoutComponent *SpawnCombatantWithLoadout(UCharacterData *Data, UWeaponData *PrimaryWeapon = nullptr,
												 bool bAIControlled = false)
	{
		AActor *Actor = SpawnCombatant(Data, bAIControlled);
		if (!Actor)
		{
			return nullptr;
		}
		UInventoryComponent *Inventory = AddComponent<UInventoryComponent>(Actor);
		ULoadoutComponent *Loadout = AddComponent<ULoadoutComponent>(Actor);
		Loadout->InitializeFromCharacterData(Data, Inventory);
		if (PrimaryWeapon && !Inventory->SavedLoadouts.IsValidIndex(Inventory->ActiveLoadoutIndex))
		{
			UE_LOG(LogWoR, Warning, TEXT("[WoRTestWorld] No active saved loadout to hold the primary weapon"));
		}
		else if (PrimaryWeapon)
		{
			FCombatLoadout &Active = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
			Active.PrimarySlotType = EPrimarySlotType::Weapon;
			Active.PrimaryWeapon.WeaponEntry.Weapon = PrimaryWeapon;
			Loadout->NotifyActiveWeaponChanged(); // a direct edit, so announce it (B-138)
		}
		return Loadout;
	}

private:
	FTestWorldWrapper Wrapper;
};

#endif // WITH_DEV_AUTOMATION_TESTS
