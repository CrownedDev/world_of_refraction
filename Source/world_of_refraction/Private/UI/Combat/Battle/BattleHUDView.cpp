// BattleHUDView.cpp

#include "UI/Combat/Battle/BattleHUDView.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatConstants.h"
#include "Combat/CombatPlayerController.h"
#include "Combat/TurnManager.h"
#include "Combat/WoRFightSubsystem.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Actor.h"
#include "Infusion/ElementColors.h"
#include "Infusion/HybridSpellColors.h"
#include "Loadout/Entries/FRingLoadoutEntry.h"
#include "Loadout/FCombatLoadout.h"
#include "Loadout/LoadoutComponent.h"
#include "Player/TurnStateComponent.h"
#include "Skills/Effects/ActiveSkillEffect.h"
#include "UI/Combat/CombatCommandMenuSubsystem.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "BattleHUD"

namespace
{
	/** "xN" shows from this many stacks. */
	constexpr int32 MIN_STACKS_SHOWN = 2;

	// B-391b: the old panel's overload tints (CharacterPanelWidget::RefreshEnergyBar).
	const FLinearColor OVERLOAD_NONE_COLOUR = FLinearColor::White;
	const FLinearColor OVERLOAD_YELLOW_COLOUR = FLinearColor::Yellow;
	const FLinearColor OVERLOAD_ORANGE_COLOUR(1.0f, 0.5f, 0.0f);
	const FLinearColor OVERLOAD_RED_COLOUR = FLinearColor::Red;
}

TArray<FBattleTurnSlot> BattleHUDView::MakeTurnStrip(AActor *Current, bool bCurrentBonus, int32 TurnNumber,
													  const TArray<FBattleUpcomingTurn> &Upcoming, int32 MaxUpcoming)
{
	TArray<FBattleTurnSlot> Strip;
	FBattleTurnSlot &Now = Strip.AddDefaulted_GetRef();
	Now.Actor = Current;
	Now.TurnNumber = TurnNumber;
	Now.bCurrent = true;
	Now.bBonus = bCurrentBonus;

	const int32 Shown = FMath::Min(Upcoming.Num(), FMath::Max(0, MaxUpcoming));
	for (int32 Index = 0; Index < Shown; ++Index)
	{
		FBattleTurnSlot &Next = Strip.AddDefaulted_GetRef();
		Next.Actor = Upcoming[Index].Actor;
		Next.TurnNumber = TurnNumber + Index + 1;
		Next.bBonus = Upcoming[Index].bBonus;
	}
	return Strip;
}

TArray<FBattleTurnSlot> BattleHUDView::ReadTurnStrip(const UObject *Context, int32 TurnNumber, int32 MaxUpcoming)
{
	TArray<FBattleUpcomingTurn> Upcoming;
	if (const UTurnStateComponent *TurnState = UTurnStateComponent::Find(Context))
	{
		for (const FTurnStateSlot &Slot : TurnState->GetUpcoming())
		{
			Upcoming.Add({Slot.Actor.Get(), Slot.bIsBonusTurn});
		}
		return MakeTurnStrip(TurnState->GetCurrentActor(), TurnState->GetCurrentTurnIsBonus(), TurnNumber, Upcoming, MaxUpcoming);
	}
	if (const UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(Context))
	{
		for (const FPreviewTurnEntry &Entry : TurnManager->PreviewTurnOrder(MaxUpcoming))
		{
			Upcoming.Add({Entry.Actor, Entry.bIsBonusTurn});
		}
		return MakeTurnStrip(TurnManager->GetCurrentActor(), TurnManager->GetCurrentTurnIsBonus(), TurnNumber, Upcoming, MaxUpcoming);
	}
	return {};
}

int32 BattleHUDView::ReadTurnNumber(const UObject *Context)
{
	if (const UTurnStateComponent *TurnState = UTurnStateComponent::Find(Context))
	{
		return FMath::Max(1, TurnState->GetTurnNumber());
	}
	if (const UTurnManager *TurnManager = WoRFight::Get<UTurnManager>(Context))
	{
		return FMath::Max(1, TurnManager->GetTurnCount());
	}
	return 1;
}

FText BattleHUDView::FormatPool(const FText &Label, int32 Current, int32 Max)
{
	return FText::Format(LOCTEXT("Pool", "{0} {1} / {2}"), Label, FText::AsNumber(Current), FText::AsNumber(Max));
}

