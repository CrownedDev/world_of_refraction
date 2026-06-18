// ResistanceProfileDebug.cpp

#include "Combat/Resistance/ResistanceProfileDebug.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Combat/CombatConstants.h"
#include "Skills/Effects/StatusBuildupManager.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Skills/Effects/ESkillEffectType.h"
#include "Loadout/LoadoutComponent.h"
#include "Equipment/FEquipmentStatBonus.h"
#include "Equipment/FResistanceBonus.h"
#include "Equipment/FRuntimeAttachedItem.h"
#include "Equipment/Crystals/CrystalEffectTable.h"

namespace
{
	/** Strip an enum's scope prefix for readable output (e.g.
	 *  "ESpellElement::Fire" -> "Fire"). */
	FString EnumLeaf(const FString &FullName, const TCHAR *Prefix)
	{
		FString Leaf = FullName;
		Leaf.RemoveFromStart(Prefix);
		return Leaf;
	}

	/** Build the formatted profile block for one resolved (class, innate, BD) tuple.
	 *  bDesignTime toggles the footer note about runtime divergence. */
	FString BuildProfileString(const FString &Header, ECharacterClass Class,
							   ESpellElement InnateElement, bool bIsBrokenDarkness, bool bDesignTime)
	{
		using namespace ClassInnateResistanceTable;

		// Which selection arm fires (mirrors ResolveRow's precedence exactly).
		FString Arm;
		if (bIsBrokenDarkness)
		{
			Arm = TEXT("Broken Darkness  (arm 1 — overrides class)");
		}
		else if (Class == ECharacterClass::Generic)
		{
			Arm = TEXT("Generic class row  (arm 2)");
		}
		else if (Class == ECharacterClass::Resonator)
		{
			Arm = TEXT("Resonator class row  (arm 3)");
		}
		else
		{
			Arm = FString::Printf(TEXT("Caster — %s element row  (arm 4)"),
								  *EnumLeaf(UEnum::GetValueAsString(InnateElement), TEXT("ESpellElement::")));
		}

		const FResistanceRow Row = ResolveRow(Class, InnateElement, bIsBrokenDarkness);

		FString Out;
		Out += TEXT("========== RESISTANCE PROFILE ==========\n");
		Out += FString::Printf(TEXT("%s\n"), *Header);
		Out += FString::Printf(TEXT("Class: %s   InnateElement: %s   IsBrokenDarkness: %s\n"),
							   *EnumLeaf(UEnum::GetValueAsString(Class), TEXT("ECharacterClass::")),
							   *EnumLeaf(UEnum::GetValueAsString(InnateElement), TEXT("ESpellElement::")),
							   bIsBrokenDarkness ? TEXT("YES") : TEXT("no"));
		Out += FString::Printf(TEXT("Resolved row: %s\n"), *Arm);
		Out += TEXT("--- Elements (percent, +resist / -weak) ---\n");
		Out += FString::Printf(TEXT("  Fire:      %+5.0f\n"), Row.Fire);
		Out += FString::Printf(TEXT("  Water:     %+5.0f\n"), Row.Water);
		Out += FString::Printf(TEXT("  Earth:     %+5.0f\n"), Row.Earth);
		Out += FString::Printf(TEXT("  Wind:      %+5.0f\n"), Row.Wind);
		Out += FString::Printf(TEXT("  Light:     %+5.0f\n"), Row.Light);
		Out += FString::Printf(TEXT("  Darkness:  %+5.0f   (incoming Broken Darkness aliases here)\n"), Row.Darkness);
		Out += FString::Printf(TEXT("  Lightning: %+5.0f\n"), Row.Lightning);
		Out += FString::Printf(TEXT("  Void:      %+5.0f\n"), Row.Void);
		Out += FString::Printf(TEXT("  Reality:   %+5.0f\n"), Row.Reality);
		Out += TEXT("--- Physical (percent, +resist / -weak) ---\n");
		Out += FString::Printf(TEXT("  Slash:     %+5.0f\n"), Row.Slash);
		Out += FString::Printf(TEXT("  Pierce:    %+5.0f\n"), Row.Pierce);
		Out += FString::Printf(TEXT("  Impact:    %+5.0f\n"), Row.Impact);
		Out += FString::Printf(TEXT("(combined term per hit = (element + physical) / %.0f, clamped [-1,+1] at the buildup site)\n"),
							   RESISTANCE_PERCENT_DIVISOR);
		if (bDesignTime)
		{
			Out += TEXT("NOTE: design-time view — a live Broken-Darkness transform would resolve the BD row instead.\n");
		}
		Out += TEXT("========================================\n");
		return Out;
	}
}

