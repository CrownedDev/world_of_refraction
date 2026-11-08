// NotifyValidationSpec.cpp
// B-368: the combat notify validator (Combat/Presentation/NotifyValidation.h). Rules on hand-built marks, a
// deliberately broken montage on a skill, and an informational scan of every shipped skill asset.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Combat/CombatNotify.h"
#include "Combat/Presentation/NotifyValidation.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr float NOTIFY_TEST_TOLERANCE = 0.001f;
	constexpr float NOTIFY_TEST_FAST_RATE = 2.0f;

	NotifyValidation::FNotifyMark Mark(ECombatNotifyFamily Family, int32 Index, float Seconds, float Lead = 0.0f)
	{
		NotifyValidation::FNotifyMark Out;
		Out.Family = Family;
		Out.Index = Index;
		Out.Seconds = Seconds;
		Out.TelegraphLeadSeconds = Lead;
		return Out;
	}

	NotifyValidation::FSkillArrays Arrays(int32 HitCount, int32 CastCount, int32 VFXCount, bool bFromCasts = false)
	{
		NotifyValidation::FSkillArrays Out;
		Out.HitCount = HitCount;
		Out.CastCount = CastCount;
		Out.VFXCount = VFXCount;
		Out.bHitsMayComeFromCasts = bFromCasts;
		return Out;
	}

	void AddNotify(UAnimMontage *Montage, ECombatNotifyFamily Family, int32 Index, float Time)
	{
		UCombatNotify *Notify = NewObject<UCombatNotify>(Montage);
		Notify->Family = Family;
		Notify->Index = Index;
		FAnimNotifyEvent &Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.Notify = Notify;
		Event.SetTime(Time);
	}

	using namespace NotifyValidation;
}

BEGIN_DEFINE_SPEC(FNotifyValidationSpec, "WoR.Presentation.NotifyValidation",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FNotifyValidationSpec)

