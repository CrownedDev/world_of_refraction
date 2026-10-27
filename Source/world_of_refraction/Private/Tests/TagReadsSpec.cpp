// TagReadsSpec.cpp
// GAS stage 3 (B-042x): with `wor.Effects.UseGAS 1` the Stun / Silence / HealBlock / immunity / DOT
// checks read the holder's tags (Status.*, Immunity.*), not the enum store; with 0 they read the store.

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
		UWoREffectMirrorComponent::SetReadsFromGAS(false);
		EffectManager = nullptr;
		ASC = nullptr;
		Hero = nullptr;
		Foe = nullptr;
		TestWorld.Reset();
	});

	It("reads each status and immunity from the holder's tag with UseGAS 1, and from the store with 0", [this]()
	{
		if (!EffectManager) return;
		for (const FTagReadCase &Case : TagReadCases())
		{
			// A tag with no stored effect behind it: only a tag read sees it.
			ASC->AddLooseGameplayTag(Case.Tag);
			UWoREffectMirrorComponent::SetReadsFromGAS(false);
			TestFalse(FString::Printf(TEXT("UseGAS 0: not %s from a tag alone"), Case.Label), Case.Read(*EffectManager, Hero));
			UWoREffectMirrorComponent::SetReadsFromGAS(true);
			TestTrue(FString::Printf(TEXT("UseGAS 1: %s from the tag"), Case.Label), Case.Read(*EffectManager, Hero));
			TestTrue(FString::Printf(TEXT("UseGAS 1: GetEffectTags lists %s"), *Case.Tag.ToString()),
					 EffectManager->GetEffectTags(Hero).HasTagExact(Case.Tag));
			ASC->RemoveLooseGameplayTag(Case.Tag);
			TestFalse(FString::Printf(TEXT("UseGAS 1: not %s once the tag goes"), Case.Label), Case.Read(*EffectManager, Hero));
		}
	});

	It("reads a stored stun the same on both paths, tags included", [this]()
	{
		if (!EffectManager) return;
		EffectManager->ApplyEffect(Hero, FActiveSkillEffect::CreateBuff(TEXT("Tag read stun"), TAG_READ_STUN_ID,
																		ESkillEffectType::Stun, 0.0f, TAG_READ_TURNS),
								   Foe);
		for (const bool bReadsFromGAS : {false, true})
		{
			UWoREffectMirrorComponent::SetReadsFromGAS(bReadsFromGAS);
			const TCHAR *Path = bReadsFromGAS ? TEXT("UseGAS 1") : TEXT("UseGAS 0");
			TestTrue(FString::Printf(TEXT("%s: stunned"), Path), EffectManager->IsStunned(Hero));
			TestTrue(FString::Printf(TEXT("%s: GetEffectTags lists Status.Stun"), Path),
					 EffectManager->GetEffectTags(Hero).HasTagExact(WoRTags::Status_Stun));
			TestFalse(FString::Printf(TEXT("%s: no DOT"), Path), EffectManager->HasActiveDOT(Hero));
		}
	});
}

#endif
