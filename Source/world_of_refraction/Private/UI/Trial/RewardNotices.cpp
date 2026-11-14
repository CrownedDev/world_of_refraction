// RewardNotices.cpp

#include "UI/Trial/RewardNotices.h"

#include "Currency/CurrencyTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "UI/Menus/MenuScreenStyle.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "WoRRewardNotice"

namespace RewardNoticeStyle
{
	// The mockup's kind colours (run/run_end/make.py): Prisms steel, a level cyan, a memory gold, a secret quest Void's
	// violet. A visible quest takes the menu's own accent.
	inline const FLinearColor MemoryColor = FLinearColor::FromSRGBColor(FColor(240, 200, 90));
	inline const FLinearColor SecretQuestColor = FLinearColor::FromSRGBColor(FColor(150, 90, 220));
}

FRewardNotice WoRRewardNotice::MakeWin(int32 RunPrisms)
{
	FRewardNotice Notice;
	Notice.Kind = ERewardNoticeKind::Win;
	Notice.Head = LOCTEXT("WinHead", "Victory");
	if (RunPrisms > 0)
	{
		Notice.Body = FText::Format(LOCTEXT("WinBody", "+{0} {1}"), FText::AsNumber(RunPrisms), WoRGameNames::GetCurrencyName(ECurrencyType::Gold));
	}
	return Notice;
}

FRewardNotice WoRRewardNotice::MakeLevels(int32 Levels)
{
	FRewardNotice Notice;
	Notice.Kind = ERewardNoticeKind::Level;
	Notice.Head = LOCTEXT("LevelHead", "Level banked");
	Notice.Body = FText::Format(LOCTEXT("LevelBody", "+{0} {0}|plural(one=pillar level,other=pillar levels) \u00B7 place {0}|plural(one=it,other=them) at a rest spot"),
								Levels);
	return Notice;
}

FRewardNotice WoRRewardNotice::MakeMemory(const FText &Title)
{
	FRewardNotice Notice;
	Notice.Kind = ERewardNoticeKind::Memory;
	Notice.Head = LOCTEXT("MemoryHead", "New memory");
	Notice.Body = Title;
	return Notice;
}

FRewardNotice WoRRewardNotice::MakeQuest(const FText &Notice, bool bSecret)
{
	FRewardNotice Made;
	Made.Kind = bSecret ? ERewardNoticeKind::SecretQuest : ERewardNoticeKind::Quest;
	Made.Head = bSecret ? LOCTEXT("SecretQuestHead", "Secret quest") : LOCTEXT("QuestHead", "Quest");
	Made.Body = Notice;
	Made.bOnlyYou = bSecret;
	return Made;
}

FLinearColor WoRRewardNotice::GetColor(ERewardNoticeKind Kind)
{
	switch (Kind)
	{
	case ERewardNoticeKind::Level:
		return WoRMenuStyle::CyanColor;
	case ERewardNoticeKind::Memory:
		return RewardNoticeStyle::MemoryColor;
	case ERewardNoticeKind::Quest:
		return WoRMenuStyle::TextColor;
	case ERewardNoticeKind::SecretQuest:
		return RewardNoticeStyle::SecretQuestColor;
	case ERewardNoticeKind::Win:
	default:
		return WoRMenuStyle::DimColor;
	}
}

void WoRRewardNotice::SortForShow(TArray<FRewardNotice> &Notices)
{
	Notices.StableSort([](const FRewardNotice &A, const FRewardNotice &B)
	{
		return static_cast<uint8>(A.Kind) < static_cast<uint8>(B.Kind);
	});
}

void WoRRewardNotice::Post(const UObject *WorldContext, const FRewardNotice &Notice)
{
	const UWorld *World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		// A game-instance subsystem (the quest system) between worlds: its own game instance.
		const UGameInstanceSubsystem *Subsystem = Cast<UGameInstanceSubsystem>(WorldContext);
		GameInstance = Subsystem ? Subsystem->GetGameInstance() : nullptr;
	}
	if (URewardNoticeSubsystem *Notices = GameInstance ? GameInstance->GetSubsystem<URewardNoticeSubsystem>() : nullptr)
	{
		Notices->Post(Notice);
	}
}

FString WoRRewardNotice::GetNoticeString(const FRewardNotice &Notice)
{
	return FString::Printf(TEXT("[%d] %s: %s%s"), static_cast<int32>(Notice.Kind), *Notice.Head.ToString(), *Notice.Body.ToString(),
						   Notice.bOnlyYou ? TEXT(" (only you see this)") : TEXT(""));
}