FString UResistanceProfileDebug::GetResistanceProfileString(UCharacterData *Character)
{
	if (!Character)
	{
		return TEXT("ERROR: Invalid Character Data");
	}

	// Design-time BD = character-created (bBrokenDarknessInnate toggle); the
	// runtime transform flag is not an asset concern.
	const bool bAssetBrokenDarkness = Character->bBrokenDarknessInnate;
	return BuildProfileString(
		FString::Printf(TEXT("Character: %s   [DESIGN-TIME]"), *Character->Name),
		Character->CharacterClass, Character->GetElement(), bAssetBrokenDarkness, /*bDesignTime=*/true);
}

FString UResistanceProfileDebug::GetActorResistanceProfileString(AActor *Actor)
{
	if (!Actor)
	{
		return TEXT("ERROR: Actor is NULL");
	}

	const UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!Comp || !Comp->CharacterData)
	{
		return FString::Printf(TEXT("ERROR: %s has no CharacterData"), *Actor->GetName());
	}

	const UCharacterData *Data = Comp->CharacterData;
	return BuildProfileString(
		FString::Printf(TEXT("Actor: %s   [RUNTIME]"), *Actor->GetName()),
		Data->CharacterClass, Data->GetElement(), Comp->IsBrokenDarkness(), /*bDesignTime=*/false);
}

void UResistanceProfileDebug::PrintResistanceProfile(AActor *Actor, float Duration, FLinearColor TextColor)
{
	const FString ProfileString = GetActorResistanceProfileString(Actor);

	if (GEngine)
	{
		// Line-split + reverse so on-screen messages read top-to-bottom
		// (mirrors UCharacterDataDebug::PrintCharacterStats).
		TArray<FString> Lines;
		ProfileString.ParseIntoArray(Lines, TEXT("\n"));
		for (int32 i = Lines.Num() - 1; i >= 0; --i)
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, TextColor.ToFColor(true), Lines[i]);
		}
	}
}

void UResistanceProfileDebug::LogResistanceProfile(UCharacterData *Character)
{
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *GetResistanceProfileString(Character));
}

