// CrystalBreak.cpp

#include "Equipment/Durability/CrystalBreak.h"
#include "GameFramework/Actor.h"
#include "Engine/GameInstance.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/EAttachedItemKind.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Crystals/EvolutionItemData.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Crystals/CrystalTypeHelpers.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h"

namespace
{
	/** Branch-aware display name. Refined crystals have no asset pointer; resolve via ItemIdentity. */
	FString CrystalBreakDisplayName(const FEquippedCrystalSlot &Slot)
	{
		switch (Slot.Kind)
		{
		case EAttachedItemKind::Crystal:
			return ItemIdentity::GetDisplayName(Slot.CrystalId);
		case EAttachedItemKind::Evolution:
			return Slot.Item ? Slot.Item->GetFullItemName() : FString(TEXT("(null)"));
		default:
			return TEXT("(none)");
		}
	}

	/** "Weapon '<name>'" / "Ring '<name>'"; empty for any other holder, which is never cleared. */
	FString CrystalBreakHolderDesc(const UObject *Holder)
	{
		if (const UWeaponData *Weapon = Cast<UWeaponData>(Holder))
		{
			return FString::Printf(TEXT("Weapon '%s'"), *Weapon->Name);
		}
		if (const URingData *Ring = Cast<URingData>(Holder))
		{
			return FString::Printf(TEXT("Ring '%s'"), *Ring->Name);
		}
		return FString();
	}

