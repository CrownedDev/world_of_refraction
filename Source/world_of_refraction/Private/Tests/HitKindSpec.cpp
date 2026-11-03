// HitKindSpec.cpp
// B-273 (S6): a hit's kind in one struct. FromPair keeps today's rule; an Ice hit is Water-keyed, deals
// Ice and reads Ice resistance alone (Q-145b); its tag sits under Water so Water triggers hear it (D-029, ED-04, Q-273).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Combat/Damage/WoRHitKind.h"
#include "Combat/Elements/ElementTable.h"
#include "Combat/Resistance/ResistanceQuery.h"
#include "Tags/WoRGameplayTags.h"

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
