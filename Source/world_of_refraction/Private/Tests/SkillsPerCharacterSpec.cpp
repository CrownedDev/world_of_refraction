// SkillsPerCharacterSpec.cpp
// B-316b (D-111, Q-316, Q-316b): learned skills belong to one character and are kept in the store. A learned spell
// survives a re-spawn (the next draw loads it); a given spell moves (one copy: the giver loses it); a request to an
// AI member is granted and moves the ability; a spell bought for another member is learned by the receiver even when
// the payer knows it; the skills survive a save round trip.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Currency/EconomyService.h"
#include "Equipment/Crystals/CrystalInventoryComponent.h"
#include "Equipment/Crystals/EvolutionInventoryComponent.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/GearGive.h"
#include "Inventory/InventoryComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Loadout/LoadoutComponent.h"
#include "Merchant/MerchantData.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
	constexpr int32 SKILLS_PARTY_SIZE = 2;
	constexpr int32 SKILLS_GIVER = 0; // player-controlled
	constexpr int32 SKILLS_RECEIVER = 1; // an AI member
	constexpr int32 SKILLS_WALLET = 100000;

	int32 CopiesOf(const UPoolSubsystem &Pool, const USpellData *Spell)
	{
		int32 Copies = 0;
		for (const TPair<FName, FPoolSkills> &Pair : Pool.GetCharacterSkills())
		{
			Copies += Pair.Value.Spells.FilterByPredicate([Spell](const FSpellInstance &I) { return I.Spell == Spell; }).Num();
		}
		return Copies;
	}
}

