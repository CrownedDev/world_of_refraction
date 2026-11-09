// HitKindSpec.cpp
// B-273 (S6): a hit's kind in one struct. FromPair keeps today's rule; an Ice hit is Water-keyed, deals
// Ice and reads Ice resistance alone (Q-145b); its tag sits under Water so Water triggers hear it (D-029, ED-04, Q-273).
// B-273b: the pair path and the FromPair hit-kind path give the same trigger, buildup and bar; an Ice hit breaks on
// the Ice row and its bar remembers it.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/WoRHitKind.h"
#include "Combat/Elements/ElementTable.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Skills/Effects/BarCapTriggerResolver.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	/** Well under the bar's cap, so no hit fires a break. */
	constexpr float HIT_KIND_BUILDUP = 10.0f;
	constexpr float HIT_KIND_TOLERANCE = 1.0e-4f;

	const TArray<ESpellElement> &HitKindElements()
	{
		static const TArray<ESpellElement> Elements = {
			ESpellElement::None, ESpellElement::Generic, ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
			ESpellElement::Wind, ESpellElement::Light, ESpellElement::Darkness, ESpellElement::Lightning, ESpellElement::Void,
			ESpellElement::Reality};
		return Elements;
	}

	const TArray<EPhysicalDamageType> &HitKindPhysicals()
	{
		static const TArray<EPhysicalDamageType> Physicals = {EPhysicalDamageType::None, EPhysicalDamageType::Slash,
															  EPhysicalDamageType::Pierce, EPhysicalDamageType::Impact};
		return Physicals;
	}
}

