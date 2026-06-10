// ResistanceProfileDebug.cpp

#include "Combat/Resistance/ResistanceProfileDebug.h"
#include "Combat/Resistance/ClassInnateResistanceTable.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"

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

	// Design-time BD = character-created (InnateElement == BrokenDarkness); the
	// runtime transform flag is not an asset concern.
	const bool bAssetBrokenDarkness = Character->InnateElement == ESpellElement::BrokenDarkness;
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
