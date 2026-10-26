// WoRTagsSpec.cpp
// GAS stage 1 (B-040): the enum <-> tag maps round-trip, and an actor's effect tags give the
// same answers as the enum-based effect queries (UWoRTagDebug::GetTagMismatchString).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Tags/WoRGameplayTags.h"
#include "Debug/Tags/WoRTagDebug.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ActiveSkillEffect.h"

namespace
{
	constexpr int32 TAGS_TEST_STUN_ID = 9101;
	constexpr int32 TAGS_TEST_IMMUNITY_ID = 9102;
	constexpr int32 TAGS_TEST_HEAL_BLOCK_ID = 9103;
	constexpr int32 TAGS_TEST_TURNS = 2;

	const ESpellElement TAGS_TEST_ELEMENTS[] = {
		ESpellElement::Generic, ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
		ESpellElement::Wind, ESpellElement::Light, ESpellElement::Darkness, ESpellElement::Lightning,
		ESpellElement::Void, ESpellElement::Reality, ESpellElement::None};

	const ECharacterClass TAGS_TEST_CLASSES[] = {ECharacterClass::Generic, ECharacterClass::Caster, ECharacterClass::Resonator};
}

BEGIN_DEFINE_SPEC(FWoRTagsSpec, "WoR.Tags",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;

	void Apply(AActor *Target, const TCHAR *Name, int32 EffectID, ESkillEffectType Type)
	{
		EffectManager->ApplyEffect(Target, FActiveSkillEffect::CreateBuff(Name, EffectID, Type, 0.0f, TAGS_TEST_TURNS), Hero);
	}
END_DEFINE_SPEC(FWoRTagsSpec)

void FWoRTagsSpec::Define()
{
	Describe("the enum to tag maps", [this]()
	{
		It("gives every element its own tag and maps it back", [this]()
		{
			TSet<FGameplayTag> Seen;
			for (const ESpellElement Element : TAGS_TEST_ELEMENTS)
			{
				const FGameplayTag Tag = WoRTags::ElementToTag(Element);
				ESpellElement Back = ESpellElement::None;
				TestTrue(FString::Printf(TEXT("%s is valid"), *Tag.ToString()), Tag.IsValid());
				TestFalse(FString::Printf(TEXT("%s is unique"), *Tag.ToString()), Seen.Contains(Tag));
				TestTrue(FString::Printf(TEXT("%s maps back"), *Tag.ToString()), WoRTags::TagToElement(Tag, Back) && Back == Element);
				Seen.Add(Tag);
			}
		});

		It("gives every class its own tag and maps it back", [this]()
		{
			TSet<FGameplayTag> Seen;
			for (const ECharacterClass CharacterClass : TAGS_TEST_CLASSES)
			{
				const FGameplayTag Tag = WoRTags::ClassToTag(CharacterClass);
				ECharacterClass Back = ECharacterClass::Generic;
				TestTrue(FString::Printf(TEXT("%s is valid"), *Tag.ToString()), Tag.IsValid());
				TestFalse(FString::Printf(TEXT("%s is unique"), *Tag.ToString()), Seen.Contains(Tag));
				TestTrue(FString::Printf(TEXT("%s maps back"), *Tag.ToString()), WoRTags::TagToClass(Tag, Back) && Back == CharacterClass);
				Seen.Add(Tag);
			}
		});

		It("gives every tagged effect type its own valid tag", [this]()
		{
			TSet<FGameplayTag> Seen;
			for (const ESkillEffectType Type : WoRTags::TaggedEffectTypes())
			{
				const FGameplayTag Tag = WoRTags::EffectTypeToTag(Type);
				TestTrue(FString::Printf(TEXT("type %d has a tag"), static_cast<int32>(Type)), Tag.IsValid());
				TestFalse(FString::Printf(TEXT("%s is unique"), *Tag.ToString()), Seen.Contains(Tag));
				Seen.Add(Tag);
			}
			TestFalse(TEXT("A stat buff has no tag yet (B-042)"), WoRTags::EffectTypeToTag(ESkillEffectType::SpellDamageBuff).IsValid());
		});

		It("rejects a tag from another branch", [this]()
		{
			ESpellElement Element = ESpellElement::Fire;
			ECharacterClass CharacterClass = ECharacterClass::Caster;
			TestFalse(TEXT("A class tag is not an element"), WoRTags::TagToElement(WoRTags::Class_Caster, Element));
			TestFalse(TEXT("An element tag is not a class"), WoRTags::TagToClass(WoRTags::Element_Fire, CharacterClass));
		});
	});

	Describe("an actor's effect tags", [this]()
	{
		BeforeEach([this]()
		{
			TestWorld = MakeUnique<FWoRTestWorld>();
			TestTrue(TEXT("Test world created"), TestWorld->Create());
			EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
			Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestNotNull(TEXT("SkillEffectManager"), EffectManager);
			TestTrue(TEXT("Combatants spawned"), Hero && Foe);
		});

		AfterEach([this]()
		{
			EffectManager = nullptr;
			Hero = Foe = nullptr;
			TestWorld.Reset();
		});

		It("is empty with no effects, and agrees with the enum queries", [this]()
		{
			if (!EffectManager || !Foe)
			{
				return;
			}
			TestTrue(TEXT("No tags"), EffectManager->GetEffectTags(Foe).IsEmpty());
			const FString Mismatches = UWoRTagDebug::GetTagMismatchString(Foe);
			TestTrue(FString::Printf(TEXT("Tags agree: [%s]"), *Mismatches), Mismatches.IsEmpty());
		});

		It("carries Status.Stun while stunned", [this]()
		{
			if (!EffectManager || !Foe)
			{
				return;
			}
			Apply(Foe, TEXT("Stun"), TAGS_TEST_STUN_ID, ESkillEffectType::Stun);
			TestTrue(TEXT("Enum says stunned"), EffectManager->IsStunned(Foe));
			TestTrue(TEXT("Tag says stunned"), EffectManager->GetEffectTags(Foe).HasTagExact(WoRTags::Status_Stun));
			const FString Mismatches = UWoRTagDebug::GetTagMismatchString(Foe);
			TestTrue(FString::Printf(TEXT("Tags agree: [%s]"), *Mismatches), Mismatches.IsEmpty());
		});

		It("carries Immunity.All and Status.HealBlock, still agreeing", [this]()
		{
			if (!EffectManager || !Foe)
			{
				return;
			}
			// HealBlock first: once the ward is up it refuses new statuses.
			Apply(Foe, TEXT("Rot"), TAGS_TEST_HEAL_BLOCK_ID, ESkillEffectType::HealBlock);
			Apply(Foe, TEXT("Ward"), TAGS_TEST_IMMUNITY_ID, ESkillEffectType::GrantAllStatusImmunity);
			const FGameplayTagContainer Tags = EffectManager->GetEffectTags(Foe);
			TestTrue(TEXT("Immunity.All"), Tags.HasTagExact(WoRTags::Immunity_All));
			TestTrue(TEXT("Status.HealBlock"), Tags.HasTagExact(WoRTags::Status_HealBlock));
			const FString Mismatches = UWoRTagDebug::GetTagMismatchString(Foe);
			TestTrue(FString::Printf(TEXT("Tags agree: [%s]"), *Mismatches), Mismatches.IsEmpty());
		});

		It("drops the tag when the effect is gone", [this]()
		{
			if (!EffectManager || !Foe)
			{
				return;
			}
			Apply(Foe, TEXT("Stun"), TAGS_TEST_STUN_ID, ESkillEffectType::Stun);
			EffectManager->RemoveAllEffects(Foe);
			TestFalse(TEXT("No Status.Stun"), EffectManager->GetEffectTags(Foe).HasTagExact(WoRTags::Status_Stun));
		});
	});
}

#endif