BEGIN_DEFINE_SPEC(FHitKindSpec, "WoR.Resistance.HitKind",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AActor *FireCaster = nullptr;
END_DEFINE_SPEC(FHitKindSpec)

void FHitKindSpec::Define()
{
	It("derives the damage type from the pair as before (the element wins)", [this]()
	{
		TestEqual(TEXT("Fire sword swing"), FWoRHitKind::FromPair(ESpellElement::Fire, EPhysicalDamageType::Slash).DamageType, EWoRDamageType::Fire);
		TestEqual(TEXT("plain swing"), FWoRHitKind::FromPair(ESpellElement::None, EPhysicalDamageType::Slash).DamageType, EWoRDamageType::Slash);
		TestEqual(TEXT("leftover Generic"), FWoRHitKind::FromPair(ESpellElement::Generic, EPhysicalDamageType::None).DamageType, EWoRDamageType::None);
		TestEqual(TEXT("a swing's element axis"), FWoRHitKind::FromPair(ESpellElement::None, EPhysicalDamageType::Slash).ElementType(), EWoRDamageType::None);
	});

	It("makes an Ice hit Water-keyed, dealing Ice", [this]()
	{
		const FWoRHitKind Ice = FWoRHitKind::Ice();
		TestEqual(TEXT("keyed Water"), Ice.Element, ESpellElement::Water);
		TestEqual(TEXT("deals Ice"), Ice.DamageType, EWoRDamageType::Ice);
		TestEqual(TEXT("element axis is Ice"), Ice.ElementType(), EWoRDamageType::Ice);
		TestEqual(TEXT("Water resistance doesn't reach Ice (Q-145b)"), ElementTable::For(EWoRDamageType::Ice).AlsoResistedBy, EWoRDamageType::None);
		TestEqual(TEXT("Water is also resisted by Ice"), ElementTable::For(EWoRDamageType::Water).AlsoResistedBy, EWoRDamageType::Ice);
	});

	It("nests the Ice tag under Water", [this]()
	{
		const FGameplayTag IceTag = WoRTags::DamageTypeToTag(EWoRDamageType::Ice);
		TestEqual(TEXT("tag name"), IceTag.ToString(), FString(TEXT("DamageType.Element.Water.Ice")));
		TestTrue(TEXT("an on-Water trigger hears it"), IceTag.MatchesTag(WoRTags::DamageTypeToTag(EWoRDamageType::Water)));
		EWoRDamageType RoundTrip = EWoRDamageType::None;
		TestTrue(TEXT("tag reads back"), WoRTags::TagToDamageType(IceTag, RoundTrip) && RoundTrip == EWoRDamageType::Ice);
	});

	It("resolves the bar-cap trigger the same by pair and by FromPair hit kind (B-273b)", [this]()
	{
		for (const ESpellElement Element : HitKindElements())
		{
			for (const EPhysicalDamageType Physical : HitKindPhysicals())
			{
				for (const bool bBrokenDarkness : {false, true})
				{
					const FWoRHitKind Kind = FWoRHitKind::FromPair(Element, Physical);
					TestEqual(Kind.ToString(), BarCapTriggerResolver::ResolveTrigger(Kind, bBrokenDarkness),
							  BarCapTriggerResolver::ResolveTrigger(Element, Physical, bBrokenDarkness));
				}
			}
		}
	});

	It("resolves an Ice hit's break on the Ice row, and round-trips its override (B-273b)", [this]()
	{
		TestEqual(TEXT("Ice row"), BarCapTriggerResolver::ResolveTrigger(FWoRHitKind::Ice(), false),
				  ElementTable::For(EWoRDamageType::Ice).BreakTrigger);
		TestEqual(TEXT("override of an Ice hit"), FWoRHitKind::GetDealtTypeOverride(FWoRHitKind::Ice()), EWoRDamageType::Ice);
		TestEqual(TEXT("no override for a pair"), FWoRHitKind::GetDealtTypeOverride(FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::Slash)),
				  EWoRDamageType::None);
		const FWoRHitKind Rebuilt = FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None, EWoRDamageType::Ice);
		TestEqual(TEXT("rebuilt Ice hit"), Rebuilt.DamageType, EWoRDamageType::Ice);
	});

	Describe("resistance", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("test world created"), TestWorld->Create());
			UCharacterData *FireData = FWoRTestWorld::NewCharacterData();
			FireData->CharacterClass = ECharacterClass::Caster;
			FireData->InnateElement = ESpellElement::Fire;
			FireCaster = TestWorld->SpawnCombatant(FireData);
		});

		AfterEach([this]()
		{
			FireCaster = nullptr;
			TestWorld.Reset();
		});

		It("reads Ice resistance alone for an Ice hit, not the Water weakness (Q-145b)", [this]()
		{
			const float Water = WoRResistance::GetTypeResistance(FireCaster, EWoRDamageType::Water);
			const float Ice = WoRResistance::GetTypeResistance(FireCaster, EWoRDamageType::Ice);
			TestTrue(TEXT("the Fire Caster is weak to Water"), Water < 0.0f);
			TestEqual(TEXT("Ice hit"), WoRResistance::GetHitDamageResistance(FireCaster, FWoRHitKind::Ice()), Ice);
			TestEqual(TEXT("Ice hit, buildup axes"), WoRResistance::GetHitTypeResistance(FireCaster, FWoRHitKind::Ice()), Ice);
			TestEqual(TEXT("a Water hit still adds Ice"), WoRResistance::GetHitDamageResistance(FireCaster, FWoRHitKind::FromPair(ESpellElement::Water, EPhysicalDamageType::None)), Water + Ice);
		});

		It("fills the bar the same by pair and by FromPair hit kind (B-273b)", [this]()
		{
			UStatusBuildupManager *Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
			AActor *Other = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
			if (!TestNotNull(TEXT("StatusBuildupManager"), Buildup) || !TestNotNull(TEXT("second target"), Other)) return;
			for (const ESpellElement Element : HitKindElements())
			{
				for (const EPhysicalDamageType Physical : HitKindPhysicals())
				{
					const FWoRHitKind Kind = FWoRHitKind::FromPair(Element, Physical);
					TestEqual(*(Kind.ToString() + TEXT(": gathered")),
							  WoRBuildupMath::Compute(Buildup->GatherBuildupMathInputs(Other, FireCaster, HIT_KIND_BUILDUP, Kind, false)),
							  WoRBuildupMath::Compute(Buildup->GatherBuildupMathInputs(Other, FireCaster, HIT_KIND_BUILDUP, Element, Physical, false)),
							  HIT_KIND_TOLERANCE);
					Buildup->ResetStatusBar(FireCaster);
					Buildup->AddStatusBuildup(Other, FireCaster, HIT_KIND_BUILDUP, Element, Physical);
					const float ByPair = Buildup->GetStatusBarBuildup(FireCaster);
					const ESkillEffectType PairTrigger = Buildup->GetPendingTrigger(FireCaster);
					Buildup->ResetStatusBar(FireCaster);
					Buildup->AddStatusBuildupForHit(Other, FireCaster, HIT_KIND_BUILDUP, Kind);
					TestEqual(*(Kind.ToString() + TEXT(": bar")), Buildup->GetStatusBarBuildup(FireCaster), ByPair, HIT_KIND_TOLERANCE);
					TestEqual(*(Kind.ToString() + TEXT(": pending trigger")), Buildup->GetPendingTrigger(FireCaster), PairTrigger);
					Buildup->ResetStatusBar(FireCaster);
				}
			}
		});

		It("builds an Ice hit against Ice resistance alone, and its bar remembers Ice (B-273b)", [this]()
		{
			UStatusBuildupManager *Buildup = TestWorld->GetSubsystem<UStatusBuildupManager>();
			if (!TestNotNull(TEXT("StatusBuildupManager"), Buildup)) return;
			const FBuildupMathInputs Inputs = Buildup->GatherBuildupMathInputs(nullptr, FireCaster, HIT_KIND_BUILDUP, FWoRHitKind::Ice(), false);
			TestEqual(TEXT("Ice resistance alone"), Inputs.HitTypeResistance, WoRResistance::GetTypeResistance(FireCaster, EWoRDamageType::Ice));
			Buildup->AddStatusBuildupForHit(nullptr, FireCaster, HIT_KIND_BUILDUP, FWoRHitKind::Ice());
			TestEqual(TEXT("keyed Water on the bar"), Buildup->GetPendingElement(FireCaster), ESpellElement::Water);
			TestEqual(TEXT("breaks on the Ice row"), Buildup->GetPendingTrigger(FireCaster), ElementTable::For(EWoRDamageType::Ice).BreakTrigger);
			TestTrue(TEXT("the debug line names Ice"), Buildup->GetBarDebugString(FireCaster).Contains(TEXT("Ice")));
		});

		It("reads the old pair formula for every element and physical type", [this]()
		{
			for (const ESpellElement Element : {ESpellElement::None, ESpellElement::Generic, ESpellElement::Fire, ESpellElement::Water, ESpellElement::Void})
			{
				const EWoRDamageType ElementType = WoRDamageType::ElementToDamageType(Element);
				const float ElementResist = WoRResistance::GetTypeResistance(FireCaster, ElementType) +
											(ElementType == EWoRDamageType::Water ? WoRResistance::GetTypeResistance(FireCaster, EWoRDamageType::Ice) : 0.0f);
				for (const EPhysicalDamageType Physical : {EPhysicalDamageType::None, EPhysicalDamageType::Slash, EPhysicalDamageType::Impact})
				{
					const FWoRHitKind Kind = FWoRHitKind::FromPair(Element, Physical);
					const float PhysicalResist = Physical == EPhysicalDamageType::None ? 0.0f
												 : WoRResistance::GetTypeResistance(FireCaster, WoRDamageType::PhysicalToDamageType(Physical));
					TestEqual(Kind.ToString(), WoRResistance::GetHitTypeResistance(FireCaster, Kind), ElementResist + PhysicalResist);
				}
				const float OldDamage = ElementType == EWoRDamageType::None ? 0.0f : ElementResist;
				TestEqual(TEXT("damage"), WoRResistance::GetHitDamageResistance(FireCaster, FWoRHitKind::FromPair(Element, EPhysicalDamageType::None)), OldDamage);
			}
		});
	});
}

#endif
