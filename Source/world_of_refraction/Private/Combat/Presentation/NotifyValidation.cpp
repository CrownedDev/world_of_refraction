// NotifyValidation.cpp
// B-368: see NotifyValidation.h.

#include "Combat/Presentation/NotifyValidation.h"
#include "Animation/AnimMontage.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Skills/Definitions/SpellData.h"
#include "Combat/Camera/SignatureCameraCap.h"
#include "Combat/Boss/BossData.h"
#include "LevelSequence.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

namespace
{
	constexpr float MIN_PLAY_RATE = FCombatCameraRules::MinPlayRate; // one source with B-370's camera window
	constexpr float RETURN_LEG_RATE = FCombatCameraRules::ReturnLegRate;
}

namespace NotifyValidation
{
	TArray<FNotifyMark> ReadMarks(const UAnimMontage *Montage, float PlayRate, float OffsetSeconds)
	{
		TArray<FNotifyMark> Marks;
		if (!Montage)
		{
			return Marks;
		}
		const float Rate = FMath::Max(MIN_PLAY_RATE, PlayRate);
		for (const FAnimNotifyEvent &Event : Montage->Notifies)
		{
			const UCombatNotify *Notify = Cast<UCombatNotify>(Event.Notify);
			if (!Notify)
			{
				continue;
			}
			FNotifyMark Mark;
			Mark.Family = Notify->Family;
			Mark.Index = Notify->Index;
			Mark.Seconds = OffsetSeconds + Event.GetTriggerTime() / Rate;
			Mark.TelegraphLeadSeconds = Notify->TelegraphLeadSeconds;
			Marks.Add(Mark);
		}
		Marks.StableSort([](const FNotifyMark &A, const FNotifyMark &B) { return A.Seconds < B.Seconds; });
		return Marks;
	}

	TArray<FNotifyMark> ReadSkillMarks(const USkillDataBase &Skill)
	{
		const float Rate = FMath::Max(MIN_PLAY_RATE, Skill.BaseAnimSpeed);
		TArray<FNotifyMark> Marks;
		float Offset = 0.0f;
		// The ritual and skill legs play at the skill's rate, the return leg at 1.0 (as the runner plays them).
		struct FLeg
		{
			const UAnimMontage *Montage;
			float Rate;
		};
		const FLeg Legs[] = {{Skill.RitualCastMontage, Rate}, {Skill.SkillMontage, Rate}, {Skill.ReturnMontage, RETURN_LEG_RATE}};
		for (const FLeg &Leg : Legs)
		{
			if (!Leg.Montage)
			{
				continue;
			}
			Marks.Append(ReadMarks(Leg.Montage, Leg.Rate, Offset));
			Offset += Leg.Montage->GetPlayLength() / Leg.Rate;
		}
		return Marks;
	}

	TArray<FString> CheckMarks(const TArray<FNotifyMark> &Marks, const FSkillArrays &Arrays)
	{
		TArray<FString> Problems;
		TArray<int32> ImpactOrder;
		for (const FNotifyMark &Mark : Marks)
		{
			switch (Mark.Family)
			{
			case ECombatNotifyFamily::Impact:
				ImpactOrder.Add(Mark.Index);
				break;
			case ECombatNotifyFamily::Cast:
				if (Arrays.CastCount > 0 && (Mark.Index < 0 || Mark.Index >= Arrays.CastCount))
				{
					Problems.Add(FString::Printf(TEXT("Cast%d at %.2fs is outside CastArray (%d entries)"), Mark.Index, Mark.Seconds, Arrays.CastCount));
				}
				break;
			case ECombatNotifyFamily::VFX:
				if (Mark.Index < 0 || Mark.Index >= Arrays.VFXCount)
				{
					Problems.Add(FString::Printf(TEXT("VFX%d at %.2fs is outside VFXArray (%d entries)"), Mark.Index, Mark.Seconds, Arrays.VFXCount));
				}
				break;
			case ECombatNotifyFamily::SFX:
				break; // no SFX array yet (the runner logs and skips)
			}
		}

		// A skill whose hits are its casts' arrivals may carry no Impact notify at all.
		if (ImpactOrder.IsEmpty() && Arrays.bHitsMayComeFromCasts)
		{
			return Problems;
		}
		const int32 HitCount = FMath::Max(1, Arrays.HitCount);
		bool bInOrder = ImpactOrder.Num() == HitCount;
		for (int32 i = 0; bInOrder && i < ImpactOrder.Num(); ++i)
		{
			bInOrder = ImpactOrder[i] == i;
		}
		if (!bInOrder)
		{
			FString Found;
			for (const int32 Index : ImpactOrder)
			{
				Found += FString::Printf(TEXT("%s%d"), Found.IsEmpty() ? TEXT("") : TEXT(", "), Index);
			}
			Problems.Add(FString::Printf(TEXT("Impact notifies [%s] should be 0..%d once each, in order (HitCount %d)"),
										 Found.IsEmpty() ? TEXT("none") : *Found, HitCount - 1, HitCount));
		}
		return Problems;
	}

