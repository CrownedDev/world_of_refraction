// BuildupExecutionSpec.cpp
// GAS stage 3 (B-042l): UWoRBuildupExecution reads the source's StatusMultiplier and the target's
// status resistance layers from the ASCs and runs WoRBuildupMath: the same buildup as
// UStatusBuildupManager::AddStatusBuildup for the same hit, and applied as Buildup += that amount.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Combat/CombatConstants.h"
#include "GAS/WoRAttributeSets.h"
#include "GAS/WoRAttributeSyncComponent.h"
#include "GAS/WoRBuildupExecution.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/StatusBuildupManager.h"

namespace
{
	/** Well under the bar's cap, so no hit triggers or resets it. */
	constexpr float BUILDUP_EXEC_AMOUNT = 10.0f;
	constexpr int32 BUILDUP_EXEC_RESIST_ID = 4601;
	constexpr int32 BUILDUP_EXEC_SLASH_RESIST_ID = 4602;
	constexpr int32 BUILDUP_EXEC_MULTIPLIER_ID = 4603;
	constexpr int32 BUILDUP_EXEC_FIRE_MULTIPLIER_ID = 4604;
	constexpr int32 BUILDUP_EXEC_CURSE_ID = 4605;
	constexpr int32 BUILDUP_EXEC_TURNS = 3;
	/** A ModifyStatusResist far below the floor, so the clamp at RESISTANCE_MIN is what counts. */
	constexpr float BUILDUP_EXEC_CURSE_PERCENT = -300.0f;
	/** Percents, as buff values are. */
	constexpr float BUILDUP_EXEC_RESIST_PERCENT = 20.0f;
	constexpr float BUILDUP_EXEC_MULTIPLIER_PERCENT = 50.0f;
	constexpr float BUILDUP_EXEC_TOLERANCE = 1.0e-4f;
	/** An Ice resistance only GAS holds: nothing in the game grants Ice resistance yet (Q-042o). */
	constexpr float BUILDUP_EXEC_ICE_RESIST = 0.2f;
}

BEGIN_DEFINE_SPEC(FBuildupExecutionSpec, "WoR.GAS.BuildupExecution",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UStatusBuildupManager *Buildup = nullptr;
	AActor *Source = nullptr;
	AActor *Target = nullptr;
	UAbilitySystemComponent *SourceASC = nullptr;
	UAbilitySystemComponent *TargetASC = nullptr;

	/** ASC + attribute sync: the attributes hold what the getters return (B-041). */
	static UAbilitySystemComponent *GiveAttributes(AActor *Actor)
	{
		UAbilitySystemComponent *ASC = FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Actor);
		FWoRTestWorld::AddComponent<UWoRAttributeSyncComponent>(Actor);
		return ASC;
	}

	static void Resync(AActor *Actor)
	{
		if (UWoRAttributeSyncComponent *Sync = Actor ? Actor->FindComponentByClass<UWoRAttributeSyncComponent>() : nullptr)
		{
			Sync->SyncAttributes();
		}
	}

	/** The execution's buildup for Request, over the ASCs' attributes. */
	float GASBuildup(const FWoRBuildupRequest &Request) const
	{
		return WoRBuildupMath::Compute(UWoRBuildupExecution::ReadAttributes(Request, SourceASC, TargetASC));
	}

	/** What AddStatusBuildup adds to Target's bar for the hit (the bar is emptied first). */
	float LiveBuildup(ESpellElement Element, EPhysicalDamageType PhysicalType) const
	{
		Buildup->ResetStatusBar(Target);
		const float Before = Buildup->GetStatusBarBuildup(Target);
		Buildup->AddStatusBuildup(Source, Target, BUILDUP_EXEC_AMOUNT, Element, PhysicalType);
		const float Added = Buildup->GetStatusBarBuildup(Target) - Before;
		Buildup->ResetStatusBar(Target);
		return Added;
	}
END_DEFINE_SPEC(FBuildupExecutionSpec)

void FBuildupExecutionSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
		Source = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (Source && Target)
		{
			SourceASC = GiveAttributes(Source);
			TargetASC = GiveAttributes(Target);
			// A raised source multiplier and two target resistances, so each step reads a real attribute.
			if (USkillEffectManager *SEM = TestWorld->GetSubsystem<USkillEffectManager>())
			{
				SEM->ApplyEffect(Source, FActiveSkillEffect::CreateBuff(TEXT("Buildup exec multiplier"), BUILDUP_EXEC_MULTIPLIER_ID,
																	   ESkillEffectType::StatusMultiplierBuff, BUILDUP_EXEC_MULTIPLIER_PERCENT,
																	   BUILDUP_EXEC_TURNS), Source);
				FActiveSkillEffect Fire = FActiveSkillEffect::CreateBuff(TEXT("Buildup exec fire resist"), BUILDUP_EXEC_RESIST_ID,
																		 ESkillEffectType::ResistanceBuff, BUILDUP_EXEC_RESIST_PERCENT,
																		 BUILDUP_EXEC_TURNS);
				Fire.Element = ESpellElement::Fire;
				SEM->ApplyEffect(Target, Fire, Target);
				FActiveSkillEffect Slash = FActiveSkillEffect::CreateBuff(TEXT("Buildup exec slash resist"), BUILDUP_EXEC_SLASH_RESIST_ID,
																		  ESkillEffectType::ResistanceBuff, BUILDUP_EXEC_RESIST_PERCENT,
																		  BUILDUP_EXEC_TURNS);
				Slash.PhysicalType = EPhysicalDamageType::Slash;
				SEM->ApplyEffect(Target, Slash, Target);
			}
			Resync(Source);
			Resync(Target);
		}
		if (!TestNotNull(TEXT("StatusBuildupManager"), Buildup) || !TestNotNull(TEXT("Source ASC"), SourceASC) ||
			!TestNotNull(TEXT("Target ASC"), TargetASC))
		{
			Buildup = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		Buildup = nullptr;
		Source = nullptr;
		Target = nullptr;
		SourceASC = nullptr;
		TargetASC = nullptr;
		TestWorld.Reset();
	});

	It("adds the same buildup as AddStatusBuildup for the same hit", [this]()
	{
		if (!Buildup) return;
		for (const ESpellElement Element : {ESpellElement::None, ESpellElement::Fire, ESpellElement::Lightning, ESpellElement::Water})
		{
			for (const EPhysicalDamageType Physical : {EPhysicalDamageType::None, EPhysicalDamageType::Slash})
			{
				const FString Case = FString::Printf(TEXT("%s %s"), *UEnum::GetValueAsString(Element), *UEnum::GetValueAsString(Physical));
				const FWoRBuildupRequest Request =
					UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT, Element, Physical);
				TestEqual(*(Case + TEXT(": buildup")), GASBuildup(Request), LiveBuildup(Element, Physical), BUILDUP_EXEC_TOLERANCE);
			}
		}
		const FWoRBuildupRequest FireSlash = UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT,
																			   ESpellElement::Fire, EPhysicalDamageType::Slash);
		TestTrue(TEXT("The source multiplier is raised"), FireSlash.Inputs.SourceStatusMultiplier > 1.0f);
		TestTrue(TEXT("Fire + Slash reads real resistances"), FireSlash.Inputs.HitTypeResistance > 0.0f);
	});

	It("matches with an element-keyed multiplier, a resistance below the floor and a baked-in multiplier", [this]()
	{
		if (!Buildup) return;
		USkillEffectManager *SEM = TestWorld->GetSubsystem<USkillEffectManager>();
		if (!TestNotNull(TEXT("SkillEffectManager"), SEM)) return;
		FActiveSkillEffect FireMultiplier = FActiveSkillEffect::CreateBuff(TEXT("Buildup exec fire multiplier"), BUILDUP_EXEC_FIRE_MULTIPLIER_ID,
																		   ESkillEffectType::StatusMultiplierBuff, BUILDUP_EXEC_MULTIPLIER_PERCENT,
																		   BUILDUP_EXEC_TURNS);
		FireMultiplier.Element = ESpellElement::Fire;
		SEM->ApplyEffect(Source, FireMultiplier, Source);
		SEM->ApplyEffect(Target, FActiveSkillEffect::CreateBuff(TEXT("Buildup exec curse"), BUILDUP_EXEC_CURSE_ID,
															   ESkillEffectType::ModifyStatusResist, BUILDUP_EXEC_CURSE_PERCENT,
															   BUILDUP_EXEC_TURNS), Target);
		Resync(Source);
		Resync(Target);
		for (const bool bSkip : {false, true})
		{
			const FString Case = bSkip ? TEXT("baked-in multiplier") : TEXT("full pipeline");
			const FWoRBuildupRequest Request = UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT,
																				 ESpellElement::Fire, EPhysicalDamageType::None, bSkip);
			// A baked-in multiplier skips the element-keyed step too (AddStatusBuildup's step 5b).
			TestEqual(*(Case + TEXT(": element-keyed multiplier read only on the full pipeline")),
					  Request.Inputs.ElementStatusMultiplier > 1.0f, !bSkip);
			TestEqual(*(Case + TEXT(": resistance clamped at the floor")), WoRBuildupMath::GetTotalStatusResistance(Request.Inputs),
					  CombatConstants::RESISTANCE_MIN);
			Buildup->ResetStatusBar(Target);
			Buildup->AddStatusBuildup(Source, Target, BUILDUP_EXEC_AMOUNT, ESpellElement::Fire, EPhysicalDamageType::None, bSkip);
			const float Live = Buildup->GetStatusBarBuildup(Target);
			Buildup->ResetStatusBar(Target);
			TestEqual(*(Case + TEXT(": buildup")), GASBuildup(Request), Live, BUILDUP_EXEC_TOLERANCE);
		}
	});

	It("reads the attributes, not the gathered numbers", [this]()
	{
		if (!Buildup) return;
		FWoRBuildupRequest Request = UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT,
																	   ESpellElement::Fire, EPhysicalDamageType::Slash);
		const FBuildupMathInputs Gathered = Request.Inputs;
		Request.Inputs.SourceStatusMultiplier = 1.0f;
		Request.Inputs.GeneralStatusResistance = 0.0f;
		Request.Inputs.HitTypeResistance = 0.0f;
		Request.Inputs.StatusResistBonus = 0.0f;
		const FBuildupMathInputs Read = UWoRBuildupExecution::ReadAttributes(Request, SourceASC, TargetASC);
		TestEqual(TEXT("StatusMultiplier"), Read.SourceStatusMultiplier, Gathered.SourceStatusMultiplier, BUILDUP_EXEC_TOLERANCE);
		TestEqual(TEXT("StatusResistance"), Read.GeneralStatusResistance, Gathered.GeneralStatusResistance, BUILDUP_EXEC_TOLERANCE);
		TestEqual(TEXT("Resist_Fire + Resist_Slash"), Read.HitTypeResistance, Gathered.HitTypeResistance, BUILDUP_EXEC_TOLERANCE);
		TestEqual(TEXT("StatusResistBonus"), Read.StatusResistBonus, Gathered.StatusResistBonus, BUILDUP_EXEC_TOLERANCE);
	});

	It("cuts Water buildup by Ice resistance too, and only Water (B-226)", [this]()
	{
		if (!Buildup) return;
		const FWoRBuildupRequest Water =
			UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT, ESpellElement::Water, EPhysicalDamageType::None);
		const FWoRBuildupRequest Fire =
			UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT, ESpellElement::Fire, EPhysicalDamageType::None);
		const float WaterBefore = GASBuildup(Water);
		const float FireBefore = GASBuildup(Fire);
		const float WaterHitTypeBefore = UWoRBuildupExecution::ReadAttributes(Water, SourceASC, TargetASC).HitTypeResistance;

		UGameplayEffect *IceResist = NewObject<UGameplayEffect>(GetTransientPackage());
		IceResist->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UWoRAttributeSet_Resistance::GetTypeAttribute(EWoRDamageType::Ice);
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(BUILDUP_EXEC_ICE_RESIST));
		IceResist->Modifiers.Add(Modifier);
		TargetASC->ApplyGameplayEffectToSelf(IceResist, 1.0f, TargetASC->MakeEffectContext());

		TestEqual(TEXT("Water: Ice adds to the hit-type resistance"),
				  UWoRBuildupExecution::ReadAttributes(Water, SourceASC, TargetASC).HitTypeResistance, WaterHitTypeBefore + BUILDUP_EXEC_ICE_RESIST,
				  BUILDUP_EXEC_TOLERANCE);
		TestTrue(TEXT("Water: less buildup"), GASBuildup(Water) < WaterBefore - BUILDUP_EXEC_TOLERANCE);
		TestEqual(TEXT("Fire: unchanged"), GASBuildup(Fire), FireBefore, BUILDUP_EXEC_TOLERANCE);
	});

	It("keeps a baked-in or missing source multiplier", [this]()
	{
		if (!Buildup) return;
		const FWoRBuildupRequest Baked = UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT,
																		   ESpellElement::Fire, EPhysicalDamageType::None, true);
		TestEqual(TEXT("Skip: multiplier stays 1"),
				  UWoRBuildupExecution::ReadAttributes(Baked, SourceASC, TargetASC).SourceStatusMultiplier, 1.0f);
		const FWoRBuildupRequest Sourceless = UWoRBuildupExecution::MakeRequest(*Buildup, nullptr, Target, BUILDUP_EXEC_AMOUNT,
																				ESpellElement::Fire, EPhysicalDamageType::None);
		TestEqual(TEXT("No source: multiplier stays 1"),
				  UWoRBuildupExecution::ReadAttributes(Sourceless, SourceASC, TargetASC).SourceStatusMultiplier, 1.0f);
	});

	It("keeps the gathered numbers when the attributes aren't synced", [this]()
	{
		if (!Buildup) return;
		// An ASC holding the sets but no attribute sync: its attributes are 0, not the getters' values.
		AActor *Unsynced = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		UAbilitySystemComponent *UnsyncedASC = Unsynced ? FWoRTestWorld::AddComponent<UAbilitySystemComponent>(Unsynced) : nullptr;
		if (!TestNotNull(TEXT("Unsynced ASC"), UnsyncedASC)) return;
		UnsyncedASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Stats>(Unsynced));
		UnsyncedASC->AddAttributeSetSubobject(NewObject<UWoRAttributeSet_Resistance>(Unsynced));
		const FWoRBuildupRequest Request = UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT,
																			 ESpellElement::Fire, EPhysicalDamageType::Slash);
		const FBuildupMathInputs Read = UWoRBuildupExecution::ReadAttributes(Request, UnsyncedASC, UnsyncedASC);
		TestEqual(TEXT("Multiplier kept"), Read.SourceStatusMultiplier, Request.Inputs.SourceStatusMultiplier);
		TestEqual(TEXT("Hit-type resistance kept"), Read.HitTypeResistance, Request.Inputs.HitTypeResistance);
	});

	It("adds the buildup to the Buildup attribute when the spec is applied", [this]()
	{
		if (!Buildup) return;
		const FWoRBuildupRequest Request = UWoRBuildupExecution::MakeRequest(*Buildup, Source, Target, BUILDUP_EXEC_AMOUNT,
																			 ESpellElement::Fire, EPhysicalDamageType::None);
		const float Added = GASBuildup(Request);
		const FGameplayAttribute BuildupAttribute = UWoRAttributeSet_Vitals::GetBuildupAttribute();
		const float Before = TargetASC->GetNumericAttribute(BuildupAttribute);
		const FGameplayEffectSpecHandle Spec = UWoRBuildupExecution::MakeBuildupSpec(*SourceASC, Request);
		if (!TestTrue(TEXT("Spec built"), Spec.IsValid())) return;
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
		TestTrue(TEXT("Some buildup"), Added > 0.0f);
		TestEqual(TEXT("Buildup rose by the amount"), TargetASC->GetNumericAttribute(BuildupAttribute), Before + Added,
				  BUILDUP_EXEC_TOLERANCE);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
