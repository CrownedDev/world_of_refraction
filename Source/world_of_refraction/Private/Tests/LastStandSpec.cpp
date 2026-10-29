// LastStandSpec.cpp
// B-282: Last Stand and the Sapphire revive (ET-37, PA-23, PA-24). A lethal blow on a Last Stand
// holder leaves it at the effect's HP percent (Sapphire 50%; a 0 value falls back to 30%) and uses
// one charge; with two Last Stands the highest fires. Sapphire's window F..S is 2,2,3,3,4,4,5 turns
// with one charge, and Sapphire on a dead ally revives it at 30%.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterDataComponent.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Equipment/Crystals/CrystalEffectTable.h"
#include "Inventory/ItemConstants.h"
#include "Inventory/ItemExecutor.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	/** Round numbers so every percent of it is a whole HP value. */
	constexpr int32 LAST_STAND_TEST_MAX_HP = 1000;

	/** Rules page values (ET-37, PA-23, PA-24), as literals so a changed constant fails here. */
	constexpr float LAST_STAND_TEST_RULE_HP_PERCENT = 50.0f;
	constexpr float LAST_STAND_TEST_RULE_FALLBACK_FRACTION = 0.3f;
	constexpr int32 LAST_STAND_TEST_RULE_HP_AT_50 = 500;
	constexpr int32 LAST_STAND_TEST_RULE_HP_AT_30 = 300;
	constexpr int32 LAST_STAND_TEST_RULE_SAPPHIRE_CHARGES = 1;

	/** Sapphire's window by tier, F..S (ET-37). */
	const EItemTier LAST_STAND_TEST_TIERS[] = {EItemTier::F_Tier, EItemTier::E_Tier, EItemTier::D_Tier,
											   EItemTier::C_Tier, EItemTier::B_Tier, EItemTier::A_Tier,
											   EItemTier::S_Tier};
	const int32 LAST_STAND_TEST_RULE_WINDOWS[] = {2, 2, 3, 3, 4, 4, 5};

	/** A hand-built ward: distinct ids so two of them are held side by side. */
	constexpr int32 LAST_STAND_TEST_WARD_ID = 1;
	constexpr int32 LAST_STAND_TEST_SECOND_WARD_ID = 2;
	constexpr int32 LAST_STAND_TEST_WARD_TURNS = 3;
	constexpr float LAST_STAND_TEST_UNSET_PERCENT = 0.0f;
	constexpr float LAST_STAND_TEST_HIGH_PERCENT = 80.0f;
	constexpr int32 LAST_STAND_TEST_HP_AT_80 = 800;
}