FString UResistanceProfileDebug::GetCombinedResistanceString(AActor *Actor, ESpellElement Element, EPhysicalDamageType PhysicalType)
{
	if (!Actor)
	{
		return TEXT("ERROR: Actor is NULL");
	}

	UWorld *World = Actor->GetWorld();
	UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	UStatusBuildupManager *SBM = GI ? GI->GetSubsystem<UStatusBuildupManager>() : nullptr;
	if (!SBM)
	{
		return TEXT("ERROR: StatusBuildupManager unavailable");
	}

	const UCharacterDataComponent *Comp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (!Comp || !Comp->CharacterData)
	{
		return FString::Printf(TEXT("ERROR: %s has no CharacterData"), *Actor->GetName());
	}

	// Per-source terms — each read via the SAME function GetTotalStatusResistance
	// calls, so the breakdown values cannot drift from the getter.
	// 1. base Spirit (already pre-clamped to [0, RESISTANCE_MAX] inside CalculateResistance).
	const float BaseSpirit = Comp->CharacterData->CalculateResistance();

	// 2 + 3. equipment BonusResistance substat + attached ResistanceStone.
	// 6. gear per-category resistance — read via the SAME column helpers the getter
	//    uses (incl. the BD->Darkness alias), so the breakdown can't drift.
	float EquipBonus = 0.0f;
	float StonePct = 0.0f;
	float GearResist = 0.0f;
	if (ULoadoutComponent *Loadout = Actor->FindComponentByClass<ULoadoutComponent>())
	{
		const FEquipmentStatBonus Bonus = Loadout->GetActiveStatBonus(Actor);
		EquipBonus = Bonus.BonusResistance * CombatConstants::RESISTANCE_PER_POINT;
		if (const FRuntimeAttachedItem *Att = Loadout->GetActiveWeaponAttachment())
		{
			StonePct = CrystalEffectTable::GetAttachedStonePercent(*Att, ESubStat::Resistance) / CombatConstants::STAT_PERCENT_DIVISOR;
		}

		const FResistanceBonus GearBonus = Loadout->GetActiveResistanceBonus(Actor);
		const ClassInnateResistanceTable::FResistanceRow GearRow{
			GearBonus.Fire, GearBonus.Water, GearBonus.Earth, GearBonus.Wind,
			GearBonus.Light, GearBonus.Darkness, GearBonus.Lightning, GearBonus.Void,
			GearBonus.Reality, GearBonus.Slash, GearBonus.Pierce, GearBonus.Impact};
		GearResist = (ClassInnateResistanceTable::GetElementColumn(GearRow, Element) +
		              ClassInnateResistanceTable::GetPhysicalColumn(GearRow, PhysicalType)) /
		             ClassInnateResistanceTable::RESISTANCE_PERCENT_DIVISOR;
	}

	// 4. element + physical effects.  5. class/innate profile.  7. ModifyStatusResist.
	const float EffectResist = SBM->GetTotalElementResistance(Actor, Element, PhysicalType);
	const float ClassResist = ClassInnateResistanceTable::GetClassInnateResistance(Actor, Element, PhysicalType);
	float ModifyResist = 0.0f;
	if (USkillEffectManager *SEM = GI->GetSubsystem<USkillEffectManager>())
	{
		ModifyResist = SEM->GetTotalStatModifier(Actor, ESkillEffectType::ModifyStatusResist) / CombatConstants::STAT_PERCENT_DIVISOR;
	}

	const float PreClampSum = BaseSpirit + EquipBonus + StonePct + EffectResist + ClassResist + GearResist + ModifyResist;
	const float PostClampManual = FMath::Clamp(PreClampSum, CombatConstants::RESISTANCE_MIN, CombatConstants::RESISTANCE_MAX);

	// Authoritative total straight from the getter — must equal PostClampManual.
	const float Authoritative = SBM->GetTotalStatusResistance(Actor, Element, PhysicalType);
	const bool bMatch = FMath::IsNearlyEqual(PostClampManual, Authoritative, KINDA_SMALL_NUMBER);

	auto Leaf = [](const FString &Full, const TCHAR *Prefix) -> FString
	{
		FString L = Full;
		L.RemoveFromStart(Prefix);
		return L;
	};

	FString Out;
	Out += TEXT("======= COMBINED STATUS RESISTANCE =======\n");
	Out += FString::Printf(TEXT("Actor: %s   vs Element: %s   Physical: %s\n"),
						   *Actor->GetName(),
						   *Leaf(UEnum::GetValueAsString(Element), TEXT("ESpellElement::")),
						   *Leaf(UEnum::GetValueAsString(PhysicalType), TEXT("EPhysicalDamageType::")));
	Out += TEXT("--- Per-source (fraction; +resist / -weak) ---\n");
	Out += FString::Printf(TEXT("  1. base Spirit (CalculateResistance):  %+.3f\n"), BaseSpirit);
	Out += FString::Printf(TEXT("  2. equipment BonusResistance:          %+.3f\n"), EquipBonus);
	Out += FString::Printf(TEXT("  3. attached ResistanceStone:           %+.3f\n"), StonePct);
	Out += FString::Printf(TEXT("  4. element + physical effects:         %+.3f\n"), EffectResist);
	Out += FString::Printf(TEXT("  5. class/innate profile:               %+.3f\n"), ClassResist);
	Out += FString::Printf(TEXT("  6. rolled gear resistance:             %+.3f\n"), GearResist);
	Out += FString::Printf(TEXT("  7. ModifyStatusResist:                 %+.3f\n"), ModifyResist);
	Out += TEXT("  ----------------------------------------\n");
	Out += FString::Printf(TEXT("  Sum (pre-final-clamp):                 %+.3f\n"), PreClampSum);
	Out += FString::Printf(TEXT("  Clamp [%+.2f, %+.2f] ->                 %+.3f\n"),
						   CombatConstants::RESISTANCE_MIN, CombatConstants::RESISTANCE_MAX, PostClampManual);
	Out += FString::Printf(TEXT("  Getter total (authoritative):          %+.3f   [%s]\n"),
						   Authoritative, bMatch ? TEXT("MATCH") : TEXT("MISMATCH"));
	Out += FString::Printf(TEXT("  Applied to buildup: Amount x %.3f\n"), 1.0f - Authoritative);
	Out += TEXT("==========================================\n");
	return Out;
}

void UResistanceProfileDebug::PrintCombinedResistance(AActor *Actor, ESpellElement Element, EPhysicalDamageType PhysicalType,
													  float Duration, FLinearColor TextColor)
{
	const FString CombinedString = GetCombinedResistanceString(Actor, Element, PhysicalType);

	if (GEngine)
	{
		// Line-split + reverse so on-screen messages read top-to-bottom.
		TArray<FString> Lines;
		CombinedString.ParseIntoArray(Lines, TEXT("\n"));
		for (int32 i = Lines.Num() - 1; i >= 0; --i)
		{
			GEngine->AddOnScreenDebugMessage(-1, Duration, TextColor.ToFColor(true), Lines[i]);
		}
	}
}
