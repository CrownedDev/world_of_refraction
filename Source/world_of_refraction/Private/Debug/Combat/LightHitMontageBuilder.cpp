// LightHitMontageBuilder.cpp
// B-424 (editor only): builds the light hit's two montages, which the editor's Python can't author (a second segment,
// the length, notify subobjects). Run headless by Scripts/editor/build_light_hit.py; that script saves the packages.
//   wor.Editor.BuildLightHitMontages
//   wor.Editor.BuildLightHitFieldMontage (B-503b: the strike alone, for the field attack)
//   wor.Editor.BuildFieldPulseMontage (B-503d: the field pulse's cast, the throw cut from AS_Cast_FireBall; it shares
//   this file's montage helpers)
//   wor.Editor.BuildQuickStrikeMontage (B-595a: the dash + the one-handed sword's slash, for Quick Strike)
//   wor.Editor.BuildForceStrikeMontage (B-595e: a standing slash, Cast0 on the cut, for Force Strike's projectile)
//   wor.Editor.BuildRallyMontage (B-595h: Kwang's charge, VFX0 and SFX0 as it starts, for Rally's self buff)
//   wor.Editor.BuildOverUnderMontage (B-595b: the dash + a cut down and a cut up, Impact0 and Impact1)
//   wor.Editor.BuildHeavyAttackMontages (B-595c: Heavy Attack's wind-up turn, and its dash + overhead lunge)
//   wor.Editor.BuildWhirlWindMontage (B-595d: the dash + a spin ending in a slam, for WhirlWind)
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
	constexpr const TCHAR *FIELD_MONTAGE = TEXT("AM_SAS_LightHit_Field");

	// ---- B-503d: the field pulse's cast (D-215) ----
	constexpr const TCHAR *PULSE_FOLDER = TEXT("/Game/Data/Skills/Field/Pulse");
	constexpr const TCHAR *PULSE_MONTAGE = TEXT("AM_Field_Pulse");
	/** A right-hand throw, in place, read only (vendor folder). The clip is 7 s; only the throw is used. */
	constexpr const TCHAR *PULSE_CLIP = TEXT("/Game/Imports/Animations/MagicAnimations/Animations/AS_Cast_FireBall.AS_Cast_FireBall");
	/** In the clip's own time (Scripts/ue/sample_bones.sh, hand_r): the hands are drawn back at 4.0 s, the right hand
	 *  is fastest at 4.47 s and fully forward by 4.6 s, and it stays there until 5.8 s. */
	constexpr float PULSE_CLIP_START = 4.0f;
	constexpr float PULSE_CLIP_END = 5.2f;
	/** In the montage's time: 4.5 s of the clip, the hand at full speed and nearly forward. */
	constexpr float PULSE_RELEASE_TIME = 0.5f;

	// ---- B-595a: Quick Strike, the light hit's dash with the one-handed sword's slash ----
	constexpr const TCHAR *QUICK_STRIKE_FOLDER = TEXT("/Game/Data/Skills/Weapons/Sword/QuickStrike");
	constexpr const TCHAR *QUICK_STRIKE_MONTAGE = TEXT("AM_Sword_QuickStrike");
	/** The slash, read only (vendor folder). Root motion: a 1.2 m step in, which the warp subtracts. */
	constexpr const TCHAR *QUICK_STRIKE_CLIP = TEXT("/Game/CombatMasterAnimBundle/Animations/HandsomeSwordV1/Manny_UE5/RootMotion/Attack/Anim_Sword_1H_Attack_Slash1.Anim_Sword_1H_Attack_Slash1");
	/** In the slash's own time: weapon_r is fastest at frame 20 of 75 (60 fps) and slows sharply after
	 *  (Scripts/ue/sample_bones.sh, one clear peak). */
	constexpr float QUICK_STRIKE_IMPACT_TIME = 0.333f;

	// ---- B-595e: Force Strike, a standing slash that sends a projectile (CastArray[0]) ----
	constexpr const TCHAR *FORCE_STRIKE_FOLDER = TEXT("/Game/Data/Skills/Weapons/Sword/ForceStrike");
	constexpr const TCHAR *FORCE_STRIKE_MONTAGE = TEXT("AM_Sword_ForceStrike");
	/** In place, read only (vendor folder): a rising lift (the wind-up), then a flat cut. */
	constexpr const TCHAR *FORCE_STRIKE_CLIP = TEXT("/Game/CombatMasterAnimBundle/Animations/DSAS_V1/Manny_UE5/InPlace/Attack/CommonAttack/Anim_SAS_Slash1_IP.Anim_SAS_Slash1_IP");
	/** The flat cut: weapon_r is fastest at frame 38 of 80 (40 fps) and stops sharply by frame 41 (sample_bones.sh). */
	constexpr float FORCE_STRIKE_RELEASE_TIME = 0.95f;

	// ---- B-595h: Rally, Kwang's charge (the sword drawn up), a self damage buff ----
	constexpr const TCHAR *RALLY_FOLDER = TEXT("/Game/Data/Skills/Weapons/Sword/Rally");
	constexpr const TCHAR *RALLY_MONTAGE = TEXT("AM_Sword_Rally");
	/** In place, read only (vendor folder): Kwang's ultimate intro retargeted to Manny (the same rig as AN_KwangAttack_C),
	 *  1.53 s. SoftTofu's Ability_R_Intro_Wind is this move on Kwang's own skeleton. */
	constexpr const TCHAR *RALLY_CLIP = TEXT("/Game/Imports/Paragon/ParagonAnimations/KwangManny/Ability/Ability_R_Intro.Ability_R_Intro");
	/** VFX0 (the charge energy at the sword) and SFX0 (the charge sound, B-618) as the draw-up starts. */
	constexpr float RALLY_CHARGE_TIME = 0.1f;

	// ---- B-595b: Over Under, the light hit's dash then a cut down and a cut up (two Impacts) ----
	constexpr const TCHAR *OVER_UNDER_FOLDER = TEXT("/Game/Data/Skills/Weapons/Sword/OverUnder");
	constexpr const TCHAR *OVER_UNDER_MONTAGE = TEXT("AM_Sword_OverUnder");
	/** Root motion, read only (vendor folder): a three-swing combo; only its 2nd (down) and 3rd (rising) swings are used. */
	constexpr const TCHAR *OVER_UNDER_CLIP = TEXT("/Game/CombatMasterAnimBundle/Animations/HandsomeSwordV1/Manny_UE5/RootMotion/Attack/Anim_Sword_1H_Attack_Combo1.Anim_Sword_1H_Attack_Combo1");
	/** In the clip's own time (sample_bones.sh, weapon_r at 60 fps): the sword is raised between swings at 0.65 s (the
	 *  pose nearest the dash's end, closer than Quick Strike's join), settled after the rising cut by 1.65 s. */
	constexpr float OVER_UNDER_CLIP_START = 0.65f;
	constexpr float OVER_UNDER_CLIP_END = 1.65f;
	/** In the segment's time: the down cut peaks at 0.883 s of the clip (z 139 -> 97), the rising cut at 1.383 s (z 106 -> 174). */
	constexpr float OVER_UNDER_OVER_IMPACT_TIME = 0.233f;
	constexpr float OVER_UNDER_UNDER_IMPACT_TIME = 0.733f;

	// ---- B-595c: Heavy Attack, a wind-up turn (the ritual leg), then the dash and one lunging overhead cut ----
	constexpr const TCHAR *HEAVY_ATTACK_FOLDER = TEXT("/Game/Data/Skills/Weapons/Sword/HeavyAttack");
	constexpr const TCHAR *HEAVY_ATTACK_MONTAGE = TEXT("AM_Sword_HeavyAttack");
	constexpr const TCHAR *HEAVY_WIND_UP_MONTAGE = TEXT("AM_Sword_HeavyAttack_WindUp");
	/** Read only (vendor folder): a cut, the sword raised overhead, a lunging cut down (root motion 2.5 m). */
	constexpr const TCHAR *HEAVY_ATTACK_CLIP = TEXT("/Game/CombatMasterAnimBundle/Animations/HandsomeSwordV2/Manny_UE5/RootMotion/Attack/CommonAttack/Anim_SwordV2_HeavyAttack.Anim_SwordV2_HeavyAttack");
	/** Read only (vendor folder): the one-handed sword's in-place power-up gesture, 2.17 s: the arm turn. */
	constexpr const TCHAR *HEAVY_WIND_UP_CLIP = TEXT("/Game/CombatMasterAnimBundle/Animations/HandsomeSwordAndShield/SwordAnimations/Buff/Anim_HSAS_Buff.Anim_HSAS_Buff");
	/** In the clip's own time (sample_bones.sh, weapon_r): 0.78 s is after the first cut, closest to the dash's end pose
	 *  (65 cm); the sword rises overhead to 1.0 s; the lunge reaches furthest forward at 1.133 s; settled by 1.75 s. */
	constexpr float HEAVY_ATTACK_CLIP_START = 0.78f;
	constexpr float HEAVY_ATTACK_CLIP_END = 1.75f;
	constexpr float HEAVY_ATTACK_IMPACT_TIME = 1.133f - HEAVY_ATTACK_CLIP_START;

	// ---- B-595d: WhirlWind, the light hit's dash then a full spin that ends in a slam (one hit on every target) ----
	constexpr const TCHAR *WHIRLWIND_FOLDER = TEXT("/Game/Data/Skills/Weapons/Sword/WhirlWind");
	constexpr const TCHAR *WHIRLWIND_MONTAGE = TEXT("AM_Sword_WhirlWind");
	/** Read only (vendor folder): a 360-degree spinning cut (0.5 s), then a leaping slam down; root motion 3.1 m. Its
	 *  first frame is 16 cm from the dash's end pose. */
	constexpr const TCHAR *WHIRLWIND_CLIP = TEXT("/Game/CombatMasterAnimBundle/Animations/HandsomeSwordV2/Manny_UE5/RootMotion/Attack/CommonAttack/Anim_SwordV2_Slash3.Anim_SwordV2_Slash3");
	/** In the clip's own time (sample_bones.sh, weapon_r): the slam reaches the ground at 0.9 s (z 173 to 56 cm). The
	 *  spinning cut is earlier but the clip still travels 2 m after it, so it would land short. */
	constexpr float WHIRLWIND_IMPACT_TIME = 0.9f;

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

	UAnimMontage *MakeMontage(const TCHAR *Name, const TArray<UAnimSequenceBase *> &Clips, const TCHAR *Folder = FOLDER)
	{
		UPackage *Package = CreatePackage(*(FString(Folder) / Name));
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

		Montage->AddAnimCompositeSection(TEXT("Default"), 0.0f);
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
		Event.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(Event.EndLink.GetTime()));
	}

	void AddCombatNotify(UAnimMontage *Montage, float Time, ECombatNotifyFamily Family, int32 Index = 0)
	{
		UCombatNotify *Notify = NewObject<UCombatNotify>(Montage, NAME_None, RF_Transactional);
		Notify->Family = Family;
		Notify->Index = Index;
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

	/** B-503b (D-215): the map's swing, the strike alone (in place, no warp), with the approach montage's strike notifies. */
	void BuildField()
	{
		UAnimSequenceBase *Strike = LoadObject<UAnimSequenceBase>(nullptr, STRIKE_CLIP);
		if (!Strike)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing the strike clip %s"), STRIKE_CLIP);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		if (LoadObject<UAnimMontage>(nullptr, *(FString(FOLDER) / FIELD_MONTAGE + TEXT(".") + FIELD_MONTAGE), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete it to rebuild"), FIELD_MONTAGE);
			return;
		}
		UAnimMontage *Field = MakeMontage(FIELD_MONTAGE, {Strike});
		AddCombatNotify(Field, STRIKE_VFX_TIME, ECombatNotifyFamily::VFX);
		AddCombatNotify(Field, STRIKE_SFX_TIME, ECombatNotifyFamily::SFX);
		AddCombatNotify(Field, STRIKE_IMPACT_TIME, ECombatNotifyFamily::Impact);
		Finish(Field);
	}

	static FAutoConsoleCommand GBuildFieldCommand(
		TEXT("wor.Editor.BuildLightHitFieldMontage"),
		TEXT("EDITOR (B-503b): build AM_SAS_LightHit_Field (the strike alone: the map's swing) in ")
			TEXT("/Game/Data/Skills/Weapons/SwordAndShield/LightHit. Doesn't save; Scripts/editor/build_light_hit.py does."),
		FConsoleCommandDelegate::CreateStatic(&BuildField));

	/** B-503d (D-215): the field pulse's cast, the clip alone (in place), with the release as its Impact notify. */
	void BuildFieldPulse()
	{
		UAnimSequenceBase *CastClip = LoadObject<UAnimSequenceBase>(nullptr, PULSE_CLIP);
		if (!CastClip)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing the cast clip %s"), PULSE_CLIP);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		if (LoadObject<UAnimMontage>(nullptr, *(FString(PULSE_FOLDER) / PULSE_MONTAGE + TEXT(".") + PULSE_MONTAGE), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete it to rebuild"), PULSE_MONTAGE);
			return;
		}
		UAnimMontage *Pulse = MakeMontage(PULSE_MONTAGE, {CastClip}, PULSE_FOLDER);
		FAnimSegment &Throw = Pulse->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
		Throw.AnimStartTime = PULSE_CLIP_START;
		Throw.AnimEndTime = PULSE_CLIP_END;
		Pulse->SetCompositeLength(Throw.GetLength());
		AddCombatNotify(Pulse, PULSE_RELEASE_TIME, ECombatNotifyFamily::Impact);
		Finish(Pulse);
		UE_LOG(LogWoR, Display, TEXT("[LightHitBuilder] %s: skeleton %s, release at %.2f s of %.2f s"), PULSE_MONTAGE,
			   *GetPathNameSafe(Pulse->GetSkeleton()), PULSE_RELEASE_TIME, Pulse->GetPlayLength());
	}

	static FAutoConsoleCommand GBuildFieldPulseCommand(
		TEXT("wor.Editor.BuildFieldPulseMontage"),
		TEXT("EDITOR (B-503d): build AM_Field_Pulse (the field pulse's cast, Impact0 at the release) in ")
			TEXT("/Game/Data/Skills/Field/Pulse. Doesn't save: save it in the editor, then uncomment FieldPulseMontage in DefaultGame.ini."),
		FConsoleCommandDelegate::CreateStatic(&BuildFieldPulse));

	/** B-595a: Quick Strike's approach, the light hit's dash (same warp window) then the slash with its Impact on the
	 *  contact frame. Its return is the light hit's hop back (AM_SAS_LightHit_Return), set on the skill. */
	void BuildQuickStrike()
	{
		UAnimSequenceBase *Dash = LoadObject<UAnimSequenceBase>(nullptr, DASH_CLIP);
		UAnimSequenceBase *Slash = LoadObject<UAnimSequenceBase>(nullptr, QUICK_STRIKE_CLIP);
		if (!Dash || !Slash)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing a clip (dash %d, slash %d)"), Dash != nullptr, Slash != nullptr);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		if (LoadObject<UAnimMontage>(nullptr, *(FString(QUICK_STRIKE_FOLDER) / QUICK_STRIKE_MONTAGE + TEXT(".") + QUICK_STRIKE_MONTAGE), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete it to rebuild"), QUICK_STRIKE_MONTAGE);
			return;
		}
		UAnimMontage *Montage = MakeMontage(QUICK_STRIKE_MONTAGE, {Dash, Slash}, QUICK_STRIKE_FOLDER);
		AddWarpWindow(Montage, DASH_WARP_START, DASH_WARP_END);
		AddCombatNotify(Montage, Dash->GetPlayLength() + QUICK_STRIKE_IMPACT_TIME, ECombatNotifyFamily::Impact);
		Finish(Montage);
	}

	static FAutoConsoleCommand GBuildQuickStrikeCommand(
		TEXT("wor.Editor.BuildQuickStrikeMontage"),
		TEXT("EDITOR (B-595a): build AM_Sword_QuickStrike (the light hit's dash + the one-handed slash, Impact0 on contact) in ")
			TEXT("/Game/Data/Skills/Weapons/Sword/QuickStrike. Doesn't save: save it in the editor."),
		FConsoleCommandDelegate::CreateStatic(&BuildQuickStrike));

	/** B-595e: Force Strike's swing, the clip alone (in place, no warp), Cast0 on the cut: the projectile leaves there. */
	void BuildForceStrike()
	{
		UAnimSequenceBase *Swing = LoadObject<UAnimSequenceBase>(nullptr, FORCE_STRIKE_CLIP);
		if (!Swing)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing the swing clip %s"), FORCE_STRIKE_CLIP);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		if (LoadObject<UAnimMontage>(nullptr, *(FString(FORCE_STRIKE_FOLDER) / FORCE_STRIKE_MONTAGE + TEXT(".") + FORCE_STRIKE_MONTAGE), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete it to rebuild"), FORCE_STRIKE_MONTAGE);
			return;
		}
		UAnimMontage *Montage = MakeMontage(FORCE_STRIKE_MONTAGE, {Swing}, FORCE_STRIKE_FOLDER);
		AddCombatNotify(Montage, FORCE_STRIKE_RELEASE_TIME, ECombatNotifyFamily::Cast);
		Finish(Montage);
	}

	static FAutoConsoleCommand GBuildForceStrikeCommand(
		TEXT("wor.Editor.BuildForceStrikeMontage"),
		TEXT("EDITOR (B-595e): build AM_Sword_ForceStrike (a standing slash, Cast0 on the cut) in ")
			TEXT("/Game/Data/Skills/Weapons/Sword/ForceStrike. Doesn't save: save it in the editor."),
		FConsoleCommandDelegate::CreateStatic(&BuildForceStrike));

	/** B-595h: Rally's charge, the clip alone (in place, no warp, no Impact: a self buff applies when the action ends). */
	void BuildRally()
	{
		UAnimSequenceBase *Charge = LoadObject<UAnimSequenceBase>(nullptr, RALLY_CLIP);
		if (!Charge)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing the charge clip %s"), RALLY_CLIP);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		if (LoadObject<UAnimMontage>(nullptr, *(FString(RALLY_FOLDER) / RALLY_MONTAGE + TEXT(".") + RALLY_MONTAGE), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete it to rebuild"), RALLY_MONTAGE);
			return;
		}
		UAnimMontage *Montage = MakeMontage(RALLY_MONTAGE, {Charge}, RALLY_FOLDER);
		AddCombatNotify(Montage, RALLY_CHARGE_TIME, ECombatNotifyFamily::VFX);
		AddCombatNotify(Montage, RALLY_CHARGE_TIME, ECombatNotifyFamily::SFX);
		Finish(Montage);
	}

	static FAutoConsoleCommand GBuildRallyCommand(
		TEXT("wor.Editor.BuildRallyMontage"),
		TEXT("EDITOR (B-595h): build AM_Sword_Rally (Kwang's charge on Manny, VFX0 and SFX0 as it starts) in ")
			TEXT("/Game/Data/Skills/Weapons/Sword/Rally. Doesn't save: save it in the editor."),
		FConsoleCommandDelegate::CreateStatic(&BuildRally));

	/** B-595b: Over Under's approach, the light hit's dash (same warp window) then the combo's down cut and rising cut,
	 *  Impact0 and Impact1 on their contact frames. Its return is the light hit's hop back, set on the skill. */
	void BuildOverUnder()
	{
		UAnimSequenceBase *Dash = LoadObject<UAnimSequenceBase>(nullptr, DASH_CLIP);
		UAnimSequenceBase *Combo = LoadObject<UAnimSequenceBase>(nullptr, OVER_UNDER_CLIP);
		if (!Dash || !Combo)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing a clip (dash %d, combo %d)"), Dash != nullptr, Combo != nullptr);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		if (LoadObject<UAnimMontage>(nullptr, *(FString(OVER_UNDER_FOLDER) / OVER_UNDER_MONTAGE + TEXT(".") + OVER_UNDER_MONTAGE), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete it to rebuild"), OVER_UNDER_MONTAGE);
			return;
		}
		UAnimMontage *Montage = MakeMontage(OVER_UNDER_MONTAGE, {Dash, Combo}, OVER_UNDER_FOLDER);
		FAnimSegment &Cuts = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[1];
		Cuts.AnimStartTime = OVER_UNDER_CLIP_START;
		Cuts.AnimEndTime = OVER_UNDER_CLIP_END;
		const float CutsStart = Dash->GetPlayLength();
		Montage->SetCompositeLength(CutsStart + Cuts.GetLength());
		AddWarpWindow(Montage, DASH_WARP_START, DASH_WARP_END);
		AddCombatNotify(Montage, CutsStart + OVER_UNDER_OVER_IMPACT_TIME, ECombatNotifyFamily::Impact, 0);
		AddCombatNotify(Montage, CutsStart + OVER_UNDER_UNDER_IMPACT_TIME, ECombatNotifyFamily::Impact, 1);
		Finish(Montage);
	}

	static FAutoConsoleCommand GBuildOverUnderCommand(
		TEXT("wor.Editor.BuildOverUnderMontage"),
		TEXT("EDITOR (B-595b): build AM_Sword_OverUnder (the light hit's dash + a cut down and a cut up, Impact0 and Impact1 ")
			TEXT("on contact) in /Game/Data/Skills/Weapons/Sword/OverUnder. Doesn't save: save it in the editor."),
		FConsoleCommandDelegate::CreateStatic(&BuildOverUnder));

	/** B-595c: Heavy Attack's two montages. The wind-up is the arm turn's RitualCastMontage (no notifies); the strike is
	 *  the light hit's dash then the heavy cut, trimmed to the overhead lunge, Impact0 on contact. */
	void BuildHeavyAttack()
	{
		UAnimSequenceBase *Dash = LoadObject<UAnimSequenceBase>(nullptr, DASH_CLIP);
		UAnimSequenceBase *Cut = LoadObject<UAnimSequenceBase>(nullptr, HEAVY_ATTACK_CLIP);
		UAnimSequenceBase *WindUpClip = LoadObject<UAnimSequenceBase>(nullptr, HEAVY_WIND_UP_CLIP);
		if (!Dash || !Cut || !WindUpClip)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing a clip (dash %d, cut %d, wind-up %d)"), Dash != nullptr, Cut != nullptr,
				   WindUpClip != nullptr);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		for (const TCHAR *Name : {HEAVY_ATTACK_MONTAGE, HEAVY_WIND_UP_MONTAGE})
		{
			if (LoadObject<UAnimMontage>(nullptr, *(FString(HEAVY_ATTACK_FOLDER) / Name + TEXT(".") + Name), nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete both to rebuild"), Name);
				return;
			}
		}
		UAnimMontage *Strike = MakeMontage(HEAVY_ATTACK_MONTAGE, {Dash, Cut}, HEAVY_ATTACK_FOLDER);
		FAnimSegment &CutSegment = Strike->SlotAnimTracks[0].AnimTrack.AnimSegments[1];
		CutSegment.AnimStartTime = HEAVY_ATTACK_CLIP_START;
		CutSegment.AnimEndTime = HEAVY_ATTACK_CLIP_END;
		const float CutStart = Dash->GetPlayLength();
		Strike->SetCompositeLength(CutStart + CutSegment.GetLength());
		AddWarpWindow(Strike, DASH_WARP_START, DASH_WARP_END);
		AddCombatNotify(Strike, CutStart + HEAVY_ATTACK_IMPACT_TIME, ECombatNotifyFamily::Impact);
		Finish(Strike);

		Finish(MakeMontage(HEAVY_WIND_UP_MONTAGE, {WindUpClip}, HEAVY_ATTACK_FOLDER));
	}

	static FAutoConsoleCommand GBuildHeavyAttackCommand(
		TEXT("wor.Editor.BuildHeavyAttackMontages"),
		TEXT("EDITOR (B-595c): build AM_Sword_HeavyAttack (dash + the overhead lunge, Impact0 on contact) and AM_Sword_HeavyAttack_WindUp ")
			TEXT("(the arm turn) in /Game/Data/Skills/Weapons/Sword/HeavyAttack. Doesn't save: save them in the editor."),
		FConsoleCommandDelegate::CreateStatic(&BuildHeavyAttack));

	/** B-595d: WhirlWind's approach, the light hit's dash then the spin and slam, Impact0 on the slam (every target). */
	void BuildWhirlWind()
	{
		UAnimSequenceBase *Dash = LoadObject<UAnimSequenceBase>(nullptr, DASH_CLIP);
		UAnimSequenceBase *Spin = LoadObject<UAnimSequenceBase>(nullptr, WHIRLWIND_CLIP);
		if (!Dash || !Spin)
		{
			UE_LOG(LogWoR, Error, TEXT("[LightHitBuilder] missing a clip (dash %d, spin %d)"), Dash != nullptr, Spin != nullptr);
			return;
		}
		// LoadObject, not FindObject: the montage may be on disk and not in memory, and building over it would replace it.
		if (LoadObject<UAnimMontage>(nullptr, *(FString(WHIRLWIND_FOLDER) / WHIRLWIND_MONTAGE + TEXT(".") + WHIRLWIND_MONTAGE), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UE_LOG(LogWoR, Warning, TEXT("[LightHitBuilder] %s already exists; delete it to rebuild"), WHIRLWIND_MONTAGE);
			return;
		}
		UAnimMontage *Montage = MakeMontage(WHIRLWIND_MONTAGE, {Dash, Spin}, WHIRLWIND_FOLDER);
		AddWarpWindow(Montage, DASH_WARP_START, DASH_WARP_END);
		AddCombatNotify(Montage, Dash->GetPlayLength() + WHIRLWIND_IMPACT_TIME, ECombatNotifyFamily::Impact);
		Finish(Montage);
	}

	static FAutoConsoleCommand GBuildWhirlWindCommand(
		TEXT("wor.Editor.BuildWhirlWindMontage"),
		TEXT("EDITOR (B-595d): build AM_Sword_WhirlWind (the light hit's dash + a spin ending in a slam, Impact0 on the slam) in ")
			TEXT("/Game/Data/Skills/Weapons/Sword/WhirlWind. Doesn't save: save it in the editor."),
		FConsoleCommandDelegate::CreateStatic(&BuildWhirlWind));

	static FAutoConsoleCommand GBuildCommand(
		TEXT("wor.Editor.BuildLightHitMontages"),
		TEXT("EDITOR (B-424): build AM_SAS_LightHit (dash + strike, warp on the hop) and AM_SAS_LightHit_Return in ")
			TEXT("/Game/Data/Skills/Weapons/SwordAndShield/LightHit. Doesn't save; Scripts/editor/build_light_hit.py does."),
		FConsoleCommandDelegate::CreateStatic(&Build));
}

#endif