float BattleHUDView::PoolFraction(int32 Current, int32 Max)
{
	return Max > 0 ? FMath::Clamp(static_cast<float>(Current) / static_cast<float>(Max), 0.0f, 1.0f) : 0.0f;
}

FText BattleHUDView::GetHPLabel()
{
	return LOCTEXT("HP", "HP");
}

FText BattleHUDView::GetEPLabel()
{
	return LOCTEXT("EP", "EP");
}

FText BattleHUDView::GetAbsorbLabel()
{
	return LOCTEXT("Absorb", "Absorb");
}

bool BattleHUDView::IsOverloaded(int32 Current, int32 Max)
{
	return Max > 0 && Current > Max;
}

FText BattleHUDView::FormatAbsorbLine(int32 Current, int32 Max)
{
	const FText Pool = FormatPool(GetAbsorbLabel(), Current, Max);
	if (!IsOverloaded(Current, Max))
	{
		return Pool;
	}
	const double Ratio = static_cast<double>(Current) / static_cast<double>(Max);
	return FText::Format(LOCTEXT("AbsorbOverload", "{0}, overload {1}"), Pool, FText::AsPercent(Ratio));
}

FLinearColor BattleHUDView::GetOverloadColour(int32 Current, int32 Max)
{
	if (!IsOverloaded(Current, Max))
	{
		return OVERLOAD_NONE_COLOUR;
	}
	const float Ratio = static_cast<float>(Current) / static_cast<float>(Max);
	if (Ratio > CombatConstants::OVERLOAD_RED_THRESHOLD)
	{
		return OVERLOAD_RED_COLOUR;
	}
	if (Ratio > CombatConstants::OVERLOAD_ORANGE_THRESHOLD)
	{
		return OVERLOAD_ORANGE_COLOUR;
	}
	return Ratio > CombatConstants::OVERLOAD_YELLOW_THRESHOLD ? OVERLOAD_YELLOW_COLOUR : OVERLOAD_NONE_COLOUR;
}

FText BattleHUDView::GetCombatantName(const AActor *Actor)
{
	if (!Actor)
	{
		return FText::GetEmpty();
	}
	const UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData && !CharComp->CharacterData->Name.IsEmpty())
	{
		return FText::FromString(CharComp->CharacterData->Name); // authored name (data asset)
	}
	return FText::FromString(Actor->GetName());
}

FText BattleHUDView::MakeStatusChip(const FActiveSkillEffect &Effect)
{
	const FText Name = WoRGameNames::GetSkillEffectTypeName(Effect.EffectType);
	const bool bShowTurns = !Effect.bPermanent && Effect.RemainingTurns > 0;
	const bool bShowStacks = Effect.bCanStack && Effect.CurrentStacks >= MIN_STACKS_SHOWN;
	if (bShowTurns && bShowStacks)
	{
		return FText::Format(LOCTEXT("ChipTurnsStacks", "{0} {1} x{2}"), Name, FText::AsNumber(Effect.RemainingTurns),
							 FText::AsNumber(Effect.CurrentStacks));
	}
	if (bShowTurns)
	{
		return FText::Format(LOCTEXT("ChipTurns", "{0} {1}"), Name, FText::AsNumber(Effect.RemainingTurns));
	}
	if (bShowStacks)
	{
		return FText::Format(LOCTEXT("ChipStacks", "{0} x{1}"), Name, FText::AsNumber(Effect.CurrentStacks));
	}
	return Name;
}

FText BattleHUDView::JoinStatusChips(const TArray<FActiveSkillEffect> &Effects)
{
	TArray<FText> Chips;
	for (const FActiveSkillEffect &Effect : Effects)
	{
		Chips.Add(MakeStatusChip(Effect));
	}
	return FText::Join(LOCTEXT("ChipSeparator", " · "), Chips);
}

FText BattleHUDView::DescribeStatus(const FActiveSkillEffect &Effect)
{
	const FText Name = WoRGameNames::GetSkillEffectTypeName(Effect.EffectType);
	const FText Element = Effect.Element == ESpellElement::None ? FText::GetEmpty()
																: FText::Format(LOCTEXT("StatusElement", " ({0})"), WoRGameNames::GetElementName(Effect.Element));
	const FText Turns = Effect.bPermanent ? LOCTEXT("StatusPermanent", "until removed")
										  : FText::Format(LOCTEXT("StatusTurns", "{0} {0}|plural(one=turn,other=turns) left"),
														  FText::AsNumber(Effect.RemainingTurns));
	const FText Stacks = Effect.bCanStack && Effect.CurrentStacks >= MIN_STACKS_SHOWN
							 ? FText::Format(LOCTEXT("StatusStacks", ", x{0}"), FText::AsNumber(Effect.CurrentStacks))
							 : FText::GetEmpty();
	const FText Description = Effect.Description.IsEmpty()
								  ? FText::GetEmpty()
								  : FText::Format(LOCTEXT("StatusDescription", ": {0}"), FText::FromString(Effect.Description));
	return FText::Format(LOCTEXT("StatusLine", "{0}{1}, {2}{3}{4}"), Name, Element, Turns, Stacks, Description);
}

