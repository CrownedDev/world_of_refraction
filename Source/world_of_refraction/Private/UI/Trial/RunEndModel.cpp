// RunEndModel.cpp

#include "UI/Trial/RunEndModel.h"

#include "Currency/CurrencyTypes.h"
#include "Trial/DefeatRules.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRRunEnd"

FWipeCard WoRRunEnd::BuildWipeCard(const WoRDefeat::FWipeResult &Wipe, bool bHasRestStand)
{
	FWipeCard Card;
	Card.Title = LOCTEXT("WipeTitle", "Your possibilities end here"); // D-188
	if (!Wipe.LastBlowBy.IsEmpty())
	{
		Card.LastBlowLine = FText::Format(LOCTEXT("LastBlow", "Felled by {0}."), Wipe.LastBlowBy); // D-187: the name, never a rank
	}
	if (Wipe.PrismsDropped > 0)
	{
		Card.Lines.Add(FText::Format(LOCTEXT("PrismsDropped", "{0} {1} dropped where you fell. Go back for them."), FText::AsNumber(Wipe.PrismsDropped),
									 WoRGameNames::GetCurrencyName(ECurrencyType::Gold)));
	}
	if (Wipe.PrismsLostForGood > 0)
	{
		Card.Lines.Add(FText::Format(LOCTEXT("PrismsLost", "The {0} {1} you dropped before are lost for good."), FText::AsNumber(Wipe.PrismsLostForGood),
									 WoRGameNames::GetCurrencyName(ECurrencyType::Gold)));
	}
	Card.bRunEnds = Wipe.bRunEnds;
	if (Wipe.bRunEnds)
	{
		Card.Lines.Add(LOCTEXT("RunEnds", "No second chances are left: the trial ends."));
	}
	else
	{
		Card.Lines.Add(bHasRestStand ? LOCTEXT("WakeRest", "You wake at the last place you rested.") : LOCTEXT("WakeStart", "You wake where the trial began."));
		Card.Lines.Add(LOCTEXT("EnemyStays", "The enemy is still there."));
	}
	Card.WipesUsed = FMath::Clamp(Wipe.Wipes, 0, DefeatRules::MAX_WIPES_PER_RUN);
	Card.WipesMax = DefeatRules::MAX_WIPES_PER_RUN;
	Card.WipesLine = FText::Format(LOCTEXT("WipesLine", "Wipes {0} of {1}"), FText::AsNumber(Card.WipesUsed), FText::AsNumber(Card.WipesMax));
	Card.ButtonText = LOCTEXT("GetUp", "Get up");
	return Card;
}

FText WoRRunEnd::FormatPayoutLine(const FRunPayout &Payout)
{
	TArray<FText> Parts;
	const auto Add = [&Parts](int32 Amount, ECurrencyType Currency)
	{
		if (Amount > 0)
		{
			Parts.Add(FText::Format(LOCTEXT("PayoutPart", "+{0} {1}"), FText::AsNumber(Amount), WoRGameNames::GetCurrencyName(Currency)));
		}
	};
	Add(Payout.HubGold, ECurrencyType::Prisms); // the hub's Gold (D-159)
	Add(Payout.GearEssence, ECurrencyType::GearEssence);
	Add(Payout.SkillEssence, ECurrencyType::SkillEssence);
	// TODO: B-465c: the memories, quests and upgrades the run gave (D-187's list) once the run records them.
	return Parts.IsEmpty() ? LOCTEXT("PayoutNothing", "The run gave nothing.") : FText::Join(LOCTEXT("PayoutSeparator", " \u00B7 "), Parts);
}

FRunEndPage WoRRunEnd::BuildRunEnd(ERunEnding Ending, const FRunPayout &Payout)
{
	FRunEndPage Page;
	Page.Ending = Ending;
	Page.bCleared = Ending == ERunEnding::BossBeaten;
	Page.Title = Page.bCleared ? LOCTEXT("TitleCleared", "Trial cleared") : LOCTEXT("TitleEnds", "The trial ends");
	switch (Ending)
	{
	case ERunEnding::BossBeaten:
		Page.ShareLine = LOCTEXT("ShareFull", "Full payout");
		break;
	case ERunEnding::Extracted:
		Page.ShareLine = LOCTEXT("ShareHalf", "Half payout, and the run's upgrades kept");
		break;
	case ERunEnding::OutOfWipes:
		// TODO: B-451: GetRunEndPayout still pays a run-ending wipe the full tally; this line holds once B-451 pays 25%.
		Page.ShareLine = LOCTEXT("ShareQuarter", "A quarter of the run's currencies, no upgrades");
		break;
	case ERunEnding::Quit:
	default:
		Page.ShareLine = LOCTEXT("ShareNone", "Quit: nothing is paid");
		break;
	}
	Page.PayoutLine = Ending == ERunEnding::Quit ? LOCTEXT("PayoutNothing", "The run gave nothing.") : FormatPayoutLine(Payout);
	Page.ButtonText = LOCTEXT("Return", "Return");
	return Page;
}

FString WoRRunEnd::GetWipeCardString(const FWipeCard &Card)
{
	TArray<FString> Lines;
	for (const FText &Line : Card.Lines)
	{
		Lines.Add(Line.ToString());
	}
	return FString::Printf(TEXT("%s | %s | %s | %s | [%s]%s"), *Card.Title.ToString(), *Card.LastBlowLine.ToString(), *FString::Join(Lines, TEXT(" / ")),
						   *Card.WipesLine.ToString(), *Card.ButtonText.ToString(), Card.bRunEnds ? TEXT(" (the run ends)") : TEXT(""));
}

FString WoRRunEnd::GetRunEndString(const FRunEndPage &Page)
{
	return FString::Printf(TEXT("%s | %s | %s | [%s]"), *Page.Title.ToString(), *Page.ShareLine.ToString(), *Page.PayoutLine.ToString(),
						   *Page.ButtonText.ToString());
}

#undef LOCTEXT_NAMESPACE
