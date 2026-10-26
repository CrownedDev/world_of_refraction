// AIStatusQuerySpec.cpp
// The AI's status queries read the effect types the game actually applies (B-023): a
// Lightning bar-cap Stun counts as a dangerous debuff and a valuable status, like the Wind
// SkipTurn, and the self-cleanse branch finds a spell carrying the live CleanseSelf payload.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "AI/AIDecisionManager.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 AI_STATUS_TEST_SOURCE_ID = 9023;
	constexpr int32 AI_STATUS_TEST_DURATION = 2;

	template <class TComponent>
	TComponent *AIStatusTestAddComponent(AActor *Actor)
	{
		TComponent *Component = NewObject<TComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		Component->RegisterComponent();
		return Component;
	}

	/** A transient spell whose one effect carries a single payload of the given type. */
	USpellData *AIStatusTestNewSpell(ESkillEffectType PayloadType)
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = PayloadType;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		Definition->Effects.Add(Effect);
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->ReferencedEffects.Add(Definition);
		return Spell;
	}
}

BEGIN_DEFINE_SPEC(FAIStatusQuerySpec, "WoR.AI.StatusQueries",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
TUniquePtr<FWoRTestWorld> TestWorld;
UAIDecisionManager *AI = nullptr;
USkillEffectManager *EffectManager = nullptr;

bool HasDangerousDebuffWith(ESkillEffectType Type)
{
	AActor *Actor = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	EffectManager->ApplyEffect(Actor, FActiveSkillEffect::CreateBuff(
		UEnum::GetValueAsString(Type), AI_STATUS_TEST_SOURCE_ID, Type, 0.0f, AI_STATUS_TEST_DURATION));
	return AI->HasDangerousDebuff(Actor);
}

/** A Caster whose only spell is the given one (innate). */
ULoadoutComponent *SpawnCasterWithSpell(USpellData *Spell)
{
	UCharacterData *Data = FWoRTestWorld::NewCharacterData();
	Data->CharacterClass = ECharacterClass::Caster;
	AActor *Actor = TestWorld->SpawnCombatant(Data);
	UInventoryComponent *Inventory = AIStatusTestAddComponent<UInventoryComponent>(Actor);
	ULoadoutComponent *Loadout = AIStatusTestAddComponent<ULoadoutComponent>(Actor);
	Loadout->InitializeFromCharacterData(Data, Inventory);
	Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].InnateSpells.Add(Spell);
	return Loadout;
}
END_DEFINE_SPEC(FAIStatusQuerySpec)

void FAIStatusQuerySpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		AI = TestWorld->GetSubsystem<UAIDecisionManager>();
		EffectManager = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("AI decision manager"), AI);
		TestNotNull(TEXT("effect manager"), EffectManager);
	});

	AfterEach([this]()
	{
		TestWorld.Reset();
	});

	Describe("HasDangerousDebuff", [this]()
	{
		It("counts Stun and SkipTurn, not a plain debuff", [this]()
		{
			TestTrue(TEXT("Stun"), HasDangerousDebuffWith(ESkillEffectType::Stun));
			TestTrue(TEXT("SkipTurn"), HasDangerousDebuffWith(ESkillEffectType::SkipTurn));
			TestFalse(TEXT("DefenseDebuff"), HasDangerousDebuffWith(ESkillEffectType::DefenseDebuff));
		});
	});

	Describe("IsValuableStatus", [this]()
	{
		It("always values Stun and SkipTurn", [this]()
		{
			AActor *Target = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
			TestTrue(TEXT("Stun"), AI->IsValuableStatus(ESkillEffectType::Stun, Target));
			TestTrue(TEXT("SkipTurn"), AI->IsValuableStatus(ESkillEffectType::SkipTurn, Target));
		});
	});

	Describe("FindCleanseSpell", [this]()
	{
		It("finds a CleanseSelf spell", [this]()
		{
			USpellData *Spell = AIStatusTestNewSpell(ESkillEffectType::CleanseSelf);
			TestEqual(TEXT("CleanseSelf spell"), AI->FindCleanseSpell(SpawnCasterWithSpell(Spell)), Spell);
		});

		It("skips CleanseAllies, which leaves the caster's own debuffs", [this]()
		{
			USpellData *Spell = AIStatusTestNewSpell(ESkillEffectType::CleanseAllies);
			TestNull(TEXT("CleanseAllies spell"), AI->FindCleanseSpell(SpawnCasterWithSpell(Spell)));
		});

		It("finds nothing without a cleanse spell", [this]()
		{
			USpellData *Spell = AIStatusTestNewSpell(ESkillEffectType::DefenseDebuff);
			TestNull(TEXT("non-cleanse spell"), AI->FindCleanseSpell(SpawnCasterWithSpell(Spell)));
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