FText BattleHUDView::WithKeyHint(const FText &Label, const FKey &Keyboard, const FKey &Gamepad)
{
	if (!Keyboard.IsValid())
	{
		return Label;
	}
	if (!Gamepad.IsValid())
	{
		return FText::Format(LOCTEXT("KeyHintKeyboard", "[{0}] {1}"), Keyboard.GetDisplayName(/*bLongDisplayName=*/false), Label);
	}
	return FText::Format(LOCTEXT("KeyHint", "[{0} / {1}] {2}"), Keyboard.GetDisplayName(/*bLongDisplayName=*/false),
						 Gamepad.GetDisplayName(/*bLongDisplayName=*/false), Label);
}

TArray<int32> BattleHUDView::AssignMenuKeys(const TArray<EPieMenuCategory> &Categories, const TArray<bool> &Enabled,
											 bool bAtMainMenu)
{
	TArray<int32> KeyForRow;
	KeyForRow.Init(INDEX_NONE, Categories.Num());
	const TArray<FCombatMenuKey> &Keys = ACombatPlayerController::GetMenuKeys();
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		const EPieMenuCategory KeyCategory = Keys[KeyIndex].Category;
		if (CommandMenuRules::IsMainMenuKey(KeyCategory) && !bAtMainMenu)
		{
			continue;
		}
		for (int32 Row = 0; Row < Categories.Num(); ++Row)
		{
			const bool bRowEnabled = Enabled.IsValidIndex(Row) && Enabled[Row];
			if (bRowEnabled && CommandMenuRules::KeyPressesButton(KeyCategory, Categories[Row]))
			{
				KeyForRow[Row] = KeyIndex;
				break;
			}
		}
	}
	return KeyForRow;
}

// ========================================
// B-391c: STATUS BUILDUP BAR
// ========================================

float BattleHUDView::BuildupFraction(float Current, float Max)
{
	return Max > 0.0f ? FMath::Clamp(Current / Max, 0.0f, 1.0f) : 0.0f;
}

TArray<float> BattleHUDView::GetCrackPositions()
{
	TArray<float> Positions;
	const float Spacing = (1.0f - CRACK_START_FRACTION) / static_cast<float>(CRACK_COUNT);
	for (int32 Index = 0; Index < CRACK_COUNT; ++Index)
	{
		Positions.Add(CRACK_START_FRACTION + Spacing * static_cast<float>(Index));
	}
	return Positions;
}

int32 BattleHUDView::CountCracksShown(float Fraction)
{
	int32 Shown = 0;
	for (const float Position : GetCrackPositions())
	{
		Shown += Fraction >= Position ? 1 : 0;
	}
	return Shown;
}

FText BattleHUDView::GetStatusLabel()
{
	return LOCTEXT("Status", "Status");
}

FText BattleHUDView::FormatBuildupLine(float Current, float Max)
{
	return FormatPool(GetStatusLabel(), FMath::TruncToInt(Current), FMath::TruncToInt(Max));
}

FLinearColor BattleHUDView::GetBuildupColour(ESpellElement PendingElement, bool bSourceInBrokenDarkness)
{
	// As CharacterPanelWidget::ApplyStatusBarTint: physical-only hits keep the neutral colour, never darkened.
	if (PendingElement == ESpellElement::None || PendingElement == ESpellElement::Generic || !bSourceInBrokenDarkness)
	{
		return ElementColors::GetColorForElement(PendingElement);
	}
	return UHybridSpellColors::GetHybridSpellColors(PendingElement).BlendedColor;
}

// ========================================
// B-391c: GEAR LINE AND TOOLTIP
// ========================================

