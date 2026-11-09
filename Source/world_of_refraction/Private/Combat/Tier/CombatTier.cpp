// CombatTier.cpp
// B-325 (D-118). See CombatTier.h.

#include "Combat/Tier/CombatTier.h"
#include "GameFramework/Actor.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/Entries/FWeaponLoadoutEntry.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

#define LOCTEXT_NAMESPACE "CombatTier"

EItemTier WoRCombatTier::GetAverageTier(TConstArrayView<EItemTier> Tiers, EItemTier Fallback)
{
	if (Tiers.Num() == 0)
	{
		return Fallback;
	}
	int32 Sum = 0;
	for (const EItemTier Tier : Tiers)
	{
		Sum += TierHelpers::GetTierValue(Tier);
	}
	// Integer division of non-negative values rounds down exactly (Q-325).
	const int32 TierIndex = FMath::Clamp(Sum / Tiers.Num(), TierHelpers::GetTierValue(EItemTier::F_Tier),
										 TierHelpers::GetTierValue(EItemTier::S_Tier));
	return static_cast<EItemTier>(TierIndex);
}

void WoRCombatTier::GatherTiers(const AActor *Actor, TArray<EItemTier> &OutTiers)
{
	OutTiers.Reset();
	const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return;
	}
	// The same gear GetActiveStatBonus counts (B-306), at each item's instance tier.
	Loadout->ForEachContributingGear([&OutTiers](const FWeaponLoadoutEntry *Weapon, const FRingLoadoutEntry *Adapter)
	{
		if (Weapon && Weapon->WeaponEntry.Weapon)
		{
			OutTiers.Add(Weapon->WeaponEntry.Tier);
		}
		else if (Adapter && Adapter->RingEntry.Ring)
		{
			OutTiers.Add(Adapter->RingEntry.Tier);
		}
	});
	const FEvolutionAttachment Seat = Loadout->GetCharacterSlotEvolution(); // B-357e: the one resolve path
	if (Seat.Item)
	{
		OutTiers.Add(Seat.Tier);
	}
	for (const UAbilityData *Ability : Loadout->GetAvailableAbilities())
	{
		if (Ability)
		{
			OutTiers.Add(Ability->Tier);
		}
	}
	for (const USpellData *Spell : Loadout->GetAvailableSpells())
	{
		if (Spell)
		{
			OutTiers.Add(Spell->Tier);
		}
	}
}

EItemTier WoRCombatTier::GetCombatTier(const AActor *Actor)
{
	const UCharacterDataComponent *CharComp = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UCharacterData *Data = CharComp ? CharComp->CharacterData : nullptr;
	const EItemTier Fallback = Data ? Data->EnemyTier : EItemTier::F_Tier;
	TArray<EItemTier> Tiers;
	GatherTiers(Actor, Tiers);
	return GetAverageTier(Tiers, Fallback);
}

FText WoRCombatTier::GetCombatTierText(EItemTier Tier)
{
	return FText::Format(LOCTEXT("CombatTierLabel", "Tier {0}"), FText::FromString(TierHelpers::GetTierName(Tier)));
}

#undef LOCTEXT_NAMESPACE
