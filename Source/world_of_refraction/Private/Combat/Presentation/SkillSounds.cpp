// SkillSounds.cpp
// B-618: see SkillSounds.h.

#include "Combat/Presentation/SkillSounds.h"
#include "Combat/Presentation/ImpactVisuals.h"
#include "Components/SceneComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Sound/SoundBase.h"
#include "WoRLog.h"

namespace
{
	TAutoConsoleVariable<bool> CVarSkillSFXLog(
		TEXT("wor.SkillSFX.Log"),
		false,
		TEXT("DEBUG (B-618): log every skill sound played (label, asset, where)."));

	USoundBase *LoadSound(const FSkillSFXEntry &Entry)
	{
		// Small assets, loaded on first use (as the impact visuals). TODO: preload with the fight if it hitches.
		USoundBase *Sound = Entry.Sound.LoadSynchronous();
		if (!Sound && !Entry.Sound.IsNull())
		{
			UE_LOG(LogWoR, Warning, TEXT("[SkillSFX] '%s': %s did not load"), *Entry.Label, *Entry.Sound.ToString());
		}
		return Sound;
	}

	void LogPlayed(const FSkillSFXEntry &Entry, const FString &Where)
	{
		if (CVarSkillSFXLog.GetValueOnGameThread())
		{
			UE_LOG(LogWoR, Display, TEXT("[SkillSFX] '%s' %s at %s"), *Entry.Label, *Entry.Sound.GetAssetName(), *Where);
		}
	}
}

namespace SkillSounds
{
	TArray<const FSkillSFXEntry *> ChooseImpactSounds(EImpactOutcome Outcome, const USkillDataBase *Skill)
	{
		TArray<const FSkillSFXEntry *> Sounds;
		if (!Skill || (Outcome != EImpactOutcome::Hit && Outcome != EImpactOutcome::Crit))
		{
			return Sounds;
		}
		for (const FSkillSFXEntry &Entry : Skill->SFXArray)
		{
			if (Entry.Role == ESFXRole::Impact && !Entry.Sound.IsNull())
			{
				Sounds.Add(&Entry);
			}
		}
		return Sounds;
	}

	bool PlayAtLocation(const UObject *WorldContext, const FSkillSFXEntry &Entry, const FVector &Location)
	{
		USoundBase *Sound = LoadSound(Entry);
		if (!Sound)
		{
			return false;
		}
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Entry.VolumeMultiplier, Entry.PitchMultiplier);
		LogPlayed(Entry, Location.ToCompactString());
		return true;
	}

	bool PlayAttached(const FSkillSFXEntry &Entry, USceneComponent *Parent, FName Socket)
	{
		USoundBase *Sound = LoadSound(Entry);
		if (!Sound || !Parent)
		{
			return false;
		}
		UGameplayStatics::SpawnSoundAttached(Sound, Parent, Socket, FVector::ZeroVector, EAttachLocation::SnapToTarget, true,
											 Entry.VolumeMultiplier, Entry.PitchMultiplier);
		LogPlayed(Entry, FString::Printf(TEXT("%s:%s"), *GetNameSafe(Parent->GetOwner()), *Socket.ToString()));
		return true;
	}

	FString GetSoundsString(const USkillDataBase *Skill)
	{
		if (!Skill || Skill->SFXArray.IsEmpty())
		{
			return TEXT("SFX: none");
		}
		FString Output = TEXT("SFX:");
		for (int32 i = 0; i < Skill->SFXArray.Num(); ++i)
		{
			const FSkillSFXEntry &Entry = Skill->SFXArray[i];
			Output += FString::Printf(TEXT("%s %d %s (%s) %s"), i == 0 ? TEXT("") : TEXT(" |"), i, *Entry.Label,
									  *UEnum::GetDisplayValueAsText(Entry.Role).ToString(),
									  Entry.Sound.IsNull() ? TEXT("(no sound)") : *Entry.Sound.GetAssetName());
		}
		return Output;
	}
}
