// CreditsScreenModel.cpp

#include "UI/Menus/CreditsScreenModel.h"

#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRCredits"

FText WoRCredits::GetGameTitle()
{
	return LOCTEXT("GameTitle", "World of Refraction");
}

TArray<FCreditsSection> WoRCredits::BuildSections()
{
	// Names of people, packs and products are not translated (INVTEXT); the headings and the licence words are.
	TArray<FCreditsSection> Sections;
	Sections.Add({LOCTEXT("CreatedBy", "Created by"), {INVTEXT("Crown")}});
	Sections.Add({LOCTEXT("UIArt", "UI art"), {INVTEXT("Hidris")}});
	Sections.Add({LOCTEXT("Fonts", "Fonts"), {LOCTEXT("FontsLine", "Spectral SC, Lato (SIL Open Font License)")}});
	// SourceArt/UI/Title/CREDITS.md: the vortex frames and the title rule.
	Sections.Add({LOCTEXT("Assets", "Assets"),
				  {INVTEXT("HD Spell VFX: ARCLIGHT  ·  godboyhappy"), INVTEXT("Classic Fantasy RPG UI Kit  ·  PONETI")}});
	Sections.Add({LOCTEXT("BuiltWith", "Built with"), {INVTEXT("Unreal Engine")}});
	return Sections;
}

float WoRCredits::GetScrollStep(float DeltaSeconds, bool bFaster)
{
	return DeltaSeconds * SCROLL_SPEED * (bFaster ? FASTER_SCALE : 1.0f);
}

FString WoRCredits::GetCreditsString()
{
	FString Out = FString::Printf(TEXT("[Credits] %s"), *GetGameTitle().ToString());
	for (const FCreditsSection &Section : BuildSections())
	{
		Out += FString::Printf(TEXT("\n  %s:"), *Section.Heading.ToString());
		for (const FText &Line : Section.Lines)
		{
			Out += FString::Printf(TEXT("\n    %s"), *Line.ToString());
		}
	}
	return Out;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand GCreditsPrintCommand(
	TEXT("wor.UI.PrintCredits"),
	TEXT("B-470: log what the credits say."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UE_LOG(LogWoRUI, Log, TEXT("%s"), *WoRCredits::GetCreditsString());
	}));
#endif

#undef LOCTEXT_NAMESPACE
