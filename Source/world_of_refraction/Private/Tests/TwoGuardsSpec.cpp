// TwoGuardsSpec.cpp
// B-392 (D-170): the two guards. With no weapon in hand a character guards unarmed (its Cosmetics BlockMontage); with
// one it guards with the weapon (the weapon's ParryMontage) unless it chose Unarmed for that weapon; equipping another
// weapon brings the weapon guard back (Q-392). A weapon with no guard montage yet falls back to the unarmed one. The
// guard is looks only: the defense window is the same in both.
// B-392b: a co-op guest's guard and parry colour priority reach the host (ServerSetGuard / ServerSetParryPriority) and
// apply to its copy of the fighter, only from the player who commands that fighter.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Animation/AnimMontage.h"
#include "Character/CosmeticsData.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Combat/Defense/EGuardStyle.h"
#include "Combat/Defense/EParryElementPriority.h"
#include "Character/CharacterDataComponent.h"
#include "Player/WoRPlayerState.h"
#include "Equipment/Weapons/WeaponData.h"

namespace
{
	constexpr EActionType TWO_GUARDS_TEST_ATTACK_TYPE = EActionType::Ability;
	constexpr bool TWO_GUARDS_WEAPON_IN_HAND = true;
	constexpr bool TWO_GUARDS_NO_WEAPON = false;
}

BEGIN_DEFINE_SPEC(FTwoGuardsSpec, "WoR.Defense.TwoGuards",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ULoadoutComponent *Loadout = nullptr;
	UAnimMontage *BodyGuard = nullptr;
	UAnimMontage *SwordGuard = nullptr;
	UWeaponData *Sword = nullptr;

	FCombatLoadout &Active() const
	{
		UInventoryComponent *Inventory = Loadout->GetOwner()->FindComponentByClass<UInventoryComponent>();
		return Inventory->SavedLoadouts[Inventory->ActiveLoadoutIndex];
	}

	/** A Generic whose unarmed guard is BodyGuard; with bArmed, holding Sword (its guard SwordGuard, its own GUID). */
	bool SpawnGuard(bool bArmed)
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->CharacterClass = ECharacterClass::Generic;
		Data->Cosmetics = NewObject<UCosmeticsData>(GetTransientPackage());
		BodyGuard = NewObject<UAnimMontage>(GetTransientPackage());
		Data->Cosmetics->BlockMontage = BodyGuard;
		Sword = NewObject<UWeaponData>(GetTransientPackage());
		SwordGuard = NewObject<UAnimMontage>(GetTransientPackage());
		Sword->ParryMontage = SwordGuard;
		Loadout = TestWorld->SpawnCombatantWithLoadout(Data, bArmed ? Sword : nullptr);
		if (!Loadout)
		{
			return false;
		}
		if (bArmed)
		{
			Active().PrimaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
		}
		return true;
	}

	/** Equips another weapon (its own GUID) in the primary slot, as a loadout edit does. */
	UWeaponData *EquipAnotherWeapon(UAnimMontage *ItsGuard)
	{
		UWeaponData *Axe = NewObject<UWeaponData>(GetTransientPackage());
		Axe->ParryMontage = ItsGuard;
		Active().PrimaryWeapon.WeaponEntry.Weapon = Axe;
		Active().PrimaryWeapon.WeaponEntry.PersistentID = FGuid::NewGuid();
		Loadout->NotifyActiveWeaponChanged();
		return Axe;
	}
END_DEFINE_SPEC(FTwoGuardsSpec)

void FTwoGuardsSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]()
	{
		Loadout = nullptr;
		TestWorld.Reset();
	});

	Describe("the rule", [this]()
	{
		It("is Unarmed with no weapon, and Unarmed with a weapon only for the weapon it was chosen with", [this]()
		{
			const FGuid Chosen = FGuid::NewGuid();
			const FGuid Other = FGuid::NewGuid();
			TestTrue(TEXT("no weapon"), ULoadoutComponent::ResolveGuardStyle(TWO_GUARDS_NO_WEAPON, FGuid(), EGuardStyle::Weapon, FGuid()) == EGuardStyle::Unarmed);
			TestTrue(TEXT("weapon, default"), ULoadoutComponent::ResolveGuardStyle(TWO_GUARDS_WEAPON_IN_HAND, Chosen, EGuardStyle::Weapon, FGuid()) == EGuardStyle::Weapon);
			TestTrue(TEXT("weapon put away"), ULoadoutComponent::ResolveGuardStyle(TWO_GUARDS_WEAPON_IN_HAND, Chosen, EGuardStyle::Unarmed, Chosen) == EGuardStyle::Unarmed);
			TestTrue(TEXT("another weapon equipped"), ULoadoutComponent::ResolveGuardStyle(TWO_GUARDS_WEAPON_IN_HAND, Other, EGuardStyle::Unarmed, Chosen) == EGuardStyle::Weapon);
		});
	});

	Describe("the montage", [this]()
	{
		It("an unarmed character guards with its body and can't choose the weapon guard away", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnGuard(false))) return;
			TestTrue(TEXT("Unarmed"), Loadout->GetGuardStyle() == EGuardStyle::Unarmed);
			TestTrue(TEXT("plays the body guard"), Loadout->GetGuardMontage() == BodyGuard);
			TestFalse(TEXT("Unarmed refused: already unarmed"), Loadout->SetGuardStyle(EGuardStyle::Unarmed));
		});

		It("a weapon defaults to its weapon guard; the setting puts it away", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnGuard(true))) return;
			TestTrue(TEXT("Weapon by default"), Loadout->GetGuardStyle() == EGuardStyle::Weapon);
			TestTrue(TEXT("plays the weapon guard"), Loadout->GetGuardMontage() == SwordGuard);
			TestTrue(TEXT("Unarmed accepted"), Loadout->SetGuardStyle(EGuardStyle::Unarmed));
			TestTrue(TEXT("Unarmed"), Loadout->GetGuardStyle() == EGuardStyle::Unarmed);
			TestTrue(TEXT("plays the body guard"), Loadout->GetGuardMontage() == BodyGuard);
			TestTrue(TEXT("Weapon accepted"), Loadout->SetGuardStyle(EGuardStyle::Weapon));
			TestTrue(TEXT("the weapon guard again"), Loadout->GetGuardMontage() == SwordGuard);
		});

		It("equipping another weapon makes the weapon guard the default again (D-170)", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnGuard(true))) return;
			TestTrue(TEXT("Unarmed accepted"), Loadout->SetGuardStyle(EGuardStyle::Unarmed));
			UAnimMontage *AxeGuard = NewObject<UAnimMontage>(GetTransientPackage());
			EquipAnotherWeapon(AxeGuard);
			TestTrue(TEXT("Weapon"), Loadout->GetGuardStyle() == EGuardStyle::Weapon);
			TestTrue(TEXT("plays the new weapon's guard"), Loadout->GetGuardMontage() == AxeGuard);
		});

		It("a weapon with no guard montage yet falls back to the body guard", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnGuard(true))) return;
			EquipAnotherWeapon(nullptr);
			TestTrue(TEXT("still the weapon guard"), Loadout->GetGuardStyle() == EGuardStyle::Weapon);
			TestTrue(TEXT("plays the body guard"), Loadout->GetGuardMontage() == BodyGuard);
		});
	});

	Describe("co-op (B-392b)", [this]()
	{
		It("the host applies a guest's guard and parry priority to the guest's own fighter only", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnGuard(TWO_GUARDS_WEAPON_IN_HAND))) return;
			AActor *Fighter = Loadout->GetOwner();
			UCharacterDataComponent *CharComp = Fighter->FindComponentByClass<UCharacterDataComponent>();
			UWorld *World = TestWorld->GetWorld();
			AWoRPlayerState *Guest = World->SpawnActor<AWoRPlayerState>();
			AWoRPlayerState *Other = World->SpawnActor<AWoRPlayerState>();
			if (!TestNotNull(TEXT("character data"), CharComp) || !TestNotNull(TEXT("guest"), Guest) || !TestNotNull(TEXT("other"), Other)) return;
			CharComp->PartyMemberId = FGuid::NewGuid(); // the guest's member, as stamped on the host's pawn
			Guest->ClaimMember(CharComp->PartyMemberId);
			TestTrue(TEXT("the guest commands it"), Guest->CommandsFighter(Fighter));
			TestFalse(TEXT("the other player does not"), Other->CommandsFighter(Fighter));

			Other->ServerSetGuard(Fighter, EGuardStyle::Unarmed); // runs here: the test world is the host
			TestTrue(TEXT("another player's guard is refused"), Loadout->GetGuardStyle() == EGuardStyle::Weapon);
			Guest->ServerSetGuard(Fighter, EGuardStyle::Unarmed);
			TestTrue(TEXT("the guest's own reaches the host"), Loadout->GetGuardStyle() == EGuardStyle::Unarmed);
			TestTrue(TEXT("and plays the body guard"), Loadout->GetGuardMontage() == BodyGuard);
			Guest->ServerSetGuard(Fighter, EGuardStyle::Weapon);
			TestTrue(TEXT("and back to the weapon guard"), Loadout->GetGuardStyle() == EGuardStyle::Weapon);

			TestTrue(TEXT("parry priority starts at Weapon"), Loadout->GetParryElementPriority() == EParryElementPriority::Weapon);
			Other->ServerSetParryPriority(Fighter, EParryElementPriority::Magic);
			TestTrue(TEXT("another player's priority is refused"), Loadout->GetParryElementPriority() == EParryElementPriority::Weapon);
			Guest->ServerSetParryPriority(Fighter, EParryElementPriority::Magic);
			TestTrue(TEXT("the guest's priority reaches the host"), Loadout->GetParryElementPriority() == EParryElementPriority::Magic);
			Guest->ServerSetGuard(nullptr, EGuardStyle::Unarmed); // a fighter that did not replicate: refused, no crash
		});

		It("a pawn with no member id is commanded by the player who claims its character", [this]()
		{
			if (!TestTrue(TEXT("spawned"), SpawnGuard(TWO_GUARDS_WEAPON_IN_HAND))) return;
			AActor *Fighter = Loadout->GetOwner();
			const UCharacterDataComponent *CharComp = Fighter->FindComponentByClass<UCharacterDataComponent>();
			AWoRPlayerState *Guest = TestWorld->GetWorld()->SpawnActor<AWoRPlayerState>();
			if (!TestNotNull(TEXT("character data"), CharComp) || !TestNotNull(TEXT("guest"), Guest)) return;
			TestFalse(TEXT("not before the claim"), Guest->CommandsFighter(Fighter));
			Guest->ClaimCharacter(CharComp->CharacterData);
			TestTrue(TEXT("its character's player"), Guest->CommandsFighter(Fighter));
		});
	});

	It("the defense window is the same in both guards (D-170: looks only)", [this]()
	{
		if (!TestTrue(TEXT("spawned"), SpawnGuard(true))) return;
		UDefenseSystem *DefenseSystem = TestWorld->GetSubsystem<UDefenseSystem>();
		AActor *Attacker = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData(), true);
		if (!TestNotNull(TEXT("DefenseSystem"), DefenseSystem) || !TestNotNull(TEXT("Attacker"), Attacker)) return;
		AActor *Defender = Loadout->GetOwner();
		const float WeaponWindow = DefenseSystem->GetEffectiveDefenseInputWindow(Defender, Attacker, TWO_GUARDS_TEST_ATTACK_TYPE);
		TestTrue(TEXT("Unarmed accepted"), Loadout->SetGuardStyle(EGuardStyle::Unarmed));
		const float UnarmedWindow = DefenseSystem->GetEffectiveDefenseInputWindow(Defender, Attacker, TWO_GUARDS_TEST_ATTACK_TYPE);
		TestTrue(TEXT("a real window"), WeaponWindow > 0.0f);
		TestEqual(TEXT("same window"), UnarmedWindow, WeaponWindow);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