void FNotifyValidationSpec::Define()
{
	Describe("Impact notifies", [this]()
	{
		It("passes 0..HitCount-1 in order", [this]()
		{
			TestEqual(TEXT("two hits in order"), CheckMarks({Mark(ECombatNotifyFamily::Impact, 0, 0.5f), Mark(ECombatNotifyFamily::Impact, 1, 0.8f)}, Arrays(2, 0, 0)).Num(), 0);
		});

		It("fails out of order, missing, doubled or without any on an ability", [this]()
		{
			TestEqual(TEXT("out of order"), CheckMarks({Mark(ECombatNotifyFamily::Impact, 1, 0.5f), Mark(ECombatNotifyFamily::Impact, 0, 0.8f)}, Arrays(2, 0, 0)).Num(), 1);
			TestEqual(TEXT("one missing"), CheckMarks({Mark(ECombatNotifyFamily::Impact, 0, 0.5f), Mark(ECombatNotifyFamily::Impact, 1, 0.8f)}, Arrays(3, 0, 0)).Num(), 1);
			TestEqual(TEXT("one doubled"), CheckMarks({Mark(ECombatNotifyFamily::Impact, 0, 0.5f), Mark(ECombatNotifyFamily::Impact, 0, 0.8f)}, Arrays(1, 0, 0)).Num(), 1);
			const TArray<FString> None = CheckMarks({}, Arrays(1, 0, 0));
			if (TestEqual(TEXT("an ability with none"), None.Num(), 1))
			{
				TestTrue(TEXT("the line says what it found"), None[0].Contains(TEXT("[none]")));
			}
		});

		It("lets a spell carry none, but checks the ones it does carry", [this]()
		{
			TestEqual(TEXT("a spell with none"), CheckMarks({Mark(ECombatNotifyFamily::Cast, 0, 0.4f)}, Arrays(1, 1, 0, true)).Num(), 0);
			TestEqual(TEXT("a spell with a wrong one"), CheckMarks({Mark(ECombatNotifyFamily::Impact, 2, 0.4f)}, Arrays(1, 1, 0, true)).Num(), 1);
		});
	});

	Describe("Cast and VFX notifies", [this]()
	{
		It("fit their arrays", [this]()
		{
			TestEqual(TEXT("Cast1 of two"), CheckMarks({Mark(ECombatNotifyFamily::Cast, 1, 0.4f)}, Arrays(1, 2, 0, true)).Num(), 0);
			TestEqual(TEXT("Cast2 of two"), CheckMarks({Mark(ECombatNotifyFamily::Cast, 2, 0.4f)}, Arrays(1, 2, 0, true)).Num(), 1);
			TestEqual(TEXT("an empty CastArray takes any (loose fallback)"), CheckMarks({Mark(ECombatNotifyFamily::Cast, 5, 0.4f)}, Arrays(1, 0, 0, true)).Num(), 0);
			TestEqual(TEXT("VFX0 of one"), CheckMarks({Mark(ECombatNotifyFamily::VFX, 0, 0.1f), Mark(ECombatNotifyFamily::Impact, 0, 0.5f)}, Arrays(1, 0, 1)).Num(), 0);
			TestEqual(TEXT("VFX1 of one"), CheckMarks({Mark(ECombatNotifyFamily::VFX, 1, 0.1f), Mark(ECombatNotifyFamily::Impact, 0, 0.5f)}, Arrays(1, 0, 1)).Num(), 1);
			TestEqual(TEXT("SFX is not checked yet"), CheckMarks({Mark(ECombatNotifyFamily::SFX, 9, 0.1f), Mark(ECombatNotifyFamily::Impact, 0, 0.5f)}, Arrays(1, 0, 0)).Num(), 0);
		});
	});

	Describe("the defence read time (D-162)", [this]()
	{
		It("finds the first defendable moment, telegraph lead included", [this]()
		{
			float First = 0.0f;
			TestTrue(TEXT("found"), FirstDefendableSeconds({Mark(ECombatNotifyFamily::VFX, 0, 0.1f), Mark(ECombatNotifyFamily::Cast, 0, 1.0f, 0.3f), Mark(ECombatNotifyFamily::Impact, 0, 1.5f)}, First));
			TestEqual(TEXT("the cast minus its lead"), First, 0.7f, NOTIFY_TEST_TOLERANCE);
			TestFalse(TEXT("no Impact or Cast: none"), FirstDefendableSeconds({Mark(ECombatNotifyFamily::VFX, 0, 0.1f)}, First));
		});

		It("keeps MIN_DEFENSE_READ_SECONDS between the window's end and that moment", [this]()
		{
			TestTrue(TEXT("ends exactly in time"), CinematicWindowFits(1.0f - MIN_DEFENSE_READ_SECONDS, 1.0f));
			TestFalse(TEXT("ends too late"), CinematicWindowFits(1.0f - MIN_DEFENSE_READ_SECONDS + 0.05f, 1.0f));
			TestFalse(TEXT("an impact too early for any window"), CinematicWindowFits(0.0f, MIN_DEFENSE_READ_SECONDS - 0.05f));
		});
	});

	Describe("a skill's montage", [this]()
	{
		It("reads its notifies in time order at the skill's speed", [this]()
		{
			UAnimMontage *Montage = NewObject<UAnimMontage>(GetTransientPackage());
			AddNotify(Montage, ECombatNotifyFamily::Impact, 1, 1.0f);
			AddNotify(Montage, ECombatNotifyFamily::Impact, 0, 0.4f);
			const TArray<FNotifyMark> Marks = ReadMarks(Montage, NOTIFY_TEST_FAST_RATE, 0.0f);
			if (TestEqual(TEXT("two marks"), Marks.Num(), 2))
			{
				TestEqual(TEXT("sorted: Impact0 first"), Marks[0].Index, 0);
				TestEqual(TEXT("at twice the speed, half the time"), Marks[1].Seconds, 0.5f, NOTIFY_TEST_TOLERANCE);
			}
		});

		It("fails a deliberately broken montage and passes a good one", [this]()
		{
			UAbilityData *Broken = NewObject<UAbilityData>(GetTransientPackage());
			Broken->HitCount = 2;
			Broken->SkillMontage = NewObject<UAnimMontage>(GetTransientPackage());
			AddNotify(Broken->SkillMontage, ECombatNotifyFamily::Impact, 1, 0.3f);
			AddNotify(Broken->SkillMontage, ECombatNotifyFamily::VFX, 3, 0.1f);
			TestEqual(TEXT("broken: impact order and a VFX index"), CheckSkill(*Broken).Num(), 2);

			UAbilityData *Good = NewObject<UAbilityData>(GetTransientPackage());
			Good->HitCount = 2;
			Good->SkillMontage = NewObject<UAnimMontage>(GetTransientPackage());
			AddNotify(Good->SkillMontage, ECombatNotifyFamily::Impact, 0, 0.3f);
			AddNotify(Good->SkillMontage, ECombatNotifyFamily::Impact, 1, 0.6f);
			TestEqual(TEXT("good"), CheckSkill(*Good).Num(), 0);

			UAbilityData *Montageless = NewObject<UAbilityData>(GetTransientPackage());
			TestEqual(TEXT("no montage: nothing to check"), CheckSkill(*Montageless).Num(), 0);

			USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
			Spell->SkillMontage = NewObject<UAnimMontage>(GetTransientPackage());
			AddNotify(Spell->SkillMontage, ECombatNotifyFamily::Cast, 0, 0.3f);
			TestEqual(TEXT("a spell's cast with no Impact notify"), CheckSkill(*Spell).Num(), 0);
		});
	});

	It("lists the shipped skills' offenders (informational)", [this]()
	{
		// Informational, never fails: real offenders are data fixes, listed in docs/PIE/B-368.md's first check.
		AddInfo(FString::Printf(TEXT("notify validation:\n%s"), *GetValidationReportString()));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
