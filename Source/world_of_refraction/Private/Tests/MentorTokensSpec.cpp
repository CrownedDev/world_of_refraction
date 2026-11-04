// MentorTokensSpec.cpp
// B-361b (D-154, Q-361b): Mentor Tokens. The price ladder and payouts are named constants; solo play pays nothing;
// teaching gives the learner a new copy, leaves the teacher's, and spends the tier price from the teacher's wallet;
// too few tokens refuses with nothing spent; a member's own player can't be taught (give it instead); the tokens
// survive a save.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/MentorTokenRules.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 MENTOR_PARTY_SIZE = 2;
	constexpr int32 MENTOR_TEACHER = 0;
	constexpr int32 MENTOR_LEARNER = 1;
	constexpr int32 MENTOR_TWO_PLAYERS = 2;
	constexpr int32 MENTOR_SPELL_TIER_PRICE = 4; // a C-tier skill: 1 + 3 tiers above F
	constexpr int32 MENTOR_FEW_TOKENS = 2;
	constexpr int32 MENTOR_ENOUGH_TOKENS = 10;
}

BEGIN_DEFINE_SPEC(FMentorTokensSpec, "WoR.Currency.MentorTokens", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	USpellData *Spell = nullptr;
	UAbilityData *Ability = nullptr;
	TArray<UCharacterData *> Members;
	TArray<UInventoryComponent *> Inventories;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }
	bool Ready() const { return Pool && Spell && Ability && !Inventories.Contains(nullptr); }
	UCurrencyComponent *WalletOf(int32 Index) const { return Inventories[Index]->GetOwner()->FindComponentByClass<UCurrencyComponent>(); }

	UInventoryComponent *SpawnMember(int32 Index)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Members[Index]);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		Inventory->AddWeapon(Sword);
		FCombatLoadout Loadout;
		Loadout.PrimarySlotType = EPrimarySlotType::Weapon;
		Loadout.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Inventory->SavedLoadouts = {Loadout};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}
END_DEFINE_SPEC(FMentorTokensSpec)

void FMentorTokensSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Mentor sword");
		Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Mentor spell");
		Spell->Tier = EItemTier::C_Tier;
		Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Mentor ability");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < MENTOR_PARTY_SIZE; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Mentor member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Member.bPlayerControlled = Index == MENTOR_TEACHER;
			Members.Add(Data);
		}
		for (int32 Index = 0; Index < MENTOR_PARTY_SIZE; ++Index)
		{
			Inventories.Add(SpawnMember(Index));
		}
	});

	AfterEach([this]()
	{
		Pool = nullptr;
		Sword = nullptr;
		Spell = nullptr;
		Ability = nullptr;
		Members.Empty();
		Inventories.Empty();
		TestWorld.Reset();
	});

	It("prices a skill by its tier and pays only when two or more play together", [this]()
	{
		TestEqual(TEXT("F costs the base price"), MentorTokenRules::GetTeachPrice(EItemTier::F_Tier), MentorTokenRules::TEACH_PRICE_AT_LOWEST_TIER);
		TestEqual(TEXT("C costs 1 + 3 tiers"), MentorTokenRules::GetTeachPrice(EItemTier::C_Tier), MENTOR_SPELL_TIER_PRICE);
		TestTrue(TEXT("S costs more than A"), MentorTokenRules::GetTeachPrice(EItemTier::S_Tier) > MentorTokenRules::GetTeachPrice(EItemTier::A_Tier));
		TestEqual(TEXT("a solo run pays none"), MentorTokenRules::GetRunPayout(1), 0);
		TestEqual(TEXT("a solo boss pays none"), MentorTokenRules::GetBossPayout(1), 0);
		TestEqual(TEXT("a co-op run pays each player"), MentorTokenRules::GetRunPayout(MENTOR_TWO_PLAYERS), MentorTokenRules::RUN_PAYOUT);
		TestEqual(TEXT("a co-op boss pays each player"), MentorTokenRules::GetBossPayout(MENTOR_TWO_PLAYERS), MentorTokenRules::BOSS_PAYOUT);
	});

	It("pays nobody without players (a headless world)", [this]()
	{
		if (!Ready()) return;
		TestEqual(TEXT("run end"), WoRMentor::PayRunEnd(TestWorld->GetWorld()), 0);
		TestEqual(TEXT("boss"), WoRMentor::PayBossBeaten(TestWorld->GetWorld()), 0);
	});

	It("teaches a spell: the learner gets a copy, the teacher keeps its own and pays the tier price", [this]()
	{
		if (!Ready()) return;
		UWorld *World = TestWorld->GetWorld();
		UCurrencyComponent *Wallet = WalletOf(MENTOR_TEACHER);
		if (!TestNotNull(TEXT("Wallet"), Wallet)) return;
		Wallet->Add(ECurrencyType::MentorTokens, MENTOR_ENOUGH_TOKENS);
		TestTrue(TEXT("the teacher learned it"), Inventories[MENTOR_TEACHER]->LearnSpell(Spell));
		const FText Refusal = WoRGive::TeachSkillPaying(World, Wallet, KeyOf(MENTOR_TEACHER), KeyOf(MENTOR_LEARNER), Spell);
		TestTrue(FString::Printf(TEXT("taught (%s)"), *Refusal.ToString()), Refusal.IsEmpty());
		TestTrue(TEXT("the learner knows it"), Pool->KnowsSpell(KeyOf(MENTOR_LEARNER), Spell));
		TestTrue(TEXT("the teacher keeps it"), Pool->KnowsSpell(KeyOf(MENTOR_TEACHER), Spell));
		TestEqual(TEXT("the price is spent"), Wallet->GetMentorTokens(), MENTOR_ENOUGH_TOKENS - MENTOR_SPELL_TIER_PRICE);
		const FPoolSkills *Teacher = Pool->FindSkills(KeyOf(MENTOR_TEACHER));
		const FPoolSkills *Learner = Pool->FindSkills(KeyOf(MENTOR_LEARNER));
		if (!TestTrue(TEXT("both ledgers"), Teacher && Learner && Teacher->Spells.Num() == 1 && Learner->Spells.Num() == 1)) return;
		TestTrue(TEXT("a new instance, not the same one"), Teacher->Spells[0].InstanceID != Learner->Spells[0].InstanceID);
		TestFalse(TEXT("teaching it again is refused"),
				  WoRGive::TeachSkillPaying(World, Wallet, KeyOf(MENTOR_TEACHER), KeyOf(MENTOR_LEARNER), Spell).IsEmpty());
		TestEqual(TEXT("and spends nothing"), Wallet->GetMentorTokens(), MENTOR_ENOUGH_TOKENS - MENTOR_SPELL_TIER_PRICE);
	});

	It("refuses with too few tokens and spends nothing", [this]()
	{
		if (!Ready()) return;
		UWorld *World = TestWorld->GetWorld();
		UCurrencyComponent *Wallet = WalletOf(MENTOR_TEACHER);
		if (!TestNotNull(TEXT("Wallet"), Wallet)) return;
		Wallet->Add(ECurrencyType::MentorTokens, MENTOR_FEW_TOKENS);
		TestTrue(TEXT("the teacher learned it"), Inventories[MENTOR_TEACHER]->LearnSpell(Spell));
		TestFalse(TEXT("refused"), WoRGive::TeachSkillPaying(World, Wallet, KeyOf(MENTOR_TEACHER), KeyOf(MENTOR_LEARNER), Spell).IsEmpty());
		TestEqual(TEXT("nothing spent"), Wallet->GetMentorTokens(), MENTOR_FEW_TOKENS);
		TestFalse(TEXT("the learner learned nothing"), Pool->KnowsSpell(KeyOf(MENTOR_LEARNER), Spell));
	});

	It("teaches an ability at the lowest tier for the base price", [this]()
	{
		if (!Ready()) return;
		UCurrencyComponent *Wallet = WalletOf(MENTOR_TEACHER);
		if (!TestNotNull(TEXT("Wallet"), Wallet)) return;
		Wallet->Add(ECurrencyType::MentorTokens, MENTOR_ENOUGH_TOKENS);
		TestTrue(TEXT("the teacher learned it"), Inventories[MENTOR_TEACHER]->LearnAbility(Ability));
		TestTrue(TEXT("taught"), WoRGive::TeachSkillPaying(TestWorld->GetWorld(), Wallet, KeyOf(MENTOR_TEACHER), KeyOf(MENTOR_LEARNER), Ability).IsEmpty());
		TestTrue(TEXT("the learner knows it"), Pool->KnowsAbility(KeyOf(MENTOR_LEARNER), Ability));
		TestEqual(TEXT("the base price"), Wallet->GetMentorTokens(), MENTOR_ENOUGH_TOKENS - MentorTokenRules::TEACH_PRICE_AT_LOWEST_TIER);
	});

	It("refuses a teach between one player's own members (give it instead)", [this]()
	{
		if (!Ready()) return;
		TestTrue(TEXT("the teacher learned it"), Inventories[MENTOR_TEACHER]->LearnSpell(Spell));
		TestFalse(TEXT("refused"), WoRGive::TryTeachSkill(TestWorld->GetWorld(), KeyOf(MENTOR_TEACHER), KeyOf(MENTOR_LEARNER), Spell).IsEmpty());
		TestFalse(TEXT("nothing learned"), Pool->KnowsSpell(KeyOf(MENTOR_LEARNER), Spell));
	});

	It("keeps the tokens through a save round trip", [this]()
	{
		if (!Ready()) return;
		UCurrencyComponent *Wallet = WalletOf(MENTOR_TEACHER);
		if (!TestNotNull(TEXT("Wallet"), Wallet)) return;
		Wallet->Add(ECurrencyType::MentorTokens, MENTOR_ENOUGH_TOKENS);
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Wallet = USaveSubsystem::CaptureWallet(Wallet);
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("saved to memory"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return;
		UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;
		UCurrencyComponent *Fresh = WalletOf(MENTOR_LEARNER);
		if (!TestNotNull(TEXT("Fresh wallet"), Fresh)) return;
		USaveSubsystem::ApplyWallet(Loaded->Data.Wallet, Fresh);
		TestEqual(TEXT("the tokens are back"), Fresh->GetMentorTokens(), MENTOR_ENOUGH_TOKENS);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