void URewardNoticeSubsystem::Initialize(FSubsystemCollectionBase &Collection)
{
	Super::Initialize(Collection);
	if (UMemoryJournalSubsystem *Journal = Collection.InitializeDependency<UMemoryJournalSubsystem>())
	{
		Journal->OnMemoryRecorded.AddDynamic(this, &URewardNoticeSubsystem::HandleMemoryRecorded);
	}
}

void URewardNoticeSubsystem::HandleMemoryRecorded(UMemoryData *Memory, EMemorySource Source)
{
	// A quest's end memory: UHiddenQuestSubsystem::ShowNotice posts it (a secret quest's stays masked, Q-397b).
	if (Memory && Source != EMemorySource::HiddenQuest)
	{
		Post(WoRRewardNotice::MakeMemory(Memory->Title));
	}
}

void URewardNoticeSubsystem::Post(const FRewardNotice &Notice)
{
	if (Waiting.Num() >= WoRRewardNotice::MAX_WAITING)
	{
		UE_LOG(LogWoRUI, Warning, TEXT("[RewardNotice] %d waiting: the oldest is dropped"), Waiting.Num());
		Waiting.RemoveAt(0);
	}
	Waiting.Add(Notice);
	UE_LOG(LogWoRUI, Verbose, TEXT("[RewardNotice] Posted %s"), *WoRRewardNotice::GetNoticeString(Notice));
	OnPosted.Broadcast();
}

TArray<FRewardNotice> URewardNoticeSubsystem::TakeWaiting()
{
	TArray<FRewardNotice> Taken = MoveTemp(Waiting);
	Waiting.Reset();
	WoRRewardNotice::SortForShow(Taken);
	return Taken;
}

FString URewardNoticeSubsystem::GetWaitingString() const
{
	TArray<FString> Lines;
	for (const FRewardNotice &Notice : Waiting)
	{
		Lines.Add(WoRRewardNotice::GetNoticeString(Notice));
	}
	return FString::Printf(TEXT("[RewardNotice] %d waiting%s%s"), Waiting.Num(), Lines.IsEmpty() ? TEXT("") : TEXT(": "),
						   *FString::Join(Lines, TEXT(" / ")));
}

#if !UE_BUILD_SHIPPING
namespace RewardNoticeDebug
{
	/** The sample popups the console command posts, one per kind (the mockup's). */
	constexpr int32 SAMPLE_PRISMS = 180;
	constexpr int32 SAMPLE_LEVELS = 1;

	static FRewardNotice MakeSample(int32 Kind)
	{
		switch (Kind)
		{
		case 1:
			return WoRRewardNotice::MakeLevels(SAMPLE_LEVELS);
		case 2:
			return WoRRewardNotice::MakeMemory(LOCTEXT("SampleMemory", "The Smith's Promise"));
		case 3:
			return WoRRewardNotice::MakeQuest(LOCTEXT("SampleQuest", "New quest: Ash on the Anvil"), /*bSecret=*/false);
		case 4:
			return WoRRewardNotice::MakeQuest(LOCTEXT("SampleSecret", "Secret quest found. Open the journal to view it."), /*bSecret=*/true);
		default:
			return WoRRewardNotice::MakeWin(SAMPLE_PRISMS);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GRewardNoticePostCommand(
	TEXT("wor.UI.RewardNotice"),
	TEXT("DEBUG (B-465b): posts a sample reward popup. Arg: kind 0 win, 1 level, 2 memory, 3 quest, 4 secret quest; "
		 "no arg posts one of each."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString> &Args, UWorld *World)
	{
		if (Args.IsEmpty())
		{
			for (int32 Kind = 0; Kind <= static_cast<int32>(ERewardNoticeKind::SecretQuest); ++Kind)
			{
				WoRRewardNotice::Post(World, RewardNoticeDebug::MakeSample(Kind));
			}
			return;
		}
		WoRRewardNotice::Post(World, RewardNoticeDebug::MakeSample(FCString::Atoi(*Args[0])));
	}));

static FAutoConsoleCommandWithWorld GRewardNoticePrintCommand(
	TEXT("wor.UI.PrintRewardNotices"),
	TEXT("DEBUG (B-465b): logs the reward popups waiting for the field's HUD."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		if (const URewardNoticeSubsystem *Notices = GameInstance ? GameInstance->GetSubsystem<URewardNoticeSubsystem>() : nullptr)
		{
			UE_LOG(LogWoRUI, Display, TEXT("%s"), *Notices->GetWaitingString());
		}
	}));
#endif

#undef LOCTEXT_NAMESPACE
