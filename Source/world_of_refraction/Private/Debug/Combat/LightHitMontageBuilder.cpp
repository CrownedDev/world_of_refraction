// LightHitMontageBuilder.cpp
// B-424 (editor only): builds the light hit's two montages, which the editor's Python can't author (a second segment,
// the length, notify subobjects). Run headless by Scripts/editor/build_light_hit.py; that script saves the packages.
//   wor.Editor.BuildLightHitMontages
// Times are from docs/analysis/ShowcaseAnimAnalysis.md, "Travel without stretching".

#if WITH_EDITOR

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimNotifyState_MotionWarping.h"
#include "Combat/Actions/ApproachWarp.h"
#include "Combat/CombatConstants.h"
#include "Combat/CombatNotify.h"
#include "HAL/IConsoleManager.h"
#include "RootMotionModifier_SkewWarp.h"
#include "WoRLog.h"

namespace LightHitMontageBuilder
{
	constexpr const TCHAR *FOLDER = TEXT("/Game/Data/Skills/Weapons/SwordAndShield/LightHit");
	/** Our root-motion copies of the vendor clips (build_light_hit.py makes them). */
	constexpr const TCHAR *DASH_CLIP = TEXT("/Game/Data/Skills/Weapons/SwordAndShield/LightHit/AS_SAS_LightHit_Dash.AS_SAS_LightHit_Dash");
	constexpr const TCHAR *HOP_BACK_CLIP = TEXT("/Game/Data/Skills/Weapons/SwordAndShield/LightHit/AS_SAS_LightHit_HopBack.AS_SAS_LightHit_HopBack");
	/** The in-place strike, read only (vendor folder). */
	constexpr const TCHAR *STRIKE_CLIP = TEXT("/Game/CombatMasterAnimBundle/Animations/DSAS_V2/Manny_UE5/InPlace/Attack/Combo/Anim_SAS_V2_ComboAttack02_01_IP.Anim_SAS_V2_ComboAttack02_01_IP");
	constexpr const TCHAR *APPROACH_MONTAGE = TEXT("AM_SAS_LightHit");
	constexpr const TCHAR *RETURN_MONTAGE = TEXT("AM_SAS_LightHit_Return");

	/** The airborne frames only: the feet never slide, the hop becomes the burst. */
	constexpr float DASH_WARP_START = 0.18f;
	constexpr float DASH_WARP_END = 0.42f;
	constexpr float HOP_BACK_WARP_START = 0.17f;
	constexpr float HOP_BACK_WARP_END = 0.40f;
	/** In the strike clip's own time (the analysis' 1.03 / 1.055 / 1.13 with the strike starting at 0.73 s). */
	constexpr float STRIKE_VFX_TIME = 0.30f;
	constexpr float STRIKE_SFX_TIME = 0.325f;
	constexpr float STRIKE_IMPACT_TIME = 0.40f;
	/** Idle to dash and hop back to idle (the idle guard sits lower than the dash's stance). */
	constexpr float IDLE_BLEND_SECONDS = 0.2f;
	/** Teams 800 cm apart minus the 100 cm striking range: the log's sample burst. */
	constexpr float FRONT_ROW_WARP_DISTANCE = 700.0f;
	const FName NOTIFY_TRACK(TEXT("1"));

