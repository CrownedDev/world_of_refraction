// MentorTokensSpec.cpp
// B-361b (D-154, Q-361b): Mentor Tokens are paid to every player only when two or more play together; teaching a
// learned skill to another player's character gives the learner its own copy, leaves the teacher's and spends the
// teacher's player's tokens by the skill's tier; too few tokens, or a teach between one player's own characters, is
// refused with nothing spent; the tokens survive a save.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/MentorTokens.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 MENTOR_HOST_ID = 1;
	constexpr int32 MENTOR_JOINER_ID = 2;
	constexpr int32 MENTOR_HOST_SLOT = 0;
	constexpr int32 MENTOR_GUEST_SLOT = 1;
	constexpr int32 MENTOR_SECOND_HOST_SLOT = 2;
	constexpr int32 MENTOR_PARTY_SIZE = 3;
	constexpr int32 MENTOR_STARTING_TOKENS = 10;
	constexpr int32 MENTOR_C_TIER_COST = MentorTokenRules::TEACH_COST_AT_F + 3 * MentorTokenRules::TEACH_COST_PER_TIER; // F, E, D, C

	int32 MentorSpellCopies(const UPoolSubsystem &Pool, const USpellData *Spell)
	{
		int32 Copies = 0;
		for (const TPair<FName, FPoolSkills> &Pair : Pool.GetCharacterSkills())
		{
			Copies += Pair.Value.Spells.FilterByPredicate([Spell](const FSpellInstance &I) { return I.Spell == Spell; }).Num();
		}
		return Copies;
	}
}

