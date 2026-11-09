// TagReadsSpec.cpp
// GAS stage 3 (B-042x): the Stun / Silence / HealBlock / immunity / DOT checks read the holder's tags
// (Status.*, Immunity.*), not the enum store (B-042g2: the store-read pass went with the UseGAS switch).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GAS/WoREffectMirrorComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Tags/WoRGameplayTags.h"

namespace
{
	constexpr int32 TAG_READ_STUN_ID = 4501;
	constexpr int32 TAG_READ_TURNS = 2;

	/** A check that reads one tag: its name, the tag, and the read under test. */
	struct FTagReadCase
	{
		const TCHAR *Label;
		FGameplayTag Tag;
		TFunction<bool(const USkillEffectManager &, AActor *)> Read;
	};

	TArray<FTagReadCase> TagReadCases()
	{
		return {
			{TEXT("stunned"), WoRTags::Status_Stun, [](const USkillEffectManager &M, AActor *A) { return M.IsStunned(A); }},
			{TEXT("silenced"), WoRTags::Status_Silence, [](const USkillEffectManager &M, AActor *A) { return M.IsSilenced(A); }},
			{TEXT("has a DOT"), WoRTags::Status_DOT, [](const USkillEffectManager &M, AActor *A) { return M.HasActiveDOT(A); }},
			{TEXT("heal-blocked"), WoRTags::Status_HealBlock,
			 [](const USkillEffectManager &M, AActor *A) { return M.IsImmuneToEffectType(A, ESkillEffectType::HealthRestore); }},
			{TEXT("stun-immune"), WoRTags::Immunity_Stun,
			 [](const USkillEffectManager &M, AActor *A) { return M.IsImmuneToEffectType(A, ESkillEffectType::Stun); }},
			{TEXT("silence-immune"), WoRTags::Immunity_Silence,
			 [](const USkillEffectManager &M, AActor *A) { return M.IsImmuneToEffectType(A, ESkillEffectType::Silenced); }},
			{TEXT("DOT-immune"), WoRTags::Immunity_DOT,
			 [](const USkillEffectManager &M, AActor *A) { return M.IsImmuneToEffectType(A, ESkillEffectType::DOT); }},
			{TEXT("all-status-immune"), WoRTags::Immunity_All,
			 [](const USkillEffectManager &M, AActor *A) { return M.IsImmuneToEffectType(A, ESkillEffectType::Stun); }},
		};
	}
}

BEGIN_DEFINE_SPEC(FTagReadsSpec, "WoR.GAS.TagReads",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	USkillEffectManager *EffectManager = nullptr;
	UAbilitySystemComponent *ASC = nullptr;
	AActor *Hero = nullptr;
	AActor *Foe = nullptr;
END_DEFINE_SPEC(FTagReadsSpec)

void FTagReadsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		Hero = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		Foe = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		const UWoREffectMirrorComponent *Mirror = FWoRTestWorld::AddEffectMirror(Hero);
		ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hero);
		if (!TestNotNull(TEXT("SkillEffectManager"), EffectManager) || !TestNotNull(TEXT("Mirror"), Mirror) ||
			!TestNotNull(TEXT("AbilitySystemComponent"), ASC))
		{
			EffectManager = nullptr; // each case returns early
		}
	});

	AfterEach([this]()
	{
		EffectManager = nullptr;
		ASC = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("reads each status and immunity from the holder's tag", [this]()
	{
		if (!EffectManager) return;
		for (const FTagReadCase &Case : TagReadCases())
		{
			// A tag with no stored effect behind it: only a tag read sees it.
			ASC->AddLooseGameplayTag(Case.Tag);
			TestTrue(FString::Printf(TEXT("%s from the tag"), Case.Label), Case.Read(*EffectManager, Hero));
			TestTrue(FString::Printf(TEXT("GetEffectTags lists %s"), *Case.Tag.ToString()),
					 EffectManager->GetEffectTags(Hero).HasTagExact(Case.Tag));
			ASC->RemoveLooseGameplayTag(Case.Tag);
			TestFalse(FString::Printf(TEXT("not %s once the tag goes"), Case.Label), Case.Read(*EffectManager, Hero));
		}
	});

	It("reads a stored stun, tags included", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Tag read stun"), TAG_READ_STUN_ID,
																		ESkillEffectType::Stun, 0.0f, TAG_READ_TURNS),
								   Foe);
		TestTrue(TEXT("stunned"), EffectManager->IsStunned(Hero));
		TestTrue(TEXT("GetEffectTags lists Status.Stun"), EffectManager->GetEffectTags(Hero).HasTagExact(WoRTags::Status_Stun));
		TestFalse(TEXT("no DOT"), EffectManager->HasActiveDOT(Hero));
		TestTrue(TEXT("the stun is cleansable"), EffectManager->HasCleansableEffectOfType(Hero, ESkillEffectType::Stun));
	});

	It("leaves a gear stun out of the cleansable read (D-015)", [this]()
	{
		if (!EffectManager) return;
		FActiveSkillEffect GearStun = FActiveSkillEffect::CreateBuff(TEXT("Tag read gear stun"), TAG_READ_STUN_ID,
																	 ESkillEffectType::Stun, 0.0f, TAG_READ_TURNS);
		GearStun.bFromGear = true;
		EffectManager->ApplyEffect(Hero, GearStun, Foe);
		TestTrue(TEXT("stunned"), EffectManager->IsStunned(Hero));
		TestFalse(TEXT("a gear stun isn't cleansable"), EffectManager->HasCleansableEffectOfType(Hero, ESkillEffectType::Stun));
	});
}

#endif
