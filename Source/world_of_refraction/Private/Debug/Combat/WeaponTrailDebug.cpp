// WeaponTrailDebug.cpp
// Crown 2026-11-24: play a weapon trail on demand, and print what it would use:
// wor.Debug.WeaponTrail [Element=None] [ActorNameContains]

#include "Combat/Presentation/WeaponTrail.h"
#include "EngineUtils.h"
#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "WoRLog.h"

namespace WeaponTrailDebug
{
	constexpr float PLAY_SECONDS = 1.0f;

	/** The first actor showing a weapon whose name contains Filter (any, with an empty one). */
	AActor *FindArmedActor(UWorld &World, const FString &Filter)
	{
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			const UWeaponMeshComponent *Weapons = It->FindComponentByClass<UWeaponMeshComponent>();
			if (Weapons && Weapons->GetWeaponVFXMesh() && (Filter.IsEmpty() || It->GetName().Contains(Filter)))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs GPlayWeaponTrailCmd(
		TEXT("wor.Debug.WeaponTrail"),
		TEXT("DEBUG: play a weapon trail for 1s on the first fighter showing a weapon. Usage: wor.Debug.WeaponTrail [Element=None] [ActorNameContains]. "
			 "Logs the weapon, its trail kind, the slot's system and the ribbon's ends."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString> &Args, UWorld *World)
			{
				if (!World)
				{
					return;
				}
				ESpellElement Element = ESpellElement::None;
				if (Args.Num() > 0)
				{
					const int64 Value = StaticEnum<ESpellElement>()->GetValueByNameString(Args[0]);
					if (Value == INDEX_NONE)
					{
						UE_LOG(LogWoR, Warning, TEXT("[WeaponTrail] no element %s"), *Args[0]);
						return;
					}
					Element = static_cast<ESpellElement>(Value);
				}
				AActor *Actor = FindArmedActor(*World, Args.Num() > 1 ? Args[1] : FString());
				if (!Actor)
				{
					UE_LOG(LogWoR, Warning, TEXT("[WeaponTrail] no actor showing a weapon"));
					return;
				}
				UE_LOG(LogWoR, Display, TEXT("[WeaponTrail] %s"), *WeaponTrail::GetTrailString(Actor));
				if (!WeaponTrail::Start(Actor, Element))
				{
					UE_LOG(LogWoR, Display, TEXT("[WeaponTrail] nothing to play (no trail kind or an empty slot)"));
					return;
				}
				const TWeakObjectPtr<AActor> WeakActor = Actor;
				FTimerHandle StopHandle;
				World->GetTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateLambda([WeakActor]()
				{
					if (AActor *Still = WeakActor.Get())
					{
						WeaponTrail::Stop(Still);
					}
				}), PLAY_SECONDS, false);
			}));
}