BEGIN_DEFINE_SPEC(FSkillsPerCharacterSpec, "WoR.Pool.Skills", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	USpellData *Spell = nullptr;
	UAbilityData *Ability = nullptr;
	TArray<UCharacterData *> Members;
	TArray<UInventoryComponent *> Inventories;

	FName KeyOf(int32 Index) const { return UPoolSubsystem::GetCharacterKey(Members[Index]); }
	bool Ready() const { return Pool && Spell && Ability && !Inventories.Contains(nullptr); }

	/** Party member Index as each level spawns it: the sword as its primary, then the draw. */
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
END_DEFINE_SPEC(FSkillsPerCharacterSpec)

void FSkillsPerCharacterSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Skills sword");
		Spell = NewObject<USpellData>(GetTransientPackage());
		Spell->Name = TEXT("Skills spell");
		Ability = NewObject<UAbilityData>(GetTransientPackage());
		Ability->Name = TEXT("Skills ability");
		UPartySessionSubsystem *PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Party->Members.Empty();
		for (int32 Index = 0; Index < SKILLS_PARTY_SIZE; ++Index)
		{
			UCharacterData *Data = FWoRTestWorld::NewCharacterData();
			Data->Name = FString::Printf(TEXT("Skills member %d"), Index);
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
			Member.bPlayerControlled = Index == SKILLS_GIVER;
			Members.Add(Data);
		}
		for (int32 Index = 0; Index < SKILLS_PARTY_SIZE; ++Index)
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

	It("banks each member's skills at the draw, and a learned spell survives a re-spawn", [this]()
	{
		if (!Ready()) return;
		TestTrue(TEXT("both members banked"), Pool->HasBankedSkills(KeyOf(SKILLS_GIVER)) && Pool->HasBankedSkills(KeyOf(SKILLS_RECEIVER)));
		TestTrue(TEXT("learned"), Inventories[SKILLS_GIVER]->LearnSpell(Spell));
		TestTrue(TEXT("the store knows it"), Pool->KnowsSpell(KeyOf(SKILLS_GIVER), Spell));
		Inventories[SKILLS_GIVER]->GetOwner()->Destroy();
		UInventoryComponent *Again = SpawnMember(SKILLS_GIVER);
		if (!TestNotNull(TEXT("re-spawned"), Again)) return;
		TestTrue(TEXT("the re-spawned member knows it"), Again->Spells.HasSpell(Spell));
		TestEqual(TEXT("one copy"), CopiesOf(*Pool, Spell), 1);
	});

	It("gives a spell: it moves (one copy), and giving it again is refused", [this]()
	{
		if (!Ready()) return;
		UWorld *World = TestWorld->GetWorld();
		TestTrue(TEXT("learned"), Inventories[SKILLS_GIVER]->LearnSpell(Spell));
		TestTrue(TEXT("giveable"), WoRGive::GetGiveableSkills(World, KeyOf(SKILLS_GIVER)).Contains(Spell));
		const FText Refusal = WoRGive::TryGiveSkill(World, KeyOf(SKILLS_GIVER), KeyOf(SKILLS_RECEIVER), Spell);
		TestTrue(FString::Printf(TEXT("given (%s)"), *Refusal.ToString()), Refusal.IsEmpty());
		TestFalse(TEXT("the giver lost it"), Inventories[SKILLS_GIVER]->Spells.HasSpell(Spell));
		TestTrue(TEXT("the receiver's member has it"), Inventories[SKILLS_RECEIVER]->Spells.HasSpell(Spell));
		TestEqual(TEXT("one copy"), CopiesOf(*Pool, Spell), 1);
		TestFalse(TEXT("the giver can't give it again"),
				  WoRGive::TryGiveSkill(World, KeyOf(SKILLS_GIVER), KeyOf(SKILLS_RECEIVER), Spell).IsEmpty());
		TestFalse(TEXT("nor give it to yourself"),
				  WoRGive::TryGiveSkill(World, KeyOf(SKILLS_RECEIVER), KeyOf(SKILLS_RECEIVER), Spell).IsEmpty());
	});

	It("grants a request to an AI member: the ability moves to the one who asked", [this]()
	{
		if (!Ready()) return;
		UWorld *World = TestWorld->GetWorld();
		TestTrue(TEXT("the AI member learned it"), Inventories[SKILLS_RECEIVER]->LearnAbility(Ability));
		const FText Refusal = WoRGive::TryRequestSkill(World, KeyOf(SKILLS_GIVER), KeyOf(SKILLS_RECEIVER), Ability);
		TestTrue(FString::Printf(TEXT("granted (%s)"), *Refusal.ToString()), Refusal.IsEmpty());
		TestTrue(TEXT("the asker has it"), Inventories[SKILLS_GIVER]->Abilities.HasAbility(Ability));
		TestFalse(TEXT("the AI member doesn't"), Inventories[SKILLS_RECEIVER]->Abilities.HasAbility(Ability));
		TestFalse(TEXT("asking again is refused"), WoRGive::TryRequestSkill(World, KeyOf(SKILLS_GIVER), KeyOf(SKILLS_RECEIVER), Ability).IsEmpty());
	});

	It("buys a spell for another member: the receiver learns it, even though the payer knows it", [this]()
	{
		if (!Ready()) return;
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		AActor *Payer = Inventories[SKILLS_GIVER]->GetOwner();
		UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Payer);
		if (!TestNotNull(TEXT("Economy"), Economy) || !TestNotNull(TEXT("Wallet"), Wallet)) return;
		for (const ECurrencyType Balance : {ECurrencyType::Prisms, ECurrencyType::SkillEssence, ECurrencyType::ElementalEssence})
		{
			Wallet->Add(Balance, SKILLS_WALLET);
		}
		TestTrue(TEXT("the payer knows it"), Inventories[SKILLS_GIVER]->LearnSpell(Spell));
		FMerchantStockEntry Entry;
		Entry.Asset = Spell;
		TestTrue(TEXT("bought for the receiver"), Economy->PurchaseFor(Payer, KeyOf(SKILLS_RECEIVER), {Entry}));
		TestTrue(TEXT("Prisms spent"), Wallet->GetBalance(ECurrencyType::Prisms) < SKILLS_WALLET);
		TestTrue(TEXT("the receiver knows it"), Pool->KnowsSpell(KeyOf(SKILLS_RECEIVER), Spell));
		TestTrue(TEXT("its member too"), Inventories[SKILLS_RECEIVER]->Spells.HasSpell(Spell));
		TestTrue(TEXT("the payer keeps its own"), Inventories[SKILLS_GIVER]->Spells.HasSpell(Spell));
		const int32 PrismsAfter = Wallet->GetBalance(ECurrencyType::Prisms);
		TestFalse(TEXT("buying it again for the receiver is refused"), Economy->PurchaseFor(Payer, KeyOf(SKILLS_RECEIVER), {Entry}));
		TestEqual(TEXT("nothing spent on the refusal"), Wallet->GetBalance(ECurrencyType::Prisms), PrismsAfter);
	});

	It("keeps each character's skills through a save round trip", [this]()
	{
		if (!Ready()) return;
		TestTrue(TEXT("learned a spell"), Inventories[SKILLS_GIVER]->LearnSpell(Spell));
		TestTrue(TEXT("learned an ability"), Inventories[SKILLS_RECEIVER]->LearnAbility(Ability));
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(*Pool);
		TArray<uint8> Bytes;
		if (!TestTrue(TEXT("saved to memory"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return;
		UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;

		FWoRTestWorld After;
		if (!TestTrue(TEXT("second world"), After.Create())) return;
		UPoolSubsystem *PoolAfter = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("PoolAfter"), PoolAfter)) return;
		USaveSubsystem::RestorePool(Loaded->Data.Pool, *PoolAfter);
		TestTrue(TEXT("the giver's spell is back"), PoolAfter->KnowsSpell(KeyOf(SKILLS_GIVER), Spell));
		TestTrue(TEXT("the receiver's ability is back"), PoolAfter->KnowsAbility(KeyOf(SKILLS_RECEIVER), Ability));
		TestFalse(TEXT("and not swapped"), PoolAfter->KnowsSpell(KeyOf(SKILLS_RECEIVER), Spell));
		TestTrue(TEXT("both still banked"), PoolAfter->HasBankedSkills(KeyOf(SKILLS_GIVER)) && PoolAfter->HasBankedSkills(KeyOf(SKILLS_RECEIVER)));
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
