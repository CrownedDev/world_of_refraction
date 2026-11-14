// WeaponSetRulesDebug.cpp

#include "Debug/Inventory/WeaponSetRulesDebug.h"
#include "Loadout/WeaponSetRules.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Character/StanceData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float WEAPON_SETS_SCREEN_SECONDS = 10.0f;

	FString DescribeHand(const FWeaponInventoryEntry &Hand)
	{
		return Hand.Weapon ? FString::Printf(TEXT("%s (%s, %s)"), *GetNameSafe(Hand.Weapon), *UEnum::GetValueAsString(Hand.Tier),
											 *UEnum::GetValueAsString(Hand.Weapon->Handling))
						   : FString(TEXT("-"));
	}

	FAutoConsoleCommandWithWorld GPrintWeaponSetsCmd(
		TEXT("wor.Loadout.PrintWeaponSets"),
		TEXT("DEBUG (B-477): per character, each weapon set's hands, whether the pair is allowed, its skill slots (D-211) and stance."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { WeaponSetRulesDebug::PrintWeaponSets(World); }));
}

FString WeaponSetRulesDebug::GetWeaponSetString(const FWeaponLoadoutEntry &Set)
{
	if (!Set.IsValid())
	{
		return TEXT("empty");
	}
	FString Pair;
	if (Set.HasOffHand())
	{
		const WeaponSetRules::EPairRefusal Refusal = WeaponSetRules::CheckPair(Set.WeaponEntry, Set.OffHandEntry);
		Pair = Refusal == WeaponSetRules::EPairRefusal::None
				   ? FString(TEXT("pair OK"))
				   : FString::Printf(TEXT("pair REFUSED (%s)"), *WeaponSetRules::GetPairRefusalText(Refusal).ToString());
	}
	else
	{
		Pair = WeaponSetRules::CanStandAlone(Set) ? FString(TEXT("alone")) : FString(TEXT("alone REFUSED (dual only)"));
	}
	const UStanceData *Stance = WeaponSetRules::GetSetStance(Set);
	return FString::Printf(TEXT("%s + %s: %s, %d tier slots, %d skill slots, %d built-ins, wield %s, stance %s"), *DescribeHand(Set.WeaponEntry),
						   *DescribeHand(Set.OffHandEntry), *Pair, WeaponSetRules::GetSetTierSlots(Set), Set.GetSkillSlotCount(),
						   WeaponSetRules::GetSetBuiltIns(Set).Num(), *UEnum::GetValueAsString(WeaponSetRules::GetSetWieldMode(Set)),
						   Stance ? *Stance->StanceName : TEXT("none"));
}

void WeaponSetRulesDebug::PrintWeaponSets(UWorld *World)
{
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const ULoadoutComponent *Loadout = It->FindComponentByClass<ULoadoutComponent>();
		if (!Loadout)
		{
			continue;
		}
		const FCombatLoadout &Active = Loadout->GetActiveLoadout();
		const FString Line = FString::Printf(TEXT("%s: primary %s | secondary %s"), *GetNameSafe(*It), *GetWeaponSetString(Active.PrimaryWeapon),
											 *GetWeaponSetString(Active.SecondaryWeapon));
		UE_LOG(LogWoRLoadout, Display, TEXT("[WeaponSets] %s"), *Line);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, WEAPON_SETS_SCREEN_SECONDS, FColor::Cyan, Line);
		}
	}
}