	bool FirstDefendableSeconds(const TArray<FNotifyMark> &Marks, float &OutSeconds)
	{
		bool bFound = false;
		for (const FNotifyMark &Mark : Marks)
		{
			if (Mark.Family != ECombatNotifyFamily::Impact && Mark.Family != ECombatNotifyFamily::Cast)
			{
				continue;
			}
			const float Defendable = FMath::Max(0.0f, Mark.Seconds - Mark.TelegraphLeadSeconds);
			OutSeconds = bFound ? FMath::Min(OutSeconds, Defendable) : Defendable;
			bFound = true;
		}
		return bFound;
	}

	bool CinematicWindowFits(float WindowEndSeconds, float FirstDefendable)
	{
		return WindowEndSeconds <= FirstDefendable - MIN_DEFENSE_READ_SECONDS + KINDA_SMALL_NUMBER;
	}

	TArray<FString> CheckSkill(const USkillDataBase &Skill)
	{
		if (!Skill.SkillMontage)
		{
			return {};
		}
		FSkillArrays Arrays;
		Arrays.HitCount = Skill.HitCount;
		Arrays.CastCount = Skill.CastArray.Num();
		Arrays.VFXCount = Skill.VFXArray.Num();
		Arrays.bHitsMayComeFromCasts = Arrays.CastCount > 0 || Skill.IsA<USpellData>();
		// TODO: B-370 adds the cinematic window; check it here with CinematicWindowFits.
		return CheckMarks(ReadSkillMarks(Skill), Arrays);
	}

	TArray<FString> CheckSignatureSeconds(float SequenceSeconds, bool bBossSkill)
	{
		if (bBossSkill || !SignatureCameraCap::IsOverCap(SequenceSeconds))
		{
			return {};
		}
		return {FString::Printf(TEXT("warning: CameraSequence is %.2fs, over the %.2fs signature cap: a player or team cast cuts back there (FeelGuide 9; fine if only enemies use it)"),
								SequenceSeconds, SignatureCameraCap::MaxSignatureSeconds)};
	}

	TArray<FString> CheckSignatureSequence(const USkillDataBase &Skill, bool bBossSkill)
	{
		if (Skill.CameraSequence.IsNull())
		{
			return {};
		}
		return CheckSignatureSeconds(SignatureCameraCap::GetSequenceSeconds(Skill.CameraSequence.LoadSynchronous()), bBossSkill);
	}

	FString GetValidationReportString()
	{
#if !WITH_EDITOR
		return TEXT("(editor only)");
#else
		IAssetRegistry &Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.WaitForCompletion(); // a half-scanned registry would read as "all fit"
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(USkillDataBase::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);

		// B-407: the skills a boss phase adds are a boss's; their sequences are not capped.
		TSet<const USkillDataBase *> BossSkills;
		TArray<FAssetData> BossAssets;
		Registry.GetAssetsByClass(UBossData::StaticClass()->GetClassPathName(), BossAssets, /*bSearchSubClasses=*/true);
		for (const FAssetData &BossAsset : BossAssets)
		{
			if (const UBossData *Boss = Cast<UBossData>(BossAsset.GetAsset()))
			{
				for (const FBossPhase &Phase : Boss->Phases)
				{
					for (const USkillDataBase *Added : Phase.AddedSkills)
					{
						BossSkills.Add(Added);
					}
				}
			}
		}

		FString Out = FString::Printf(TEXT("%d skill assets scanned\n"), Assets.Num());
		for (const FAssetData &Asset : Assets)
		{
			const USkillDataBase *Skill = Cast<USkillDataBase>(Asset.GetAsset());
			if (!Skill)
			{
				continue;
			}
			TArray<FString> Problems = CheckSkill(*Skill);
			Problems.Append(CheckSignatureSequence(*Skill, BossSkills.Contains(Skill)));
			for (const FString &Problem : Problems)
			{
				Out += FString::Printf(TEXT("%s: %s\n"), *Asset.GetObjectPathString(), *Problem);
			}
		}
		return Out;
#endif
	}
}