	FString CrystalBreakEssenceName(EEssenceType Type)
	{
		return StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Type));
	}

	/** Dismantles an owned evolution entry (hybrid essence + remove entry + free cap slot). An
	 *  invalid InstanceID is an authored / asset-only evolution with no owned record: the slot
	 *  clear is the whole action, no essence. */
	void CrystalBreakDismantleEvolution(AActor *Actor, const FGuid &InstanceID)
	{
		if (!InstanceID.IsValid())
		{
			return;
		}
		UGameInstance *GameInstance = Actor->GetGameInstance();
		if (UEconomyService *Economy = GameInstance ? GameInstance->GetSubsystem<UEconomyService>() : nullptr)
		{
			Economy->DismantleEvolution(Actor, InstanceID);
		}
	}

	/** Gem break: the crystal's TYPED essence at the crystal curve. A socketed crystal lives only in
	 *  the socket (attach consumed it from the pool), so grant directly with no pool removal. */
	void CrystalBreakYieldGem(AActor *Actor, const FCrystalId &CrystalId, const FString &CrystalName)
	{
		UCurrencyComponent *Currency = Actor->FindComponentByClass<UCurrencyComponent>();
		if (!Currency)
		{
			return;
		}
		const EEssenceType EssenceType = EconomyYield::ResolveEssenceType(CrystalId);
		const int32 Yield = EconomyYield::GetTypedEssenceYieldForTier(CrystalId.Tier);
		Currency->AddEssenceType(EssenceType, Yield);
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Broken crystal '%s' on %s dismantled -> %d %s essence"),
			   *CrystalName, *Actor->GetName(), Yield, *CrystalBreakEssenceName(EssenceType));
	}

	/** Fusion break, lossy: each GEM half yields half its crystal-curve essence (floored). Only an
	 *  elemental fusion (gem + stone) wears, so in practice one half yields; stone halves never do.
	 *  Half-per-gem discourages fuse-then-break for value. */
	void CrystalBreakYieldFusion(AActor *Actor, const FRuntimeAttachedItem &Attachment)
	{
		UCurrencyComponent *Currency = Actor->FindComponentByClass<UCurrencyComponent>();
		if (!Currency)
		{
			return;
		}
		const FCrystalId Halves[2] = {Attachment.Fusion.Id.HalfA, Attachment.Fusion.Id.HalfB};
		for (const FCrystalId &Half : Halves)
		{
			if (!CrystalTypeHelpers::IsGemType(Half.Type))
			{
				continue;
			}
			const int32 FullYield = EconomyYield::GetTypedEssenceYieldForTier(Half.Tier);
			const int32 HalfYield = FMath::FloorToInt(FullYield * EconomyYield::Constants::FUSION_BREAK_ESSENCE_FRACTION);
			if (HalfYield <= 0)
			{
				continue;
			}
			const EEssenceType EssenceType = EconomyYield::ResolveEssenceType(Half);
			Currency->AddEssenceType(EssenceType, HalfYield);
			UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Broken fusion gem '%s' on %s dismantled -> %d %s essence (half of %d)"),
				   *ItemIdentity::GetDisplayName(Half), *Actor->GetName(), HalfYield, *CrystalBreakEssenceName(EssenceType), FullYield);
		}
	}

	/** Forced dismantle of a cleared attachment (§4.2 / §5.3b). Attachment is the pre-clear copy. */
	void CrystalBreakDismantle(AActor *Actor, const FEquippedCrystalSlot &Slot, const FRuntimeAttachedItem &Attachment,
							   const FString &CrystalName)
	{
		switch (Slot.Kind)
		{
		case EAttachedItemKind::Crystal:
			CrystalBreakYieldGem(Actor, Slot.CrystalId, CrystalName);
			break;
		case EAttachedItemKind::Evolution:
			CrystalBreakDismantleEvolution(Actor, Attachment.Evolution.InstanceID);
			break;
		case EAttachedItemKind::Fusion:
			CrystalBreakYieldFusion(Actor, Attachment);
			break;
		default:
			break; // stones don't break
		}
	}

	/** Clears one equipped attachment if it broke; true when it did. */
	bool CrystalBreakDestroySlot(AActor *Actor, ULoadoutComponent *Loadout, const FEquippedCrystalSlot &Slot)
	{
		if (Slot.Kind == EAttachedItemKind::None)
		{
			return false;
		}
		// IsBroken() filters durability <= 0 (Unbreakable evolutions never report broken). An
		// unmatched holder returns an empty attachment whose IsBroken() is false.
		const FRuntimeAttachedItem Attachment = Loadout->GetCrystalEntryByHolder(Slot.Holder);
		if (!Attachment.IsBroken())
		{
			return false;
		}
		// Only weapon and ring holders are cleared; an evolution self-holder never matches above.
		const FString HolderDesc = CrystalBreakHolderDesc(Slot.Holder);
		if (HolderDesc.IsEmpty())
		{
			return false;
		}

		const FString CrystalName = CrystalBreakDisplayName(Slot);
		Loadout->ResetCrystalEntryByHolder(Slot.Holder);
		CrystalBreakDismantle(Actor, Slot, Attachment, CrystalName);
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Destroyed broken crystal '%s' from %s on %s"),
			   *CrystalName, *HolderDesc, *Actor->GetName());
		return true;
	}

	/** Case B: a standalone primary-slot evolution (e.g. Broken Darkness), which GetEquippedCrystals
	 *  doesn't surface. The owned-entry GUID is read BEFORE the clear wipes it. */
	bool CrystalBreakDestroyPrimaryEvolution(AActor *Actor, ULoadoutComponent *Loadout)
	{
		const FGuid BrokenEvoInstance = Loadout->GetActivePrimaryEvolutionInstance();
		if (!Loadout->ClearBrokenPrimaryEvolution())
		{
			return false;
		}
		CrystalBreakDismantleEvolution(Actor, BrokenEvoInstance);
		UE_LOG(LogTemp, Log, TEXT("[CombatOrchestrator] Broke + dismantled primary evolution on %s"), *Actor->GetName());
		return true;
	}
}

int32 CrystalBreak::DestroyBrokenOn(AActor *Actor)
{
	ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return 0;
	}
	int32 Destroyed = 0;
	for (const FEquippedCrystalSlot &Slot : Loadout->GetEquippedCrystals())
	{
		Destroyed += CrystalBreakDestroySlot(Actor, Loadout, Slot) ? 1 : 0;
	}
	Destroyed += CrystalBreakDestroyPrimaryEvolution(Actor, Loadout) ? 1 : 0;
	return Destroyed;
}
