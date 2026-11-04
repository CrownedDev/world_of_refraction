// TrialRecruitSpec.cpp
// B-076 (D-026, Q-076): a character found during a trial joins for that run only. It arrives
// ungeared (nothing of its own goes into the shared armoury), uses the armoury pieces it's given,
// leaves when the run ends, and a saved run keeps it.
// B-361 (D-152, D-153, Q-361, Q-361c): a recruit carries the host's pouch, banks and trades its skills like any
// member, and leaves its skills with the host (one copy); skills never change hands between two players' members.
// B-361c: a recruit and its skills survive a mid-run save; no item give inside the one pouch; gems bought for a
// recruit land in the host's pouch; a leaving recruit's skills with a full host or no host ledger.

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
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/GearGive.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Skills/Definitions/SpellData.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/InventoryConstants.h"
#include "Merchant/MerchantData.h"
#include "Loadout/LoadoutComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"

namespace
{
	constexpr int32 TRIAL_RECRUIT_SLOT = 1;
	constexpr int32 TRIAL_RECRUIT_PICKUP_COUNT = 2;
	constexpr int32 TRIAL_RECRUIT_HOST_ID = 1;
	constexpr int32 TRIAL_RECRUIT_JOINER_ID = 2;
	constexpr int32 TRIAL_RECRUIT_WALLET = 100000; // B-361c: enough Prisms and essence for any gem
	constexpr int32 TRIAL_RECRUIT_LEADER_SLOT = 0;
	constexpr int32 TRIAL_RECRUIT_PARTY_SIZE = 2;
	/** WoRGive::TryGiveCarried's refusal when both carry one pouch (GearGive.cpp, SharedPouch). */
	const TCHAR *const TRIAL_RECRUIT_SHARED_POUCH = TEXT("They carry the same pouch: a recruit uses the host's");
	const FCrystalId TRIAL_RECRUIT_GEM(ECrystalType::Garnet, EItemTier::F_Tier);

	int32 TrialRecruitSpellCopies(const UPoolSubsystem &Pool, const USpellData *Spell)
	{
		int32 Copies = 0;
		for (const TPair<FName, FPoolSkills> &Pair : Pool.GetCharacterSkills())
		{
			Copies += Pair.Value.Spells.FilterByPredicate([Spell](const FSpellInstance &I) { return I.Spell == Spell; }).Num();
		}
		return Copies;
	}
}

BEGIN_DEFINE_SPEC(FTrialRecruitSpec, "WoR.Party.TrialRecruit",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	UPartySessionSubsystem *PartySession = nullptr;
	UPoolSubsystem *Pool = nullptr;
	UWeaponData *Sword = nullptr;
	UCharacterData *Leader = nullptr;
	UCharacterData *Recruit = nullptr;

	/** Spawns a party member as each level does: its authored gear and a preset naming it, then the shared draw. */
	UInventoryComponent *Spawn(UCharacterData *Data, bool bBringsSword, USpellData *InnateSpell = nullptr)
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
		if (InnateSpell)
		{
			Preset.InnateSpells.Add(InnateSpell);
		}
		if (bBringsSword)
		{
			Inventory->AddWeapon(Sword);
			Preset.PrimarySlotType = EPrimarySlotType::Weapon;
			Preset.PrimaryWeapon.WeaponEntry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		}
		Inventory->SavedLoadouts = {Preset};
		Inventory->ActiveLoadoutIndex = 0;
		Inventory->LinkLoadoutsToOwned();
		Inventory->DrawSharedGear();
		return Inventory;
	}

	/** B-361: a player in this world, as the co-op spec spawns one (its own controller; the first is the host). */
	AWoRPlayerState *SpawnPlayer(AWoRGameState *GameState, int32 PlayerId)
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

	static FGuid ActiveWeaponID(const UInventoryComponent *Inventory)
	{
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex].PrimaryWeapon.WeaponEntry.PersistentID;
	}
END_DEFINE_SPEC(FTrialRecruitSpec)

void FTrialRecruitSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
		PartySession = TestWorld->GetSubsystem<UPartySessionSubsystem>();
		Pool = TestWorld->GetSubsystem<UPoolSubsystem>();
		UParty *Party = PartySession ? PartySession->CreateSoloParty(nullptr, TSoftClassPtr<ACombatCharacter>()) : nullptr;
		if (!TestNotNull(TEXT("Party"), Party) || !TestNotNull(TEXT("Pool"), Pool)) return;
		Leader = FWoRTestWorld::NewCharacterData();
		Leader->Name = TEXT("Leader");
		Recruit = FWoRTestWorld::NewCharacterData();
		Recruit->Name = TEXT("Recruit");
		Party->Members.Empty();
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Leader;
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		Sword->Name = TEXT("Fire Sword");
	});

	AfterEach([this]()
	{
		PartySession = nullptr;
		Pool = nullptr;
		Sword = nullptr;
		Leader = nullptr;
		Recruit = nullptr;
		TestWorld.Reset();
	});

	It("joins a recruit for this run only and lets it go when the run ends", [this]()
	{
		if (!Pool) return;
		TestTrue(TEXT("Invited"), PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit));
		const UParty *Party = PartySession->GetLocalParty();
		if (!TestEqual(TEXT("Two members"), Party->Members.Num(), 2)) return;
		TestTrue(TEXT("The recruit is run-only"), Party->Members[TRIAL_RECRUIT_SLOT].bRunOnly);
		TestFalse(TEXT("The leader isn't"), Party->Members[0].bRunOnly);
		TestFalse(TEXT("The leader's gear isn't a recruit's to set"), PartySession->SetRunMemberGear(0, FGuid::NewGuid(), FGuid()));

		TestEqual(TEXT("One leaves"), PartySession->DismissRunMembers(), 1);
		TestTrue(TEXT("Only the leader stays"), Party->Members.Num() == 1 && Party->Members[0].CharacterData == Leader);
		TestEqual(TEXT("Nobody else to dismiss"), PartySession->DismissRunMembers(), 0);
	});

	It("brings none of the recruit's own gear into the armoury", [this]()
	{
		if (!Pool) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, true);
		if (!Inventory) return;
		TestEqual(TEXT("No sword in the armoury"), Pool->GetOwnedWeapons().Num(), 0);
		TestFalse(TEXT("Nothing banked for the recruit"), Pool->HasBankedGear(UPoolSubsystem::GetCharacterKey(Recruit)));
		TestEqual(TEXT("Its inventory shows the (empty) armoury"), Inventory->Weapons.Num(), 0);
		TestFalse(TEXT("It holds no weapon"), ActiveWeaponID(Inventory).IsValid());
	});

	It("gives the recruit the armoury weapon it was handed", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *LeaderInventory = Spawn(Leader, true);
		if (!LeaderInventory) return;
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		LeaderInventory->SavedLoadouts[0] = FCombatLoadout();
		LeaderInventory->ClaimActiveSharedGear();
		TestFalse(TEXT("The leader put the sword down"), Pool->GetWearer(SwordID) == UPoolSubsystem::GetCharacterKey(Leader));

		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		TestTrue(TEXT("Gear set"), PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, SwordID, FGuid()));
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		TestEqual(TEXT("The recruit holds the sword"), ActiveWeaponID(Inventory), SwordID);
		TestEqual(TEXT("And wears it"), Pool->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Recruit));
	});

	It("keeps a Caster recruit's spells and puts its ring in the primary slot", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *LeaderInventory = Spawn(Leader, false);
		if (!LeaderInventory) return;
		LeaderInventory->AddRing(NewObject<URingData>(GetTransientPackage()));
		if (!TestEqual(TEXT("One ring in the armoury"), Pool->GetOwnedRings().Num(), 1)) return;
		const FGuid RingID = Pool->GetOwnedRings()[0].PersistentID;

		Recruit->CharacterClass = ECharacterClass::Caster;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, FGuid(), RingID);
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		UInventoryComponent *Inventory = Spawn(Recruit, false, Spell);
		if (!Inventory) return;
		const FCombatLoadout &Preset = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		TestTrue(TEXT("Its innate spell stays"), Preset.InnateSpells.Num() == 1 && Preset.InnateSpells[0] == Spell);
		TestTrue(TEXT("The ring is its primary"), Preset.HasPrimaryRing() && Preset.PrimaryRing.RingEntry.PersistentID == RingID);
		TestEqual(TEXT("It has its item slots"), Preset.ItemSlots.Num(), InventoryConstants::MAX_ITEM_LOADOUT_SLOTS);
	});

	It("puts a Resonator recruit's ring in its ring loadout", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *LeaderInventory = Spawn(Leader, true);
		if (!LeaderInventory) return;
		LeaderInventory->AddRing(NewObject<URingData>(GetTransientPackage()));
		const FGuid RingID = Pool->GetOwnedRings()[0].PersistentID;

		Recruit->CharacterClass = ECharacterClass::Resonator;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, FGuid(), RingID);
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		const FCombatLoadout &Preset = Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
		TestTrue(TEXT("The ring is in the ring loadout"), Preset.RingLoadout.Num() == 1 && Preset.RingLoadout[0].RingEntry.PersistentID == RingID);
		TestEqual(TEXT("And the recruit wears it"), Pool->GetWearer(RingID), UPoolSubsystem::GetCharacterKey(Recruit));
	});

	It("frees what a recruit wore when it leaves, and never lets one lead", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *LeaderInventory = Spawn(Leader, false);
		if (!LeaderInventory) return;
		LeaderInventory->AddRing(NewObject<URingData>(GetTransientPackage()));
		const FGuid RingID = Pool->GetOwnedRings()[0].PersistentID;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, FGuid(), RingID);
		if (!Spawn(Recruit, false)) return;
		TestEqual(TEXT("The recruit wears the ring"), Pool->GetWearer(RingID), UPoolSubsystem::GetCharacterKey(Recruit));

		PartySession->DismissRunMembers();
		TestTrue(TEXT("The ring is free again"), Pool->GetWearer(RingID).IsNone());

		PartySession->GetLocalParty()->Members.Empty();
		TestFalse(TEXT("No recruit joins a party with no leader"), PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit));
	});

	It("keeps a piece another member wears with that member", [this]()
	{
		if (!Pool) return;
		Spawn(Leader, true);
		const FGuid SwordID = Pool->GetOwnedWeapons()[0].PersistentID;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, SwordID, FGuid());
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		TestFalse(TEXT("The recruit's loadout lost the sword"), ActiveWeaponID(Inventory).IsValid());
		TestEqual(TEXT("The leader still wears it"), Pool->GetWearer(SwordID), UPoolSubsystem::GetCharacterKey(Leader));
		TestEqual(TEXT("The clash is noted"), Pool->GetClashes().Num(), 1);
	});

	It("saves a recruit only with the run, and brings it back with its gear", [this]()
	{
		if (!Pool) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		const FGuid SwordID = FGuid::NewGuid();
		PartySession->SetRunMemberGear(TRIAL_RECRUIT_SLOT, SwordID, FGuid());
		const UParty *Party = PartySession->GetLocalParty();

		TestEqual(TEXT("Outside a run, the recruit isn't saved"), USaveSubsystem::CaptureParty(Party, false).Members.Num(), 1);
		const FSavedParty Saved = USaveSubsystem::CaptureParty(Party, true);
		if (!TestEqual(TEXT("With the run, both are saved"), Saved.Members.Num(), 2)) return;
		// Restored in a fresh session, as after a restart: an existing party is never overwritten.
		FWoRTestWorld After;
		UPartySessionSubsystem *Resumed = After.Create() ? After.GetSubsystem<UPartySessionSubsystem>() : nullptr;
		if (!TestNotNull(TEXT("Party session after"), Resumed)) return;
		TestTrue(TEXT("Restored"), USaveSubsystem::RestoreParty(Saved, *Resumed));
		const UParty *Restored = Resumed->GetLocalParty();
		if (!TestNotNull(TEXT("Restored party"), Restored)) return;
		if (!TestEqual(TEXT("Two members back"), Restored->Members.Num(), 2)) return;
		const FPartyMember &Back = Restored->Members[TRIAL_RECRUIT_SLOT];
		TestTrue(TEXT("Still run-only"), Back.bRunOnly);
		TestEqual(TEXT("Same weapon"), Back.RunWeapon, SwordID);
	});

	It("puts a recruit's pickup in the host's pouch, and the recruit brings no items (B-361, Q-361c)", [this]()
	{
		if (!Pool || !Spawn(Leader, false)) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		UCrystalInventoryComponent *Pouch = Inventory ? Inventory->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("The recruit's crystal inventory"), Pouch)) return;
		const FName LeaderKey = UPoolSubsystem::GetCharacterKey(Leader);
		const FName RecruitKey = UPoolSubsystem::GetCharacterKey(Recruit);
		TestEqual(TEXT("Its pouch is the host's"), PartySession->GetPouchKey(RecruitKey), LeaderKey);
		TestTrue(TEXT("Picked up"), Pouch->AddCount(TRIAL_RECRUIT_GEM, TRIAL_RECRUIT_PICKUP_COUNT));
		TestEqual(TEXT("The host carries it"), Pool->GetCarriedCount(LeaderKey, TRIAL_RECRUIT_GEM), TRIAL_RECRUIT_PICKUP_COUNT);
		TestFalse(TEXT("No pouch of the recruit's own"), Pool->GetPouches().Contains(RecruitKey));
		TestEqual(TEXT("The recruit sees the same count"), Pouch->GetCount(TRIAL_RECRUIT_GEM), TRIAL_RECRUIT_PICKUP_COUNT);
	});

	It("lets a recruit learn a skill and give it to the host (B-361)", [this]()
	{
		if (!Pool || !Spawn(Leader, false)) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		const FName LeaderKey = UPoolSubsystem::GetCharacterKey(Leader);
		const FName RecruitKey = UPoolSubsystem::GetCharacterKey(Recruit);
		TestTrue(TEXT("The recruit's skills are kept"), Pool->HasBankedSkills(RecruitKey));
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		TestTrue(TEXT("The recruit learned it"), Inventory->LearnSpell(Spell) && Pool->KnowsSpell(RecruitKey, Spell));
		const FText Refusal = WoRGive::TryGiveSkill(TestWorld->GetWorld(), RecruitKey, LeaderKey, Spell);
		TestTrue(FString::Printf(TEXT("Given (%s)"), *Refusal.ToString()), Refusal.IsEmpty());
		TestTrue(TEXT("The host knows it"), Pool->KnowsSpell(LeaderKey, Spell));
		TestFalse(TEXT("The recruit doesn't"), Pool->KnowsSpell(RecruitKey, Spell));
	});

	It("leaves a recruit's skills with the host at run end, one copy each (B-361, Q-361, Q-361c)", [this]()
	{
		if (!Pool) return;
		UInventoryComponent *LeaderInventory = Spawn(Leader, false);
		if (!LeaderInventory) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		USpellData *Own = NewObject<USpellData>(GetTransientPackage());
		USpellData *Shared = NewObject<USpellData>(GetTransientPackage());
		TestTrue(TEXT("The recruit knows both"), Inventory->LearnSpell(Own) && Inventory->LearnSpell(Shared));
		TestTrue(TEXT("The host knows one of them"), LeaderInventory->LearnSpell(Shared));
		const FName LeaderKey = UPoolSubsystem::GetCharacterKey(Leader);
		const FName RecruitKey = UPoolSubsystem::GetCharacterKey(Recruit);

		TestEqual(TEXT("The recruit leaves"), PartySession->DismissRunMembers(), 1);
		TestTrue(TEXT("The host has the recruit's own spell"), Pool->KnowsSpell(LeaderKey, Own));
		TestEqual(TEXT("One copy of it"), TrialRecruitSpellCopies(*Pool, Own), 1);
		TestEqual(TEXT("Still one copy of the shared one"), TrialRecruitSpellCopies(*Pool, Shared), 1);
		TestFalse(TEXT("The recruit's skills are gone from the store"), Pool->HasBankedSkills(RecruitKey));
		TestTrue(TEXT("The host's member loaded it"), LeaderInventory->Spells.HasSpell(Own));
	});

	It("refuses a skill give between two players' members, but not a gear give (B-361, D-153)", [this]()
	{
		if (!Pool) return;
		UWorld *World = TestWorld->GetWorld();
		AWoRGameState *GameState = World->SpawnActor<AWoRGameState>();
		if (!TestNotNull(TEXT("GameState"), GameState)) return;
		World->SetGameState(GameState);
		AWoRPlayerState *Host = SpawnPlayer(GameState, TRIAL_RECRUIT_HOST_ID);
		AWoRPlayerState *Joiner = SpawnPlayer(GameState, TRIAL_RECRUIT_JOINER_ID);
		if (!TestNotNull(TEXT("Host"), Host) || !TestNotNull(TEXT("Joiner"), Joiner)) return;
		UCharacterData *Guest = FWoRTestWorld::NewCharacterData();
		Guest->Name = TEXT("Guest");
		FPartyMember &GuestMember = PartySession->GetLocalParty()->Members.AddDefaulted_GetRef();
		GuestMember.PawnClass = ACombatCharacter::StaticClass();
		GuestMember.CharacterData = Guest;
		GuestMember.bPlayerControlled = true;
		GuestMember.MemberId = FGuid::NewGuid();
		Joiner->ClaimMember(GuestMember.MemberId);
		const FName GuestKey = UPoolSubsystem::GetCharacterKey(GuestMember);
		UInventoryComponent *LeaderInventory = Spawn(Leader, false);
		UInventoryComponent *GuestInventory = Spawn(Guest, false);
		if (!LeaderInventory || !GuestInventory) return;
		const FName LeaderKey = UPoolSubsystem::GetCharacterKey(Leader);

		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		TestTrue(TEXT("The guest learned it"), GuestInventory->LearnSpell(Spell));
		TestFalse(TEXT("No give to the host's character"), WoRGive::TryGiveSkill(World, GuestKey, LeaderKey, Spell).IsEmpty());
		TestFalse(TEXT("No ask from it either"), WoRGive::TryRequestSkill(World, LeaderKey, GuestKey, Spell).IsEmpty());
		TestTrue(TEXT("The guest keeps it"), Pool->KnowsSpell(GuestKey, Spell) && !Pool->KnowsSpell(LeaderKey, Spell));

		GuestInventory->AddRing(NewObject<URingData>(GetTransientPackage()));
		if (!TestEqual(TEXT("One ring in the armoury"), Pool->GetOwnedRings().Num(), 1)) return;
		const FGuid RingID = Pool->GetOwnedRings()[0].PersistentID;
		if (!TestEqual(TEXT("The guest owns it"), Pool->GetGearOwner(RingID), GuestKey)) return;
		const FText Refusal = WoRGive::TryGiveGear(World, GuestKey, LeaderKey, RingID);
		TestTrue(FString::Printf(TEXT("Gear still goes (%s)"), *Refusal.ToString()), Refusal.IsEmpty());
		TestEqual(TEXT("The host's now, for good"), Pool->GetGearOwner(RingID), LeaderKey);
	});
	// ---- B-361c (B-361 review): the gaps its specs left ----

	It("keeps a recruit and its skills through a mid-run save capture and restore (B-361c; the byte round trip of skills is WoR.Pool.Skills)", [this]()
	{
		if (!Pool || !Spawn(Leader, false)) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		const FName RecruitKey = UPoolSubsystem::GetCharacterKey(Recruit);
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		if (!TestTrue(TEXT("The recruit learned it"), Inventory->LearnSpell(Spell))) return;
		const FSavedParty SavedParty = USaveSubsystem::CaptureParty(PartySession->GetLocalParty(), true);
		const FSavedPool SavedPool = USaveSubsystem::CapturePool(*Pool);

		FWoRTestWorld After;
		UPartySessionSubsystem *PartyAfter = After.Create() ? After.GetSubsystem<UPartySessionSubsystem>() : nullptr;
		UPoolSubsystem *PoolAfter = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Party session after"), PartyAfter) || !TestNotNull(TEXT("Pool after"), PoolAfter)) return;
		TestTrue(TEXT("Party restored"), USaveSubsystem::RestoreParty(SavedParty, *PartyAfter));
		USaveSubsystem::RestorePool(SavedPool, *PoolAfter);
		const UParty *Restored = PartyAfter->GetLocalParty();
		if (!TestNotNull(TEXT("Restored party"), Restored) || !TestEqual(TEXT("Two members back"), Restored->Members.Num(), TRIAL_RECRUIT_PARTY_SIZE)) return;
		TestTrue(TEXT("The recruit is back, run-only"), Restored->Members[TRIAL_RECRUIT_SLOT].bRunOnly);
		TestTrue(TEXT("Its skills are still kept"), PoolAfter->HasBankedSkills(RecruitKey));
		TestTrue(TEXT("And it still knows its spell"), PoolAfter->KnowsSpell(RecruitKey, Spell));
	});

	It("refuses an item give between a recruit and the host: one pouch (B-361c)", [this]()
	{
		if (!Pool || !Spawn(Leader, false)) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		UCrystalInventoryComponent *Pouch = Inventory ? Inventory->GetOwner()->FindComponentByClass<UCrystalInventoryComponent>() : nullptr;
		if (!TestNotNull(TEXT("The recruit's crystal inventory"), Pouch)) return;
		const FName LeaderKey = UPoolSubsystem::GetCharacterKey(Leader);
		const FName RecruitKey = UPoolSubsystem::GetCharacterKey(Recruit);
		TestTrue(TEXT("Picked up"), Pouch->AddCount(TRIAL_RECRUIT_GEM, TRIAL_RECRUIT_PICKUP_COUNT));
		UWorld *World = TestWorld->GetWorld();
		TestEqual(TEXT("Recruit to host refused: one pouch"),
				  WoRGive::TryGiveCarried(World, RecruitKey, LeaderKey, TRIAL_RECRUIT_GEM, 1).ToString(), FString(TRIAL_RECRUIT_SHARED_POUCH));
		TestEqual(TEXT("Host to recruit refused: one pouch"),
				  WoRGive::TryGiveCarried(World, LeaderKey, RecruitKey, TRIAL_RECRUIT_GEM, 1).ToString(), FString(TRIAL_RECRUIT_SHARED_POUCH));
		TestEqual(TEXT("The host still carries them all"), Pool->GetCarriedCount(LeaderKey, TRIAL_RECRUIT_GEM), TRIAL_RECRUIT_PICKUP_COUNT);
		TestFalse(TEXT("No pouch of the recruit's own"), Pool->GetPouches().Contains(RecruitKey));
	});

	It("puts gems bought for a recruit in the host's pouch, and the host paying moves nothing (B-361c)", [this]()
	{
		UInventoryComponent *LeaderInventory = Pool ? Spawn(Leader, false) : nullptr;
		if (!LeaderInventory) return;
		PartySession->GetLocalParty()->Members[TRIAL_RECRUIT_LEADER_SLOT].bPlayerControlled = true;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *RecruitInventory = Spawn(Recruit, false);
		if (!RecruitInventory) return;
		UEconomyService *Economy = TestWorld->GetSubsystem<UEconomyService>();
		AActor *Host = LeaderInventory->GetOwner();
		UCurrencyComponent *Wallet = CurrencyWallet::FindFor(Host);
		if (!TestNotNull(TEXT("Economy"), Economy) || !TestNotNull(TEXT("Host wallet"), Wallet)) return;
		Wallet->Add(ECurrencyType::Prisms, TRIAL_RECRUIT_WALLET);
		Wallet->Add(ECurrencyType::ElementalEssence, TRIAL_RECRUIT_WALLET);
		FMerchantStockEntry Gems;
		Gems.Crystal = TRIAL_RECRUIT_GEM;
		Gems.Count = TRIAL_RECRUIT_PICKUP_COUNT;
		const FName LeaderKey = UPoolSubsystem::GetCharacterKey(Leader);
		const FName RecruitKey = UPoolSubsystem::GetCharacterKey(Recruit);

		TestTrue(TEXT("Bought for the recruit"), Economy->PurchaseFor(Host, RecruitKey, {Gems}));
		TestTrue(TEXT("Paid from the host's wallet"), Wallet->GetBalance(ECurrencyType::Prisms) < TRIAL_RECRUIT_WALLET);
		TestEqual(TEXT("The gems are in the host's pouch, once"), Pool->GetCarriedCount(LeaderKey, TRIAL_RECRUIT_GEM), TRIAL_RECRUIT_PICKUP_COUNT);
		TestFalse(TEXT("No pouch of the recruit's own"), Pool->GetPouches().Contains(RecruitKey));
		const UCurrencyComponent *RecruitWallet = CurrencyWallet::FindFor(RecruitInventory->GetOwner());
		TestTrue(TEXT("The recruit has a wallet of its own"), RecruitWallet && RecruitWallet != Wallet);
		TestEqual(TEXT("And paid nothing"), RecruitWallet ? RecruitWallet->GetBalance(ECurrencyType::Prisms) : INDEX_NONE, 0);
	});

	It("drops a recruit's skill the host has no room for, and keeps its skills when the host has none kept (B-361c)", [this]()
	{
		if (!Pool) return;
		PartySession->InviteRunMember(ACombatCharacter::StaticClass(), Recruit);
		UInventoryComponent *Inventory = Spawn(Recruit, false);
		if (!Inventory) return;
		const FName LeaderKey = UPoolSubsystem::GetCharacterKey(Leader);
		const FName RecruitKey = UPoolSubsystem::GetCharacterKey(Recruit);
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		if (!TestTrue(TEXT("The recruit learned it"), Inventory->LearnSpell(Spell))) return;

		TestFalse(TEXT("The host has no skills kept yet"), Pool->HasBankedSkills(LeaderKey));
		TestEqual(TEXT("Nothing handed to a host with no ledger"), Pool->HandOverSkills(RecruitKey, LeaderKey), 0);
		TestTrue(TEXT("The recruit keeps its spell"), Pool->KnowsSpell(RecruitKey, Spell));

		if (!Spawn(Leader, false) || !TestTrue(TEXT("The host's skills are kept now"), Pool->HasBankedSkills(LeaderKey))) return;
		TArray<FSpellInstance> Full;
		for (int32 Index = 0; Index < InventoryConstants::MAX_LEARNED_SPELLS; ++Index)
		{
			Full.Emplace(NewObject<USpellData>(GetTransientPackage()));
		}
		Pool->SetSkills(LeaderKey, Full, {});
		TestEqual(TEXT("Nothing fits a full host"), Pool->HandOverSkills(RecruitKey, LeaderKey), 0);
		TestFalse(TEXT("The recruit's ledger is gone"), Pool->HasBankedSkills(RecruitKey));
		TestFalse(TEXT("Its spell is dropped, not squeezed in"), Pool->KnowsSpell(LeaderKey, Spell));
		TestEqual(TEXT("The host's list is unchanged"), Pool->FindSkills(LeaderKey)->Spells.Num(), InventoryConstants::MAX_LEARNED_SPELLS);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
