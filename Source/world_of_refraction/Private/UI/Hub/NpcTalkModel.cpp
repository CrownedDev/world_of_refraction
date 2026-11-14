// NpcTalkModel.cpp

#include "UI/Hub/NpcTalkModel.h"

#include "Memory/MemoryJournalSubsystem.h"
#include "Story/StoryLinesData.h"
#include "WoRGameNames.h"

#define LOCTEXT_NAMESPACE "WoRNpcTalk"

namespace NpcTalkBuild
{
	FText TopicName(const FVLine &Line, int32 Number)
	{
		// TODO: Crown names each line's topic (FVLine::TopicName, the story's words); until then a plain number.
		return Line.TopicName.IsEmpty() ? FText::Format(LOCTEXT("TopicNumbered", "Topic {0}"), FText::AsNumber(Number)) : Line.TopicName;
	}
}

FNpcTalkPage WoRNpcTalk::Build(const UMerchantData *Merchant, const TArray<UStoryLinesData *> &Story, const UMemoryJournalSubsystem *Journal)
{
	FNpcTalkPage Page;
	Page.Role = Merchant ? WoRGameNames::GetMerchantTypeName(Merchant->MerchantType) : LOCTEXT("RoleV", "V");
	// TODO: V's greetings are Crown's to write (lore); a neutral placeholder until then.
	Page.Greeting = LOCTEXT("GreetingPlaceholder", "…");
	if (Merchant)
	{
		Page.Choices.Add(ENpcTalkChoice::Shop);
	}
	Page.Choices.Add(ENpcTalkChoice::Talk);
	Page.Choices.Add(ENpcTalkChoice::Leave);
	if (!Journal)
	{
		return Page;
	}

	// The heard lines (to hear again), then the next one unlocked, NEW: V says them in order (StoryUnlocks).
	const FVLine *Next = StoryUnlocks::FindNextVLine(Story, *Journal);
	int32 Number = 0;
	for (const UStoryLinesData *Data : Story)
	{
		if (!Data)
		{
			continue;
		}
		for (const FVLine &Line : Data->VLines)
		{
			const bool bHeard = Journal->HasHeardVLine(Line.LineId);
			if (!bHeard && &Line != Next)
			{
				continue;
			}
			FNpcTalkTopic &Topic = Page.Topics.AddDefaulted_GetRef();
			Topic.LineId = Line.LineId;
			Topic.Name = NpcTalkBuild::TopicName(Line, ++Number);
			Topic.bNew = !bHeard;
			Topic.Line = Line.Text;
			if (bHeard)
			{
				Page.Log.Add(Line.Text);
			}
		}
	}
	Page.bTalkHasNew = Next != nullptr;
	return Page;
}

void WoRNpcTalk::Hear(UMemoryJournalSubsystem &Journal, const FNpcTalkTopic &Topic)
{
	if (!Topic.LineId.IsNone())
	{
		Journal.MarkVLineHeard(Topic.LineId);
	}
}

FText WoRNpcTalk::GetChoiceName(ENpcTalkChoice Choice)
{
	switch (Choice)
	{
	case ENpcTalkChoice::Shop:
		return LOCTEXT("ChoiceShop", "Shop");
	case ENpcTalkChoice::Talk:
		return LOCTEXT("ChoiceTalk", "Talk");
	case ENpcTalkChoice::Leave:
	default:
		return LOCTEXT("ChoiceLeave", "Leave");
	}
}

FString WoRNpcTalk::GetTalkString(const FNpcTalkPage &Page)
{
	TArray<FString> Choices;
	for (const ENpcTalkChoice Choice : Page.Choices)
	{
		Choices.Add(GetChoiceName(Choice).ToString());
	}
	FString Out = FString::Printf(TEXT("%s: %s [%s]%s"), *Page.Role.ToString(), *Page.Greeting.ToString(), *FString::Join(Choices, TEXT(" / ")),
								  Page.bTalkHasNew ? TEXT(" (Talk: NEW)") : TEXT(""));
	for (const FNpcTalkTopic &Topic : Page.Topics)
	{
		Out += FString::Printf(TEXT("\n  %s%s (%s)"), *Topic.Name.ToString(), Topic.bNew ? TEXT(" NEW") : TEXT(""), *Topic.LineId.ToString());
	}
	Out += FString::Printf(TEXT("\n  log: %d line(s)"), Page.Log.Num());
	return Out;
}

#undef LOCTEXT_NAMESPACE