BEGIN_DEFINE_SPEC(FLastStandSpec, "WoR.Effects.LastStand",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UItemExecutor *Items = nullptr;
	USkillEffectManager *Effects = nullptr;
	AActor *User = nullptr;

	UCharacterDataComponent *CharOf(AActor *Actor) const
	{
		return Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	}

	AActor *SpawnAlly()
	{
		AActor *Ally = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (UCharacterDataComponent *Comp = CharOf(Ally))
		{
			Comp->MaxHP = LAST_STAND_TEST_MAX_HP;
			Comp->CurrentHP = LAST_STAND_TEST_MAX_HP;
		}
		return Ally;
	}

	void GiveWard(AActor *Holder, int32 Id, float Percent, int32 Charges)
	{
		FActiveSkillEffect Ward = FActiveSkillEffect::CreateBuff(TEXT("Test last stand"), Id, ESkillEffectType::LastStand,
																 Percent, LAST_STAND_TEST_WARD_TURNS);
		Ward.Charges = Charges;
		Effects->ApplyEffect(Holder, Ward, Holder);
	}

	void LethalBlow(AActor *Target)
	{
		CharOf(Target)->ServerTakeDamage(LAST_STAND_TEST_MAX_HP);
	}
END_DEFINE_SPEC(FLastStandSpec)

void FLastStandSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("test world created"), TestWorld->Create());
		Items = TestWorld->GetSubsystem<UItemExecutor>();
		Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		TestNotNull(TEXT("ItemExecutor"), Items);
		TestNotNull(TEXT("SkillEffectManager"), Effects);
		User = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
	});

	AfterEach([this]()
	{
		Items = nullptr;
		Effects = nullptr;
		User = nullptr;
		TestWorld.Reset();
	});

	It("keeps the rule's numbers in its constants (ET-37, PA-23, PA-24)", [this]()
	{
		TestEqual(TEXT("Last Stand HP percent is 50"), ItemConstants::LAST_STAND_HP_PERCENT, LAST_STAND_TEST_RULE_HP_PERCENT);
		TestEqual(TEXT("fallback and revive fraction is 30%"), ItemConstants::REVIVE_HP_PERCENT, LAST_STAND_TEST_RULE_FALLBACK_FRACTION);
	});

	It("Sapphire's window is 2,2,3,3,4,4,5 turns F..S (ET-37)", [this]()
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(LAST_STAND_TEST_TIERS); ++Index)
		{
			const FCrystalId Sapphire(ECrystalType::Sapphire, LAST_STAND_TEST_TIERS[Index]);
			TestEqual(FString::Printf(TEXT("window at tier index %d"), Index),
					  CrystalEffectTable::GetLastStandWindow(Sapphire), LAST_STAND_TEST_RULE_WINDOWS[Index]);
		}
	});

	It("Sapphire on a living ally grants a ward with that window, 50% and one charge (ET-37, PA-24)", [this]()
	{
		if (!Items || !Effects) return;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(LAST_STAND_TEST_TIERS); ++Index)
		{
			AActor *Ally = SpawnAlly();
			const FCrystalId Sapphire(ECrystalType::Sapphire, LAST_STAND_TEST_TIERS[Index]);
			TestTrue(TEXT("Sapphire used"), Items->UseItem(User, Sapphire, Ally).bSuccess);
			const TArray<FActiveSkillEffect> Wards = Effects->GetEffectsByType(Ally, ESkillEffectType::LastStand);
			if (!TestEqual(TEXT("one ward held"), Wards.Num(), 1))
			{
				continue;
			}
			const FString Tier = FString::Printf(TEXT(" at tier index %d"), Index);
			TestEqual(TEXT("window") + Tier, Wards[0].RemainingTurns, LAST_STAND_TEST_RULE_WINDOWS[Index]);
			TestEqual(TEXT("HP percent") + Tier, Wards[0].EffectValue, LAST_STAND_TEST_RULE_HP_PERCENT);
			TestEqual(TEXT("charges") + Tier, Wards[0].Charges, LAST_STAND_TEST_RULE_SAPPHIRE_CHARGES);
		}
	});

	It("a lethal blow on a Sapphire ward leaves 50% HP and uses its one charge (ET-37, PA-23)", [this]()
	{
		if (!Items || !Effects) return;
		AActor *Ally = SpawnAlly();
		Items->UseItem(User, FCrystalId(ECrystalType::Sapphire, EItemTier::F_Tier), Ally);
		LethalBlow(Ally);
		UCharacterDataComponent *Comp = CharOf(Ally);
		TestTrue(TEXT("still alive"), Comp->bIsAlive);
		TestEqual(TEXT("HP is 50% of max"), Comp->CurrentHP, LAST_STAND_TEST_RULE_HP_AT_50);
		TestEqual(TEXT("HP matches LAST_STAND_HP_PERCENT"), Comp->CurrentHP,
				  FMath::RoundToInt(LAST_STAND_TEST_MAX_HP * ItemConstants::LAST_STAND_HP_PERCENT / 100.0f));
		TestFalse(TEXT("the charge is used, the ward gone"), Effects->HasEffectOfType(Ally, ESkillEffectType::LastStand));
		LethalBlow(Ally);
		TestFalse(TEXT("the next lethal blow kills"), Comp->bIsAlive);
	});

	It("a ward with a 0 value falls back to 30% (ET-37, PA-23)", [this]()
	{
		if (!Effects) return;
		AActor *Ally = SpawnAlly();
		GiveWard(Ally, LAST_STAND_TEST_WARD_ID, LAST_STAND_TEST_UNSET_PERCENT, LAST_STAND_TEST_RULE_SAPPHIRE_CHARGES);
		LethalBlow(Ally);
		UCharacterDataComponent *Comp = CharOf(Ally);
		TestTrue(TEXT("still alive"), Comp->bIsAlive);
		TestEqual(TEXT("HP is 30% of max"), Comp->CurrentHP, LAST_STAND_TEST_RULE_HP_AT_30);
		TestEqual(TEXT("HP matches REVIVE_HP_PERCENT"), Comp->CurrentHP,
				  FMath::RoundToInt(LAST_STAND_TEST_MAX_HP * ItemConstants::REVIVE_HP_PERCENT));
	});

	It("with two Last Stands the highest fires and only it uses a charge (ET-37)", [this]()
	{
		if (!Effects) return;
		AActor *Ally = SpawnAlly();
		GiveWard(Ally, LAST_STAND_TEST_WARD_ID, LAST_STAND_TEST_RULE_HP_PERCENT, LAST_STAND_TEST_RULE_SAPPHIRE_CHARGES);
		GiveWard(Ally, LAST_STAND_TEST_SECOND_WARD_ID, LAST_STAND_TEST_HIGH_PERCENT, LAST_STAND_TEST_RULE_SAPPHIRE_CHARGES);
		LethalBlow(Ally);
		TestEqual(TEXT("HP is the higher ward's 80%"), CharOf(Ally)->CurrentHP, LAST_STAND_TEST_HP_AT_80);
		const TArray<FActiveSkillEffect> Left = Effects->GetEffectsByType(Ally, ESkillEffectType::LastStand);
		if (TestEqual(TEXT("one ward left"), Left.Num(), 1))
		{
			TestEqual(TEXT("the lower ward is kept"), Left[0].EffectValue, LAST_STAND_TEST_RULE_HP_PERCENT);
		}
	});

	It("Sapphire on a dead ally revives it at 30% (PA-24)", [this]()
	{
		if (!Items || !Effects) return;
		AActor *Ally = SpawnAlly();
		UCharacterDataComponent *Comp = CharOf(Ally);
		LethalBlow(Ally);
		if (!TestFalse(TEXT("dead before the Sapphire"), Comp->bIsAlive))
		{
			return;
		}
		TestTrue(TEXT("Sapphire used"),
				 Items->UseItem(User, FCrystalId(ECrystalType::Sapphire, EItemTier::F_Tier), Ally).bSuccess);
		TestTrue(TEXT("alive again"), Comp->bIsAlive);
		TestEqual(TEXT("HP is 30% of max"), Comp->CurrentHP, LAST_STAND_TEST_RULE_HP_AT_30);
		TestEqual(TEXT("HP matches REVIVE_HP_PERCENT"), Comp->CurrentHP,
				  FMath::RoundToInt(LAST_STAND_TEST_MAX_HP * ItemConstants::REVIVE_HP_PERCENT));
		TestFalse(TEXT("a revive grants no ward"), Effects->HasEffectOfType(Ally, ESkillEffectType::LastStand));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