FBattleGearNames BattleHUDView::ReadGearNames(const AActor *Actor)
{
	FBattleGearNames Gear;
	const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return Gear;
	}
	Gear.LoadoutName = Loadout->GetActiveLoadout().LoadoutName;
	if (const UWeaponData *Weapon = Loadout->GetActiveWeapon())
	{
		Gear.WeaponName = Weapon->Name;
	}
	const URingData *Ring = Loadout->GetActiveRing(); // a Resonator's active ring; everyone else's is the primary one
	if (!Ring)
	{
		Ring = Loadout->GetPrimaryRing();
	}
	if (Ring)
	{
		Gear.RingName = Ring->Name;
	}
	return Gear;
}

FText BattleHUDView::FormatGearLine(const FBattleGearNames &Gear)
{
	if (!Gear.WeaponName.IsEmpty() && !Gear.RingName.IsEmpty())
	{
		return FText::Format(LOCTEXT("GearBoth", "{0} · {1}"), FText::FromString(Gear.WeaponName), FText::FromString(Gear.RingName));
	}
	if (!Gear.WeaponName.IsEmpty())
	{
		return FText::FromString(Gear.WeaponName); // authored name (data asset)
	}
	return Gear.RingName.IsEmpty() ? FText::GetEmpty() : FText::FromString(Gear.RingName);
}

FText BattleHUDView::FormatGearTooltip(const FBattleGearNames &Gear)
{
	const auto OrNone = [](const FString &Name) { return Name.IsEmpty() ? LOCTEXT("GearNone", "none") : FText::FromString(Name); };
	return FText::Format(LOCTEXT("GearTooltip", "Loadout: {0}\nWeapon: {1}\nRing: {2}"), OrNone(Gear.LoadoutName),
						 OrNone(Gear.WeaponName), OrNone(Gear.RingName));
}

// ========================================
// B-391c: DURABILITY HEADER (ported from UDurabilityHeaderWidget::RefreshForActor)
// ========================================

namespace
{
	bool BattleHUDIsUsableRing(const FRingLoadoutEntry &Entry)
	{
		return Entry.IsValid() && !Entry.RingEntry.AttachedItem.IsEmpty() && !Entry.RingEntry.AttachedItem.IsBroken();
	}
}

FBattleDurability BattleHUDView::ReadDurability(const AActor *Actor)
{
	FBattleDurability Durability;
	const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return Durability;
	}
	const FCombatLoadout &Active = Loadout->GetActiveLoadout();
	Durability.bHasRing = Active.RingLoadout.ContainsByPredicate(BattleHUDIsUsableRing) ||
						  (Active.HasPrimaryRing() && BattleHUDIsUsableRing(Active.PrimaryRing));
	if (Durability.bHasRing)
	{
		// Difference from the old header: it read GetActiveRing only, which answers for a Resonator alone since B-304,
		// so a Generic / Caster ring showed "AD:0/0". Their ring is the primary one.
		URingData *Ring = Loadout->GetActiveRing();
		if (!Ring)
		{
			Ring = Loadout->GetPrimaryRing();
		}
		const FRuntimeAttachedItem Attachment = Ring ? Loadout->GetCrystalEntryByHolder(Ring) : FRuntimeAttachedItem();
		if (!Attachment.IsEmpty())
		{
			Durability.RingCurrent = Attachment.GetCurrentDurability();
			Durability.RingMax = Attachment.GetMaxDurability();
		}
	}
	// The weapon's runtime attachment: the asset-side entry carries no durability (as the old header).
	if (UWeaponData *Weapon = Loadout->GetActiveWeapon())
	{
		const FRuntimeAttachedItem Attachment = Loadout->GetCrystalEntryByHolder(Weapon);
		Durability.bHasWeapon = !Attachment.IsEmpty() && !Attachment.IsBroken();
		if (Durability.bHasWeapon)
		{
			Durability.WeaponCurrent = Attachment.GetCurrentDurability();
			Durability.WeaponMax = Attachment.GetMaxDurability();
		}
	}
	return Durability;
}

TArray<FText> BattleHUDView::MakeDurabilityLines(const FBattleDurability &Durability)
{
	TArray<FText> Lines;
	if (Durability.bHasRing)
	{
		Lines.Add(FormatPool(LOCTEXT("RingDurability", "AD"), Durability.RingCurrent, Durability.RingMax));
	}
	if (Durability.bHasWeapon)
	{
		Lines.Add(FormatPool(LOCTEXT("WeaponDurability", "WD"), Durability.WeaponCurrent, Durability.WeaponMax));
	}
	return Lines;
}

#undef LOCTEXT_NAMESPACE
