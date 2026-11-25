// SkillSoundsSpec.cpp
// B-618: a skill's SFXArray. Impact entries play on a landed hit (Hit or Crit, a projectile's arrival too), never on a
// Guard, Perfect Guard or Dodge; Notify entries are left to their SFX notify; an entry with no sound plays nothing.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Presentation/ImpactVisuals.h"
#include "Combat/Presentation/SkillSounds.h"
#include "Skills/Definitions/AbilityData.h"
#include "Sound/SoundBase.h"

namespace
{
	TSoftObjectPtr<USoundBase> TestSound(const TCHAR *Name)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/Test/%s.%s"), Name, Name)));
	}

	FSkillSFXEntry &AddSound(UAbilityData &Skill, const TCHAR *Label, ESFXRole Role, bool bWithSound = true)
	{
		FSkillSFXEntry &Entry = Skill.SFXArray.AddDefaulted_GetRef();
		Entry.Label = Label;
		Entry.Role = Role;
		if (bWithSound)
		{
			Entry.Sound = TestSound(Label);
		}
		return Entry;
	}

	/** Release (Notify), Hit (Impact), Silent (Impact, no sound), Boom (Impact). */
	UAbilityData *SkillWithSounds()
	{
		UAbilityData *Skill = NewObject<UAbilityData>(GetTransientPackage());
		AddSound(*Skill, TEXT("Release"), ESFXRole::Notify);
		AddSound(*Skill, TEXT("Hit"), ESFXRole::Impact);
		AddSound(*Skill, TEXT("Silent"), ESFXRole::Impact, false);
		AddSound(*Skill, TEXT("Boom"), ESFXRole::Impact);
		return Skill;
	}
}

BEGIN_DEFINE_SPEC(FSkillSoundsSpec, "WoR.Presentation.SkillSounds",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FSkillSoundsSpec)

void FSkillSoundsSpec::Define()
{
	Describe("ChooseImpactSounds", [this]()
	{
		It("plays every Impact entry with a sound on a Hit and a Crit, in array order", [this]()
		{
			const UAbilityData *Skill = SkillWithSounds();
			for (const EImpactOutcome Outcome : {EImpactOutcome::Hit, EImpactOutcome::Crit})
			{
				const TArray<const FSkillSFXEntry *> Sounds = SkillSounds::ChooseImpactSounds(Outcome, Skill);
				if (TestEqual(TEXT("two Impact sounds (the Notify and the soundless one left out)"), Sounds.Num(), 2))
				{
					TestEqual(TEXT("first"), Sounds[0]->Label, FString(TEXT("Hit")));
					TestEqual(TEXT("second"), Sounds[1]->Label, FString(TEXT("Boom")));
				}
			}
		});

		It("plays nothing on a Guard, Perfect Guard or Dodge", [this]()
		{
			const UAbilityData *Skill = SkillWithSounds();
			for (const EImpactOutcome Outcome : {EImpactOutcome::Guard, EImpactOutcome::PerfectGuard, EImpactOutcome::Dodge})
			{
				TestEqual(TEXT("no impact sound"), SkillSounds::ChooseImpactSounds(Outcome, Skill).Num(), 0);
			}
		});

		It("plays nothing for a skill with no sounds or no skill", [this]()
		{
			TestEqual(TEXT("no sounds"), SkillSounds::ChooseImpactSounds(EImpactOutcome::Hit, NewObject<UAbilityData>(GetTransientPackage())).Num(), 0);
			TestEqual(TEXT("no skill"), SkillSounds::ChooseImpactSounds(EImpactOutcome::Hit, nullptr).Num(), 0);
		});
	});

	Describe("Play", [this]()
	{
		It("does not play an entry with no sound", [this]()
		{
			FSkillSFXEntry Silent;
			Silent.Label = TEXT("Silent");
			TestFalse(TEXT("nothing at a spot"), SkillSounds::PlayAtLocation(GetTransientPackage(), Silent, FVector::ZeroVector));
			TestFalse(TEXT("nothing attached"), SkillSounds::PlayAttached(Silent, nullptr, NAME_None));
		});
	});

	Describe("GetSoundsString", [this]()
	{
		It("lists each entry with its place, role and sound", [this]()
		{
			const FString Line = SkillSounds::GetSoundsString(SkillWithSounds());
			TestTrue(TEXT("the notify entry"), Line.Contains(TEXT("0 Release (Notify) Release")));
			TestTrue(TEXT("a soundless entry"), Line.Contains(TEXT("2 Silent (Impact) (no sound)")));
			TestEqual(TEXT("none"), SkillSounds::GetSoundsString(nullptr), FString(TEXT("SFX: none")));
		});
	});
}

#endif
