// SaveSpec.cpp
// B-090: the save keystone. The party (members, formation, control, carried HP/EP), the banked
// wallet and the pool's owned items survive capture -> save bytes -> load -> restore; Gold is
// never banked; a save below the minimum version is wiped (D-060). Headless: the slot is written
// to memory, never to disk (a test world has no local player).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CombatCharacter.h"
#include "Currency/CurrencyComponent.h"
#include "Player/WoRGameState.h"
#include "Player/WoRPlayerState.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Rings/RingData.h"
#include "Loadout/FSavedLoadout.h"
#include "Loadout/WornGear.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveGame.h"

namespace
{
	constexpr int32 SAVED_HP = 37;
	constexpr int32 SAVED_EP = 4;
	constexpr int32 SAVED_COLUMN = 2;
	constexpr int32 SAVED_HEAD_START_MIND = 2;
	constexpr int32 SAVED_HEAD_START_SPIRIT = 1;
	constexpr float SAVED_RUN_STRAIN = 42.0f;
	constexpr int32 PRISMS = 5;
	constexpr int32 ELEMENTAL_ESSENCE = 3;
	constexpr int32 PILLAR_ESSENCE = 4;
	/** B-333: the save version that brought the seven balances; anything older is wiped (D-060). */
	constexpr int32 SEVEN_BALANCES_SAVE_VERSION = 11;
	constexpr int32 GOLD = 7;
	constexpr int32 CRYSTAL_COUNT = 2;
	constexpr int32 DIAMONDS = 6;
	constexpr int32 GEAR_ESSENCE = 8;
	constexpr int32 SKILL_ESSENCE = 9;
	constexpr int32 EVOLUTION_DURABILITY = 11;
	/** B-348 (D-138): one below the current version is already too old. */
	constexpr int32 ONE_VERSION_BACK = 1;

	/** Save bytes and load them back, as the slot on disk would. */
	UWoRSaveGame *RoundTrip(UWoRSaveGame *Save)
	{
		TArray<uint8> Bytes;
		if (!UGameplayStatics::SaveGameToMemory(Save, Bytes))
		{
			return nullptr;
		}
		UWoRSaveGame *Loaded = Cast<UWoRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (Loaded)
		{
			Loaded->InitializeSaveGame(nullptr, USaveSubsystem::SLOT_NAME, true);
		}
		return Loaded;
	}
}