BEGIN_DEFINE_SPEC(FMentorTokensSpec, "WoR.Currency.MentorTokens", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	AWoRGameState *GameState = nullptr;
	UPoolSubsystem *Pool = nullptr;
	AWoRPlayerState *Host = nullptr;
	AWoRPlayerState *Joiner = nullptr;
	USpellData *Spell = nullptr;
	TArray<FName> Keys;
	TArray<UInventoryComponent *> Inventories;

	AWoRPlayerState *SpawnPlayer(int32 PlayerId)
	{
		UWorld *World = TestWorld->GetWorld();
		AWoRPlayerState *Player = World->SpawnActor<AWoRPlayerState>();
		APlayerController *PC = World->SpawnActor<APlayerController>();
		if (!Player || !PC)
		{
			return nullptr;
		}
		if (APlayerState *AutoState = PC->PlayerState; AutoState && AutoState != Player)
		{
			GameState->RemovePlayerState(AutoState);
			AutoState->Destroy();
		}
		Player->SetPlayerId(PlayerId);
		PC->PlayerState = Player;
		GameState->AddPlayerState(Player);
		return Player;
	}

	/** A party member as each level spawns it: an empty preset for its class, then the shared draw. */
	UInventoryComponent *SpawnMember(UCharacterData *Data)
	{
		AActor *Actor = TestWorld->SpawnCombatant(Data);
		if (!Actor)
		{
			return nullptr;
		}
		FWoRTestWorld::AddComponent<UCrystalInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UEvolutionInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<UCurrencyComponent>(Actor);
		UInventoryComponent *Inventory = FWoRTestWorld::AddComponent<UInventoryComponent>(Actor);
		FWoRTestWorld::AddComponent<ULoadoutComponent>(Actor);
		FCombatLoadout Preset;
		Preset.InitializeForClass(Data->CharacterClass);
		Inventory->SavedLoadouts = {Preset};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->DrawSharedGear();
		return Inventory;
	}

	bool Ready() const { return Pool && Host && Joiner && Spell && Keys.Num() == MENTOR_PARTY_SIZE && !Inventories.Contains(nullptr); }
	int32 Tokens(const AWoRPlayerState *Player) const { return Player->GetWallet()->GetBalance(ECurrencyType::MentorTokens); }
END_DEFINE_SPEC(FMentorTokensSpec)

void FMentorTokensSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		UWorld *World = TestWorld->GetWorld();
		GameState = World ? World->SpawnActor<AWoRGameState>() : nullptr;
		if (!TestNotNull(TEXT("GameState"), GameState)) return;
		World->SetGameState(GameState);
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Host = SpawnPlayer(MENTOR_HOST_ID);
		Joiner = SpawnPlayer(MENTOR_JOINER_ID);
		Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Mentor spell");
		Spell->Tier = EItemTier::C_Tier;
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !Host || !Joiner) return;
		Party->Members.Empty();
		TArray<UCharacterData *> Datas;
		for (int32 Slot = 0; Slot < MENTOR_PARTY_SIZE; ++Slot)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Mentor member %d"), Slot);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Member.bPlayerControlled = true;
			Member.MemberId = FGuid::NewGuid();
			(Slot == MENTOR_GUEST_SLOT ? Joiner : Host)->ClaimMember(Member.MemberId);
			Keys.Add(UPoolSubsystem::GetCharacterKey(Member));
			Datas.Add(Data);
		}
		for (UCharacterData *Data : Datas)
		{
			Inventories.Add(SpawnMember(Data));
		}
	});

	AfterEach([this]()
	{
		GameState = nullptr;
		Pool = nullptr;
		Host = Joiner = nullptr;
		Spell = nullptr;
		Keys.Empty();
		Inventories.Empty();
		TestWorld.Reset();
	});

	It("pays every player when two play together, and nobody in solo play", [this]()
	{
		if (!Ready()) return;
		UWorld *World = TestWorld->GetWorld();
		TestEqual(TEXT("Both players paid"), WoRMentorTokens::PayPlayersTogether(World, MentorTokenRules::PER_COOP_RUN, TEXT("spec run")), 2);
		TestEqual(TEXT("The host's token"), Tokens(Host), MentorTokenRules::PER_COOP_RUN);
		TestEqual(TEXT("The joiner's token"), Tokens(Joiner), MentorTokenRules::PER_COOP_RUN);

		GameState->RemovePlayerState(Joiner);
		TestEqual(TEXT("Solo pays nobody"), WoRMentorTokens::PayPlayersTogether(World, MentorTokenRules::PER_BOSS_TOGETHER, TEXT("spec boss")), 0);
		TestEqual(TEXT("The host's tokens unchanged"), Tokens(Host), MentorTokenRules::PER_COOP_RUN);
	});

	It("prices a teach by the skill's tier", [this]()
	{
		if (!Spell) return;
		TestEqual(TEXT("C tier"), WoRMentorTokens::GetTeachCost(Spell), MENTOR_C_TIER_COST);
		Spell->Tier = EItemTier::F_Tier;
		TestEqual(TEXT("F tier"), WoRMentorTokens::GetTeachCost(Spell), MentorTokenRules::TEACH_COST_AT_F);
		TestEqual(TEXT("Not a skill"), WoRMentorTokens::GetTeachCost(nullptr), 0);
	});

	It("teaches another player's character: the learner gets a copy, the teacher keeps its, the tokens are spent", [this]()
	{
		if (!Ready()) return;
		UWorld *World = TestWorld->GetWorld();
		TestTrue(TEXT("The host's character knows it"), Inventories[MENTOR_HOST_SLOT]->LearnSpell(Spell));
		Host->GetWallet()->Add(ECurrencyType::MentorTokens, MENTOR_STARTING_TOKENS);
		const FText Refusal = WoRGive::TryTeachSkill(World, Keys[MENTOR_HOST_SLOT], Keys[MENTOR_GUEST_SLOT], Spell);
		TestTrue(FString::Printf(TEXT("Taught (%s)"), *Refusal.ToString()), Refusal.IsEmpty());
		TestTrue(TEXT("The learner knows it"), Pool->KnowsSpell(Keys[MENTOR_GUEST_SLOT], Spell));
		TestTrue(TEXT("The teacher keeps it"), Pool->KnowsSpell(Keys[MENTOR_HOST_SLOT], Spell));
		TestEqual(TEXT("Two copies, one each"), MentorSpellCopies(*Pool, Spell), 2);
		TestEqual(TEXT("The tier's price spent"), Tokens(Host), MENTOR_STARTING_TOKENS - MENTOR_C_TIER_COST);
		TestEqual(TEXT("The learner's player paid nothing"), Tokens(Joiner), 0);
		TestTrue(TEXT("The learner's member loaded it"), Inventories[MENTOR_GUEST_SLOT]->Spells.HasSpell(Spell));
		TestFalse(TEXT("Teaching it again is refused"), WoRGive::TryTeachSkill(World, Keys[MENTOR_HOST_SLOT], Keys[MENTOR_GUEST_SLOT], Spell).IsEmpty());
	});

	It("refuses a teach with too few tokens, or between one player's own characters, and spends nothing", [this]()
	{
		if (!Ready()) return;
		UWorld *World = TestWorld->GetWorld();
		TestTrue(TEXT("The host's character knows it"), Inventories[MENTOR_HOST_SLOT]->LearnSpell(Spell));
		Host->GetWallet()->Add(ECurrencyType::MentorTokens, MENTOR_C_TIER_COST - 1);
		TestFalse(TEXT("Too few tokens"), WoRGive::TryTeachSkill(World, Keys[MENTOR_HOST_SLOT], Keys[MENTOR_GUEST_SLOT], Spell).IsEmpty());
		TestFalse(TEXT("Nothing learned"), Pool->KnowsSpell(Keys[MENTOR_GUEST_SLOT], Spell));
		TestEqual(TEXT("Nothing spent"), Tokens(Host), MENTOR_C_TIER_COST - 1);

		Host->GetWallet()->Add(ECurrencyType::MentorTokens, MENTOR_STARTING_TOKENS);
		TestFalse(TEXT("The same player's characters give instead"),
				  WoRGive::TryTeachSkill(World, Keys[MENTOR_HOST_SLOT], Keys[MENTOR_SECOND_HOST_SLOT], Spell).IsEmpty());
		TestFalse(TEXT("Nothing learned there"), Pool->KnowsSpell(Keys[MENTOR_SECOND_HOST_SLOT], Spell));
		TestEqual(TEXT("Nothing spent there"), Tokens(Host), MENTOR_C_TIER_COST - 1 + MENTOR_STARTING_TOKENS);
	});

	It("keeps the tokens through a save", [this]()
	{
		if (!Ready()) return;
		Host->GetWallet()->Add(ECurrencyType::MentorTokens, MENTOR_STARTING_TOKENS);
		const FSavedWallet Saved = USaveSubsystem::CaptureWallet(Host->GetWallet());
		FWoRTestWorld After;
		TestTrue(TEXT("Second world"), After.Create());
		UCurrencyComponent *Fresh = FWoRTestWorld::AddComponent<UCurrencyComponent>(After.GetWorld()->SpawnActor<AActor>());
		USaveSubsystem::ApplyWallet(Saved, Fresh);
		TestEqual(TEXT("The tokens are back"), Fresh->GetBalance(ECurrencyType::MentorTokens), MENTOR_STARTING_TOKENS);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