	UAnimMontage *MakeMontage(const TCHAR *Name, const TArray<UAnimSequenceBase *> &Clips)
	{
		UPackage *Package = CreatePackage(*(FString(FOLDER) / Name));
		UAnimMontage *Montage = NewObject<UAnimMontage>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
		Montage->SetSkeleton(Clips[0]->GetSkeleton());
		Montage->BlendIn.SetBlendTime(IDLE_BLEND_SECONDS);
		Montage->BlendOut.SetBlendTime(IDLE_BLEND_SECONDS);

		// A hard join: each clip ends in the next one's start pose (within 1-3 cm), and a montage can't crossfade segments.
		float Position = 0.0f;
		for (UAnimSequenceBase *Clip : Clips)
		{
			FAnimSegment Segment;
			Segment.SetAnimReference(Clip, true);
			Segment.StartPos = Position;
			Position += Segment.GetLength();
			Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Add(Segment);
		}
		Montage->SetCompositeLength(Position);

		FCompositeSection Section;
		Section.SectionName = TEXT("Default");
		Section.SetTime(0.0f);
		Montage->CompositeSections.Add(Section);
		Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(NOTIFY_TRACK, FLinearColor::White));
		return Montage;
	}

	FAnimNotifyEvent &AddEvent(UAnimMontage *Montage, float Time)
	{
		FAnimNotifyEvent &Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.Link(Montage, Time);
		Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(Time));
		Event.TrackIndex = 0;
		Event.Guid = FGuid::NewGuid();
		return Event;
	}

	/** A Skew Warp to WarpTarget that stops short by the clip's travel after the window (UE's own option). */
	void AddWarpWindow(UAnimMontage *Montage, float Start, float End)
	{
		UAnimNotifyState_MotionWarping *State = NewObject<UAnimNotifyState_MotionWarping>(Montage, NAME_None, RF_Transactional);
		URootMotionModifier_SkewWarp *Warp = NewObject<URootMotionModifier_SkewWarp>(State, NAME_None, RF_Transactional);
		Warp->WarpTargetName = CombatConstants::WARP_TARGET_NAME;
		Warp->bSubtractRemainingRootMotion = true;
		State->RootMotionModifier = Warp;

		FAnimNotifyEvent &Event = AddEvent(Montage, Start);
		Event.NotifyStateClass = State;
		Event.NotifyName = FName(*State->GetNotifyName());
		Event.SetDuration(End - Start);
		Event.EndLink.Link(Montage, Event.EndLink.GetTime());
	}

	void AddCombatNotify(UAnimMontage *Montage, float Time, ECombatNotifyFamily Family)
	{
		UCombatNotify *Notify = NewObject<UCombatNotify>(Montage, NAME_None, RF_Transactional);
		Notify->Family = Family;
		Notify->Index = 0;
		FAnimNotifyEvent &Event = AddEvent(Montage, Time);
		Event.Notify = Notify;
		Event.NotifyName = FName(*Notify->GetNotifyName());
	}

	void Finish(UAnimMontage *Montage)
	{
		Montage->RefreshCacheData();
		Montage->PostEditChange();
		Montage->MarkPackageDirty();
		UE_LOG(LogWoR, Display, TEXT("[LightHitBuilder] built %s: %.3f s, %d notifies; %s"), *Montage->GetPathName(), Montage->GetPlayLength(),
			   Montage->Notifies.Num(), *ApproachWarp::Measure(Montage, FRONT_ROW_WARP_DISTANCE, 1.0f).ToString());
	}

	void Build()
	{
		UAnimSequenceBase *Dash = LoadObject<UAnimSequenceBase>(nullptr, DASH_CLIP);
		UAnimSequenceBase *Strike = LoadObject<UAnimSequenceBase>(nullptr, STRIKE_CLIP);
		UAnimSequenceBase *HopBack = LoadObject<UAnimSequenceBase>(nullptr, HOP_BACK_CLIP);
		if (!Dash || !Strike || !HopBack)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing a clip (dash %d, strike %d, hop back %d); run Scripts/editor/build_light_hit.py"),
				   Dash != nullptr, Strike != nullptr, HopBack != nullptr);
			return;
		}
		if (FindObject<UAnimMontage>(nullptr, *(FString(FOLDER) / APPROACH_MONTAGE + TEXT(".") + APPROACH_MONTAGE)))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete both montages to rebuild"), APPROACH_MONTAGE);
			return;
		}

		UAnimMontage *Approach = MakeMontage(APPROACH_MONTAGE, {Dash, Strike});
		AddWarpWindow(Approach, DASH_WARP_START, DASH_WARP_END);
		const float StrikeStart = Dash->GetPlayLength();
		AddCombatNotify(Approach, StrikeStart + STRIKE_VFX_TIME, ECombatNotifyFamily::VFX);
		AddCombatNotify(Approach, StrikeStart + STRIKE_SFX_TIME, ECombatNotifyFamily::SFX);
		AddCombatNotify(Approach, StrikeStart + STRIKE_IMPACT_TIME, ECombatNotifyFamily::Impact);
		Finish(Approach);

		UAnimMontage *Return = MakeMontage(RETURN_MONTAGE, {HopBack});
		AddWarpWindow(Return, HOP_BACK_WARP_START, HOP_BACK_WARP_END);
		Finish(Return);
	}

	static FAutoConsoleCommand GBuildCommand(
		TEXT("wor.Editor.BuildLightHitMontages"),
		TEXT("EDITOR (B-424): build AM_SAS_LightHit (dash + strike, warp on the hop) and AM_SAS_LightHit_Return in ")
			TEXT("/Game/Data/Skills/Weapons/SwordAndShield/LightHit. Doesn't save; Scripts/editor/build_light_hit.py does."),
		FConsoleCommandDelegate::CreateStatic(&Build));
}

#endif