BEGIN_DEFINE_SPEC(FSaveSpec, "WoR.Save",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	bool CreateWorlds(FWoRTestWorld &Before, FWoRTestWorld &After)
	{
		return TestTrue(TEXT("Worlds"), Before.Create() && After.Create());
	}
END_DEFINE_SPEC(FSaveSpec)

void FSaveSpec::Define()
{
	It("brings the party back with its formation, control and carried HP/EP", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		UParty *Party = NewObject<UParty>(GetTransientPackage());
		Party->DisplayName = FText::FromString(TEXT("Saved Party"));
		Party->bLetAIFinish = true;
		UCharacterData *Hero = FWoRTestWorld::NewCharacterData();
		UCharacterData *Ally = FWoRTestWorld::NewCharacterData();
		for (UCharacterData *Data : {Hero, Ally})
		{
			FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
			Member.PawnClass = ACombatCharacter::StaticClass();
			Member.CharacterData = Data;
		}
		Party->Members[1].CurrentHP = SAVED_HP;
		Party->Members[1].CurrentEP = SAVED_EP;
		Party->Members[1].PreferredRow = ECombatRow::Back;
		Party->Members[1].PreferredColumn = SAVED_COLUMN;
		Party->Members[1].bPlayerControlled = false;
		Party->Members[1].bAISeat = true; // B-346: the saved mirror of bPlayerControlled
		Party->Members[1].AISeatDifficulty = EAIDifficulty::Hard;
		Party->Members[1].HeadStartMind = SAVED_HEAD_START_MIND; // B-348b
		Party->Members[1].HeadStartSpirit = SAVED_HEAD_START_SPIRIT;
		Party->Members[1].bBrokenDarkness = true; // B-348c
		Party->Members[1].RunStrain = SAVED_RUN_STRAIN;
		const FGuid SavedGuardWeapon = FGuid::NewGuid(); // B-392
		Party->Members[1].GuardStyle = EGuardStyle::Unarmed;
		Party->Members[1].GuardWeaponId = SavedGuardWeapon;
		Party->Members[1].ParryElementPriority = EParryElementPriority::Magic; // B-393
		// B-507 (D-221): the field skill is saved on the worn-gear record now (the case below).

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Party = USaveSubsystem::CaptureParty(Party, true);
		TestEqual(TEXT("Without a saved run, no carried HP"),
				  USaveSubsystem::CaptureParty(Party, false).Members[1].CurrentHP, PartyConstants::NO_CARRIED_VITALS);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		UPartySessionSubsystem *PartySession = After.GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("Loaded"), Loaded) || !TestNotNull(TEXT("Party session"), PartySession))
		{
			return;
		}
		TestTrue(TEXT("Restored"), USaveSubsystem::RestoreParty(Loaded->Data.Party, *PartySession));

		const UParty *Restored = PartySession->GetLocalParty();
		if (!TestNotNull(TEXT("Local party"), Restored) || !TestEqual(TEXT("Members"), Restored->Members.Num(), 2))
		{
			return;
		}
		const FPartyMember &Back = Restored->Members[1];
		TestEqual(TEXT("Name"), Restored->DisplayName.ToString(), FString(TEXT("Saved Party")));
		TestTrue(TEXT("Let AI finish"), Restored->bLetAIFinish);
		TestTrue(TEXT("Same characters"), Restored->Members[0].CharacterData == Hero && Back.CharacterData == Ally);
		TestEqual(TEXT("HP"), Back.CurrentHP, SAVED_HP);
		TestEqual(TEXT("EP"), Back.CurrentEP, SAVED_EP);
		TestTrue(TEXT("Cell"), Back.PreferredRow == ECombatRow::Back && Back.PreferredColumn == SAVED_COLUMN);
		TestFalse(TEXT("AI-controlled"), Back.bPlayerControlled);
		TestTrue(TEXT("AI seat, Hard (B-346 / B-348)"), Back.bAISeat && Back.AISeatDifficulty == EAIDifficulty::Hard);
		TestTrue(TEXT("Head-start survives save / load (B-348b)"), Back.HeadStartMind == SAVED_HEAD_START_MIND && Back.HeadStartBody == 0 &&
																		Back.HeadStartSpirit == SAVED_HEAD_START_SPIRIT);
		TestTrue(TEXT("Broken Darkness survives save / load (B-348c)"), Back.bBrokenDarkness);
		TestTrue(TEXT("The guard and its put-away weapon survive save / load (B-392)"),
				 Back.GuardStyle == EGuardStyle::Unarmed && Back.GuardWeaponId == SavedGuardWeapon);
		TestTrue(TEXT("Member 0 keeps the weapon guard"), Restored->Members[0].GuardStyle == EGuardStyle::Weapon);
		TestTrue(TEXT("The parry colour priority survives save / load (B-393)"),
				 Back.ParryElementPriority == EParryElementPriority::Magic &&
					 Restored->Members[0].ParryElementPriority == EParryElementPriority::Weapon);
		TestEqual(TEXT("The run's strain goes with a saved run"), Back.RunStrain, SAVED_RUN_STRAIN);
		TestEqual(TEXT("... and not without one"), USaveSubsystem::CaptureParty(Party, false).Members[1].RunStrain, 0.0f);
		TestTrue(TEXT("... while Broken Darkness is kept outside the run too"), USaveSubsystem::CaptureParty(Party, false).Members[1].bBrokenDarkness);
		TestFalse(TEXT("Member 0 never broke"), Loaded->Data.Party.Members.Num() > 0 && Loaded->Data.Party.Members[0].bBrokenDarkness);
		TestFalse(TEXT("A party that already exists is left alone"),
				  USaveSubsystem::RestoreParty(Loaded->Data.Party, *PartySession));
	});

	It("brings each character's worn gear back, and hands its wheel pick to the restored party (B-507, D-221)", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		UCharacterData *Hero = FWoRTestWorld::NewCharacterData();
		const FName Key = UPoolSubsystem::GetCharacterKey(Hero);
		FWornGear Worn;
		Worn.Equipment.SetNum(WornGearLayout::FIRST_ADAPTER_INDEX + 1); // a primary set, no secondary, one adapter
		Worn.Equipment[WornGearLayout::PRIMARY_INDEX].GearID = FGuid::NewGuid();
		Worn.Equipment[WornGearLayout::PRIMARY_INDEX].OffHandID = FGuid::NewGuid();
		Worn.Equipment[WornGearLayout::FIRST_ADAPTER_INDEX].GearID = FGuid::NewGuid();
		// D-224 (B-517): each entry's picks, slot by slot, an empty slot kept.
		Worn.Equipment[WornGearLayout::PRIMARY_INDEX].AssignedAbilities = {nullptr, NewObject<UAbilityData>(GetTransientPackage())};
		Worn.Equipment[WornGearLayout::FIRST_ADAPTER_INDEX].SpellOverrides = {NewObject<USpellData>(GetTransientPackage())};
		Worn.ActiveGearID = Worn.Equipment[WornGearLayout::PRIMARY_INDEX].GearID;
		Worn.Apparel = {FGuid::NewGuid()};
		UPoolSubsystem *BeforePool = Before.GetSubsystem<UPoolSubsystem>();
		UPoolSubsystem *AfterPool = After.GetSubsystem<UPoolSubsystem>();
		UPartySessionSubsystem *PartySession = After.GetSubsystem<UPartySessionSubsystem>();
		if (!TestNotNull(TEXT("Pools"), BeforePool) || !TestNotNull(TEXT("After pool"), AfterPool) || !TestNotNull(TEXT("Party session"), PartySession))
		{
			return;
		}
		BeforePool->SetWornGear(Key, Worn);
		FSavedLoadout Custom; // B-517: a custom loadout (a preset) keeps its entries' picks too
		Custom.LoadoutName = TEXT("Preset");
		Custom.EntryPicks = Worn.Equipment;
		BeforePool->SetCustomLoadouts(Key, {Custom});
		FWheelPick Pick;
		Pick.FieldSkill.Source = EFieldSkillSource::Ring; // B-214a's field skill, moved onto the record
		Pick.FieldSkill.Type = EFieldMovementType::EnhancedSprint;
		Pick.FieldAttackIndex = SAVED_COLUMN;
		Pick.bPicked = true;
		BeforePool->SetWheelPick(Key, Pick);

		UParty *Party = NewObject<UParty>(GetTransientPackage());
		FPartyMember &Member = Party->Members.AddDefaulted_GetRef();
		Member.PawnClass = ACombatCharacter::StaticClass();
		Member.CharacterData = Hero;
		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Party = USaveSubsystem::CaptureParty(Party, false);
		Save->Data.Pool = USaveSubsystem::CapturePool(*BeforePool);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded) || !TestTrue(TEXT("Party restored"), USaveSubsystem::RestoreParty(Loaded->Data.Party, *PartySession)))
		{
			return;
		}
		USaveSubsystem::RestorePool(Loaded->Data.Pool, *AfterPool); // party first, as LoadForPlayer

		const FWornGear *Back = AfterPool->GetWornGear(Key);
		if (!TestNotNull(TEXT("The record is back"), Back))
		{
			return;
		}
		Worn.WheelPick = Pick;
		TestTrue(TEXT("same pieces, places, picks, ACTIVE entry and wheel pick"), Back->SameAs(Worn));
		const TArray<FSavedLoadout> *Presets = AfterPool->GetCustomLoadouts(Key);
		const TArray<FLoadoutSlot> *PresetPicks = Presets && Presets->Num() == 1 ? &(*Presets)[0].EntryPicks : nullptr;
		using namespace WornGearLayout;
		TestTrue(TEXT("the custom loadout's entry picks are back (D-224)"),
				 PresetPicks && PresetPicks->Num() == Worn.Equipment.Num() &&
					 (*PresetPicks)[PRIMARY_INDEX].AssignedAbilities == Worn.Equipment[PRIMARY_INDEX].AssignedAbilities &&
					 (*PresetPicks)[FIRST_ADAPTER_INDEX].SpellOverrides == Worn.Equipment[FIRST_ADAPTER_INDEX].SpellOverrides);
		const FPartyMember *Restored = PartySession->GetMemberCarriedState(0);
		TestTrue(TEXT("the member took the record's field skill and pick"),
				 Restored && Restored->FieldSkill == Pick.FieldSkill && Restored->FieldAttackIndex == SAVED_COLUMN && Restored->bFieldAttackPicked);
		TestTrue(TEXT("and its weapon in hand"), Restored && Restored->ActiveWeaponId == Worn.ActiveGearID);
	});

	It("keeps a custom loadout's weapons and adapters through the slots alone, in their places (B-309c)", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		UPoolSubsystem *BeforePool = Before.GetSubsystem<UPoolSubsystem>();
		UPoolSubsystem *AfterPool = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pools"), BeforePool) || !TestNotNull(TEXT("After pool"), AfterPool))
		{
			return;
		}
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *OffHand = NewObject<UWeaponData>(GetTransientPackage());
		UWeaponData *Bow = NewObject<UWeaponData>(GetTransientPackage());
		URingData *Adapter = NewObject<URingData>(GetTransientPackage());
		URingData *Second = NewObject<URingData>(GetTransientPackage());
		UEvolutionItemData *Evolution = NewObject<UEvolutionItemData>(GetTransientPackage());
		UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
		const auto MakeRing = [](URingData *Ring)
		{
			FResonatorRingSlot Slot;
			Slot.Ring = Ring;
			Slot.RingInstance = FGuid::NewGuid();
			return Slot;
		};
		FSavedLoadout Sets; // a Generic's two weapon sets, the bow in hand, and its one adapter (B-480)
		Sets.LoadoutName = TEXT("Sets");
		Sets.bShowPrimary = false;
		Sets.PrimarySlotType = EPrimarySlotType::Weapon;
		Sets.PrimaryWeapon = Sword;
		Sets.PrimaryWeaponInstance = FGuid::NewGuid();
		Sets.PrimaryOffHandWeapon = OffHand;
		Sets.PrimaryOffHandInstance = FGuid::NewGuid();
		Sets.PrimaryWeaponAbilities = {Ability};
		Sets.SecondarySlotType = ESecondarySlotType::Weapon;
		Sets.SecondaryWeapon = Bow;
		Sets.SecondaryWeaponInstance = FGuid::NewGuid();
		Sets.SecondaryOffHandWeapon = OffHand;
		Sets.SecondaryOffHandInstance = FGuid::NewGuid();
		Sets.SecondaryWeaponAbilities = {Ability};
		Sets.EquippedRings = {MakeRing(Adapter)};
		FSavedLoadout Ringed; // an adapter primary
		Ringed.RequiredClass = ECharacterClass::Caster;
		Ringed.PrimarySlotType = EPrimarySlotType::Ring;
		Ringed.PrimaryRing = Adapter;
		Ringed.PrimaryRingInstance = FGuid::NewGuid();
		FSavedLoadout Evolved; // an evolution primary beside a Resonator's adapter loadout
		Evolved.RequiredClass = ECharacterClass::Resonator;
		Evolved.PrimarySlotType = EPrimarySlotType::Evolution;
		Evolved.PrimaryEvolution = Evolution;
		Evolved.PrimaryEvolutionInstance = FGuid::NewGuid();
		Evolved.EquippedRings = {MakeRing(Adapter), MakeRing(Second)};
		const FName Key = UPoolSubsystem::GetCharacterKey(FWoRTestWorld::NewCharacterData());
		BeforePool->SetCustomLoadouts(Key, {Sets, Ringed, Evolved});

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(*BeforePool);
		const TArray<FSavedCustomLoadout> *Written = Save->Data.Pool.CustomLoadouts.Num() == 1 ? &Save->Data.Pool.CustomLoadouts[0].Loadouts : nullptr;
		using namespace WornGearLayout;
		TestTrue(TEXT("the sets' slots: primary, secondary, then the adapter, each by GUID"),
				 Written && Written->Num() == 3 && (*Written)[0].Slots.Num() == FIRST_ADAPTER_INDEX + 1 &&
					 (*Written)[0].Slots[PRIMARY_INDEX].GearID == Sets.PrimaryWeaponInstance &&
					 (*Written)[0].Slots[PRIMARY_INDEX].OffHandID == Sets.PrimaryOffHandInstance &&
					 (*Written)[0].Slots[SECONDARY_INDEX].GearID == Sets.SecondaryWeaponInstance &&
					 (*Written)[0].Slots[FIRST_ADAPTER_INDEX].GearID == Sets.EquippedRings[0].RingInstance);
		TestTrue(TEXT("an evolution primary leaves the primary place empty"),
				 Written && Written->Num() == 3 && (*Written)[2].Slots.Num() == FIRST_ADAPTER_INDEX + 2 &&
					 !(*Written)[2].Slots[PRIMARY_INDEX].Gear.IsSet() && (*Written)[2].EvolutionInstance == Evolved.PrimaryEvolutionInstance);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded))
		{
			return;
		}
		USaveSubsystem::RestorePool(Loaded->Data.Pool, *AfterPool);
		const TArray<FSavedLoadout> *Back = AfterPool->GetCustomLoadouts(Key);
		if (!TestTrue(TEXT("three loadouts back"), Back && Back->Num() == 3))
		{
			return;
		}
		const FSavedLoadout &SetsBack = (*Back)[0];
		TestTrue(TEXT("the primary set rebuilt: sword, off hand, GUIDs, its ability list"),
				 SetsBack.PrimarySlotType == EPrimarySlotType::Weapon && SetsBack.PrimaryWeapon == Sword &&
					 SetsBack.PrimaryWeaponInstance == Sets.PrimaryWeaponInstance && SetsBack.PrimaryOffHandWeapon == OffHand &&
					 SetsBack.PrimaryOffHandInstance == Sets.PrimaryOffHandInstance && SetsBack.PrimaryWeaponAbilities == Sets.PrimaryWeaponAbilities);
		TestTrue(TEXT("the secondary set, in hand: bow, off hand, GUIDs, its ability list"),
				 SetsBack.SecondarySlotType == ESecondarySlotType::Weapon && SetsBack.SecondaryWeapon == Bow &&
					 SetsBack.SecondaryWeaponInstance == Sets.SecondaryWeaponInstance && !SetsBack.bShowPrimary &&
					 SetsBack.SecondaryOffHandWeapon == OffHand && SetsBack.SecondaryOffHandInstance == Sets.SecondaryOffHandInstance &&
					 SetsBack.SecondaryWeaponAbilities == Sets.SecondaryWeaponAbilities);
		TestTrue(TEXT("the adapter"), SetsBack.EquippedRings.Num() == 1 && SetsBack.EquippedRings[0].Ring == Adapter &&
										  SetsBack.EquippedRings[0].RingInstance == Sets.EquippedRings[0].RingInstance);
		TestTrue(TEXT("an adapter primary"), (*Back)[1].PrimarySlotType == EPrimarySlotType::Ring && (*Back)[1].PrimaryRing == Adapter &&
												 (*Back)[1].PrimaryRingInstance == Ringed.PrimaryRingInstance && !(*Back)[1].PrimaryWeapon &&
												 (*Back)[1].SecondarySlotType == ESecondarySlotType::None);
		const FSavedLoadout &EvolvedBack = (*Back)[2];
		TestTrue(TEXT("an evolution primary and two adapters in order"),
				 EvolvedBack.PrimarySlotType == EPrimarySlotType::Evolution && EvolvedBack.PrimaryEvolution == Evolution &&
					 EvolvedBack.PrimaryEvolutionInstance == Evolved.PrimaryEvolutionInstance && EvolvedBack.EquippedRings.Num() == 2 &&
					 EvolvedBack.EquippedRings[0].Ring == Adapter && EvolvedBack.EquippedRings[1].Ring == Second &&
					 EvolvedBack.EquippedRings[1].RingInstance == Evolved.EquippedRings[1].RingInstance);
	});

	It("banks every persistent balance but never Gold", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		AActor *Holder = Before.GetWorld()->SpawnActor<AActor>();
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(Holder);
		Wallet->Add(ECurrencyType::Prisms, PRISMS);
		Wallet->Add(ECurrencyType::ElementalEssence, ELEMENTAL_ESSENCE);
		Wallet->Add(ECurrencyType::PillarEssence, PILLAR_ESSENCE);
		Wallet->Add(ECurrencyType::Gold, GOLD);

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Wallet = USaveSubsystem::CaptureWallet(Wallet);
		TestEqual(TEXT("Three balances banked"), Save->Data.Wallet.Balances.Num(), 3);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded))
		{
			return;
		}
		UCurrencyComponent *Fresh = FWoRTestWorld::AddComponent<UCurrencyComponent>(After.GetWorld()->SpawnActor<AActor>());
		USaveSubsystem::ApplyWallet(Loaded->Data.Wallet, Fresh);
		USaveSubsystem::ApplyWallet(Loaded->Data.Wallet, Fresh);
		TestEqual(TEXT("Prisms (applying twice adds nothing)"), Fresh->GetBalance(ECurrencyType::Prisms), PRISMS);
		TestEqual(TEXT("Elemental Essence"), Fresh->GetElementalEssence(), ELEMENTAL_ESSENCE);
		TestEqual(TEXT("Pillar Essence"), Fresh->GetPillarEssence(), PILLAR_ESSENCE);
		TestEqual(TEXT("Gold"), Fresh->GetBalance(ECurrencyType::Gold), 0);
	});

	It("carries the run's Gold across a level change but clears it when a run starts or ends", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create()))
		{
			return;
		}
		USaveSubsystem *Save = World.GetSubsystem<USaveSubsystem>();
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		if (!TestNotNull(TEXT("Save subsystem"), Save))
		{
			return;
		}
		Wallet->Add(ECurrencyType::Gold, GOLD);
		Save->BankWallet(Wallet);
		UCurrencyComponent *NextLevel = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		Save->ApplyWalletTo(NextLevel);
		TestEqual(TEXT("Gold in the next level"), NextLevel->GetBalance(ECurrencyType::Gold), GOLD);
		TestFalse(TEXT("Gold is not written to the slot"),
				  Save->GetData().Wallet.Balances.ContainsByPredicate([](const FSavedBalance &B)
				  { return B.Currency == static_cast<uint8>(ECurrencyType::Gold); }));

		Save->ClearRunGold(World.GetWorld());
		UCurrencyComponent *AfterRun = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		Save->ApplyWalletTo(AfterRun);
		TestEqual(TEXT("No Gold after the run"), AfterRun->GetBalance(ECurrencyType::Gold), 0);
	});

	It("clears every player's run Gold when a run starts or ends, not only player 0's (B-426)", [this]()
	{
		FWoRTestWorld World;
		if (!TestTrue(TEXT("World"), World.Create()))
		{
			return;
		}
		UWorld *Level = World.GetWorld();
		USaveSubsystem *Save = World.GetSubsystem<USaveSubsystem>();
		AWoRGameState *GameState = Level->SpawnActor<AWoRGameState>();
		AWoRPlayerState *Host = Level->SpawnActor<AWoRPlayerState>();
		AWoRPlayerState *Guest = Level->SpawnActor<AWoRPlayerState>();
		if (!TestTrue(TEXT("Save, game state and two players"), Save && GameState && Host && Guest))
		{
			return;
		}
		Level->SetGameState(GameState);
		GameState->AddPlayerState(Host);
		GameState->AddPlayerState(Guest);
		Host->GetWallet()->Add(ECurrencyType::Gold, GOLD);
		Guest->GetWallet()->Add(ECurrencyType::Gold, GOLD);

		Save->ClearRunGold(Level);
		TestEqual(TEXT("The host's Gold"), Host->GetWallet()->GetBalance(ECurrencyType::Gold), 0);
		TestEqual(TEXT("The guest's Gold"), Guest->GetWallet()->GetBalance(ECurrencyType::Gold), 0);
	});

	It("brings the pool's items back with the same ids and tiers", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After))
		{
			return;
		}
		UPoolSubsystem *Pool = Before.GetSubsystem<UPoolSubsystem>();
		UPoolSubsystem *NewPool = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pools"), Pool) || !TestNotNull(TEXT("New pool"), NewPool))
		{
			return;
		}
		UWeaponData *Sword = NewObject<UWeaponData>(GetTransientPackage());
		FWeaponInventoryEntry Entry = FWeaponInventoryEntry::CreateFromWeapon(Sword);
		Entry.PersistentID = FGuid::NewGuid();
		Entry.Tier = EItemTier::B_Tier;
		Pool->AddWeaponToPool(Entry);
		FCrystalId Crystal;
		Crystal.Type = ECrystalType::Garnet;
		Crystal.Tier = EItemTier::C_Tier;
		Pool->AddCrystalToPool(Crystal, CRYSTAL_COUNT);

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Pool = USaveSubsystem::CapturePool(*Pool);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded))
		{
			return;
		}
		TestEqual(TEXT("Entries restored"), USaveSubsystem::RestorePool(Loaded->Data.Pool, *NewPool), 2);
		const FWeaponInventoryEntry *Back = NewPool->FindWeaponInPool(Entry.PersistentID);
		if (TestNotNull(TEXT("Same weapon id"), Back))
		{
			TestTrue(TEXT("Same asset"), Back->Weapon == Sword);
			TestTrue(TEXT("Same tier"), Back->Tier == EItemTier::B_Tier);
		}
		const int32 *Count = NewPool->GetCrystals().Find(Crystal);
		const int32 *StoneCount = NewPool->GetStones().Find(Crystal);
		TestEqual(TEXT("Crystal stack"), Count ? *Count : StoneCount ? *StoneCount : 0, CRYSTAL_COUNT);
	});

	It("bumps the save version for the seven balances and wipes everything older (B-333, D-060)", [this]()
	{
		TestTrue(TEXT("Latest is at least the seven-balances version"), WoRSaveVersion::LATEST >= SEVEN_BALANCES_SAVE_VERSION);
		// B-348 (D-138): the minimum is LATEST, so it is at least the seven-balances version.
		TestTrue(TEXT("Minimum is at least the seven-balances version"), WoRSaveVersion::MIN_SUPPORTED >= SEVEN_BALANCES_SAVE_VERSION);
	});

	It("wipes a save below the minimum version (D-060)", [this]()
	{
		UWoRSaveGame *Old = NewObject<UWoRSaveGame>();
		Old->Data.Party.bHasParty = true; // never stamped: version 0
		UWoRSaveGame *Loaded = RoundTrip(Old);
		if (!TestNotNull(TEXT("Loaded"), Loaded))
		{
			return;
		}
		TestTrue(TEXT("Wiped"), Loaded->WasWiped());
		TestFalse(TEXT("Counts as fresh"), Loaded->WasLoaded());
		TestFalse(TEXT("Data gone"), Loaded->Data.Party.bHasParty);

		UWoRSaveGame *Current = NewObject<UWoRSaveGame>();
		Current->HandlePreSave();
		Current->Data.Party.bHasParty = true;
		UWoRSaveGame *Kept = RoundTrip(Current);
		TestTrue(TEXT("A current save is kept"), Kept && !Kept->WasWiped() && Kept->Data.Party.bHasParty);
	});

	It("wipes a save one version below LATEST, with a warning (D-138, D-060)", [this]()
	{
		TestEqual(TEXT("One save version while in dev"), WoRSaveVersion::MIN_SUPPORTED, WoRSaveVersion::LATEST);
		UWoRSaveGame *Old = NewObject<UWoRSaveGame>();
		Old->HandlePreSave();
		FIntProperty *Version = FindFProperty<FIntProperty>(ULocalPlayerSaveGame::StaticClass(), TEXT("SavedDataVersion"));
		if (!TestNotNull(TEXT("The save's version field"), Version)) return;
		Version->SetPropertyValue_InContainer(Old, WoRSaveVersion::LATEST - ONE_VERSION_BACK);
		Old->Data.Party.bHasParty = true;
		AddExpectedError(TEXT("below the minimum"), EAutomationExpectedErrorFlags::Contains, 0);
		UWoRSaveGame *Loaded = RoundTrip(Old);
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;
		TestTrue(TEXT("Wiped"), Loaded->WasWiped());
		TestFalse(TEXT("Data gone"), Loaded->Data.Party.bHasParty);
	});

	It("adds an older save's Gear and Skill essence into Dormant Essence, once (B-550, D-235)", [this]()
	{
		FWoRTestWorld World;
		if (!World.Create()) return;
		FSavedWallet Older;
		Older.bHasWallet = true;
		for (const TPair<ECurrencyType, int32> &Part : {TPair<ECurrencyType, int32>(ECurrencyType::GearEssence, GEAR_ESSENCE),
														 TPair<ECurrencyType, int32>(ECurrencyType::SkillEssence, SKILL_ESSENCE)})
		{
			FSavedBalance &Balance = Older.Balances.AddDefaulted_GetRef();
			Balance.Currency = static_cast<uint8>(Part.Key);
			Balance.Amount = Part.Value;
		}
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(World.GetWorld()->SpawnActor<AActor>());
		USaveSubsystem::ApplyWallet(Older, Wallet);
		USaveSubsystem::ApplyWallet(Older, Wallet);
		TestEqual(TEXT("both added, not doubled by a second apply"), Wallet->GetGearEssence(), GEAR_ESSENCE + SKILL_ESSENCE);
		const FSavedWallet Saved = USaveSubsystem::CaptureWallet(Wallet);
		TestFalse(TEXT("only Dormant (GearEssence) is banked"), Saved.Balances.ContainsByPredicate([](const FSavedBalance &Balance)
		{
			return Balance.Currency == static_cast<uint8>(ECurrencyType::SkillEssence);
		}));
	});

	It("keeps every value whose SaveGame tag went, through the explicit save structs (D-059)", [this]()
	{
		FWoRTestWorld Before;
		FWoRTestWorld After;
		if (!CreateWorlds(Before, After)) return;
		UCurrencyComponent *Wallet = FWoRTestWorld::AddComponent<UCurrencyComponent>(Before.GetWorld()->SpawnActor<AActor>());
		Wallet->Add(ECurrencyType::Prisms, PRISMS);
		Wallet->Add(ECurrencyType::Diamond, DIAMONDS);
		Wallet->AddGearEssence(GEAR_ESSENCE);
		Wallet->AddSkillEssence(SKILL_ESSENCE);
		Wallet->Add(ECurrencyType::ElementalEssence, ELEMENTAL_ESSENCE); // B-333: the typed essences are gone
		UPoolSubsystem *Pool = Before.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pool"), Pool)) return;
		UEvolutionItemData *EvolutionItem = NewObject<UEvolutionItemData>(GetTransientPackage());
		FEvolutionInventoryEntry Evolution;
		Evolution.Item = EvolutionItem;
		Evolution.InstanceID = FGuid::NewGuid();
		Evolution.CurrentDurability = EVOLUTION_DURABILITY;
		Pool->AddEvolutionToPool(Evolution);
		USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
		FSpellInstance Learned(Spell);
		Pool->AddSpellToPool(Learned);
		const FCrystalId Stone(ECrystalType::DurabilityStone, EItemTier::F_Tier);
		Pool->AddCrystalToPool(Stone, CRYSTAL_COUNT);

		UWoRSaveGame *Save = NewObject<UWoRSaveGame>();
		Save->HandlePreSave();
		Save->Data.Wallet = USaveSubsystem::CaptureWallet(Wallet);
		Save->Data.Pool = USaveSubsystem::CapturePool(*Pool);
		UWoRSaveGame *Loaded = RoundTrip(Save);
		if (!TestNotNull(TEXT("Loaded"), Loaded)) return;

		UCurrencyComponent *Fresh = FWoRTestWorld::AddComponent<UCurrencyComponent>(After.GetWorld()->SpawnActor<AActor>());
		USaveSubsystem::ApplyWallet(Loaded->Data.Wallet, Fresh);
		TestEqual(TEXT("Prisms"), Fresh->GetBalance(ECurrencyType::Prisms), PRISMS);
		TestEqual(TEXT("Diamonds"), Fresh->GetBalance(ECurrencyType::Diamond), DIAMONDS);
		TestEqual(TEXT("Dormant essence: both added (B-550)"), Fresh->GetGearEssence(), GEAR_ESSENCE + SKILL_ESSENCE);
		TestEqual(TEXT("Skill essence reads the same balance"), Fresh->GetSkillEssence(), GEAR_ESSENCE + SKILL_ESSENCE);
		TestEqual(TEXT("Elemental essence"), Fresh->GetElementalEssence(), ELEMENTAL_ESSENCE);

		UPoolSubsystem *NewPool = After.GetSubsystem<UPoolSubsystem>();
		if (!TestNotNull(TEXT("Pool after"), NewPool)) return;
		USaveSubsystem::RestorePool(Loaded->Data.Pool, *NewPool);
		const FEvolutionInventoryEntry *Back = NewPool->FindEvolutionInPool(Evolution.InstanceID);
		TestTrue(TEXT("The evolution, with its durability"), Back && Back->Item == EvolutionItem && Back->CurrentDurability == EVOLUTION_DURABILITY);
		TestTrue(TEXT("The learned spell"), NewPool->FindSpellInPool(Learned.InstanceID) != nullptr);
		const int32 *Stones = NewPool->GetStones().Find(Stone);
		TestEqual(TEXT("The stored stones"), Stones ? *Stones : 0, CRYSTAL_COUNT);
	});
}

#endif
