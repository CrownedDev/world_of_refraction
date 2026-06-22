// Fill out your copyright notice in the Description page of Project Settings.

#include "Debug/Skills/GenericSpellResolveDebug.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Character/CharacterDataComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Loadout/FCombatLoadout.h"
#include "Infusion/ElementColors.h"

namespace
{
    // Mirror SpellDataDebug's element-name convention: the enum value, ESpellElement:: stripped.
    FString ElementToStr(ESpellElement Element)
    {
        FString S = UEnum::GetValueAsString(Element);
        S.RemoveFromStart(TEXT("ESpellElement::"));
        return S;
    }

    UActionExecutor *GetExec(AActor *Caster)
    {
        if (!Caster)
        {
            return nullptr;
        }
        if (UGameInstance *GI = UGameplayStatics::GetGameInstance(Caster))
        {
            return GI->GetSubsystem<UActionExecutor>();
        }
        return nullptr;
    }

    // Reverse-order on-screen print (matches SpellDataDebug so lines read top-down).
    void EmitOnScreen(const FString &Output, float Duration, const FColor &Colour)
    {
        if (!GEngine)
        {
            return;
        }
        TArray<FString> Lines;
        Output.ParseIntoArray(Lines, TEXT("\n"));
        for (int32 i = Lines.Num() - 1; i >= 0; --i)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, Colour, Lines[i]);
        }
    }
}

FString UGenericSpellResolveDebug::GetResolveString(AActor *Caster, USpellData *Spell)
{
    if (!Caster || !Spell)
    {
        return TEXT("ERROR: Invalid Caster or Spell");
    }

    UActionExecutor *Exec = GetExec(Caster);
    UCharacterDataComponent *CDC = Caster->FindComponentByClass<UCharacterDataComponent>();
    UBrokenDarknessManager *BDManager = Caster->FindComponentByClass<UBrokenDarknessManager>();

    const bool bIsBD = CDC && CDC->IsBrokenDarkness();
    const ESpellElement Authored = Spell->Element;
    const ESpellElement Resolved = Exec ? Exec->ResolveSpellCastElement(Caster, Spell) : Authored;

    FString Output;
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("GENERIC RESOLVE: %s\n"), *Spell->Name);
    Output += TEXT("===================================\n");
    Output += FString::Printf(TEXT("  Authored Element: %s  (%s)\n"), *ElementToStr(Authored),
                              Authored == ESpellElement::Generic ? TEXT("polymorphic") : TEXT("fixed"));
    if (!Exec)
    {
        Output += TEXT("  WARNING: no ActionExecutor — resolved = authored (no resolve)\n");
    }
    Output += FString::Printf(TEXT("  Resolved Element: %s\n"), *ElementToStr(Resolved));
    Output += FString::Printf(TEXT("  Broken Darkness:  %s\n"), bIsBD ? TEXT("YES") : TEXT("no"));
    if (bIsBD && BDManager)
    {
        Output += FString::Printf(TEXT("  BD Active Pool:   %s\n"), *ElementToStr(BDManager->GetActivePool()));
    }
    Output += FString::Printf(TEXT("  Display Name:     \"%s\"\n"), *Spell->GetDisplayName(Resolved, bIsBD));

    const FLinearColor Col = ElementColors::GetColorForElement(Resolved);
    Output += FString::Printf(TEXT("  Resolved Colour:  (R=%.2f G=%.2f B=%.2f)\n"), Col.R, Col.G, Col.B);
    Output += TEXT("===================================\n");
    return Output;
}

void UGenericSpellResolveDebug::PrintResolvedElement(AActor *Caster, USpellData *Spell, float Duration)
{
    if (!Caster || !Spell)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, Duration, FColor::Red, TEXT("ERROR: Caster or Spell is NULL"));
        }
        return;
    }

    const FString Output = GetResolveString(Caster, Spell);
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *Output);
    EmitOnScreen(Output, Duration, FColor::Cyan);
}

void UGenericSpellResolveDebug::PrintBDPools(AActor *Caster, float Duration)
{
    FString Output;
    Output += TEXT("===================================\n");
    Output += TEXT("BD MODEL-B POOLS\n");
    Output += TEXT("===================================\n");

    if (!Caster)
    {
        Output += TEXT("  ERROR: Caster is NULL\n");
        Output += TEXT("===================================\n");
        UE_LOG(LogTemp, Display, TEXT("\n%s"), *Output);
        EmitOnScreen(Output, Duration, FColor::Red);
        return;
    }

    UCharacterDataComponent *CDC = Caster->FindComponentByClass<UCharacterDataComponent>();
    UBrokenDarknessManager *BDManager = Caster->FindComponentByClass<UBrokenDarknessManager>();
    ULoadoutComponent *LC = Caster->FindComponentByClass<ULoadoutComponent>();

    if (!CDC || !CDC->IsBrokenDarkness() || !BDManager)
    {
        Output += FString::Printf(TEXT("  %s is NOT Broken Darkness — no pool rotation.\n"), *Caster->GetName());
    }
    else
    {
        const ESpellElement Active = BDManager->GetActivePool();
        Output += FString::Printf(TEXT("  Active Pool: %s\n"), *ElementToStr(Active));
        Output += TEXT("  (Model-B: exactly ONE pool active at a time = the active pool above.)\n");
        Output += TEXT("  Pools:\n");

        if (LC)
        {
            const FCombatLoadout Loadout = LC->GetActiveLoadout();
            // The Darkness/base pool lives in InnateSpells (not BDSpellPools).
            Output += FString::Printf(TEXT("    Darkness (base/InnateSpells): %d spell(s)%s\n"),
                                      Loadout.InnateSpells.Num(),
                                      Active == ESpellElement::Darkness ? TEXT("  <- ACTIVE") : TEXT("  (dormant)"));
            for (const FBDElementSpellPool &Pool : Loadout.BDSpellPools)
            {
                Output += FString::Printf(TEXT("    %s pool: %d spell(s)%s\n"),
                                          *ElementToStr(Pool.Element), Pool.Spells.Num(),
                                          Pool.Element == Active ? TEXT("  <- ACTIVE") : TEXT("  (dormant)"));
            }
        }
        else
        {
            Output += TEXT("    (no LoadoutComponent — cannot list pools)\n");
        }
    }

    Output += TEXT("===================================\n");
    UE_LOG(LogTemp, Display, TEXT("\n%s"), *Output);
    EmitOnScreen(Output, Duration, FColor::Cyan);
}
