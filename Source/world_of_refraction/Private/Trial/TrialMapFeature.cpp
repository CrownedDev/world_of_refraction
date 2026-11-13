// TrialMapFeature.cpp

#include "Trial/TrialMapFeature.h"
#include "Player/RunStateComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/WoRPlayerState.h"
#include "Trial/DefeatRules.h"
#include "Player/WoRInteract.h"

#include "Character/CharacterData.h"
#include "Combat/Encounter/EEncounterRank.h"
#include "Combat/Encounter/EFirstStrike.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Trial/TrialGate.h"
#include "Trial/TrialLoot.h"
#include "Trial/TrialRunSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialMapFeature"

namespace TrialMapFeatureConstants
{
	const FVector DEFAULT_INTERACTION_EXTENT(150.0f, 150.0f, 100.0f);
	constexpr float MESSAGE_SECONDS = 6.0f;
	constexpr int32 MESSAGE_KEY = -1; // a new line each time, not keyed
}

void ATrialMapFeature::ShowMessage(const FText &Message)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(TrialMapFeatureConstants::MESSAGE_KEY, TrialMapFeatureConstants::MESSAGE_SECONDS,
										 FColor::Yellow, Message.ToString());
	}
}

ATrialMapFeature::ATrialMapFeature()
{
	PrimaryActorTick.bCanEverTick = false;

	FeatureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FeatureMesh"));
	RootComponent = FeatureMesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFinder.Succeeded())
	{
		FeatureMesh->SetStaticMesh(CubeFinder.Object);
	}

	InteractionTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionTrigger"));
	InteractionTrigger->SetupAttachment(RootComponent);
	InteractionTrigger->SetBoxExtent(TrialMapFeatureConstants::DEFAULT_INTERACTION_EXTENT);
	InteractionTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionTrigger->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void ATrialMapFeature::BeginPlay()
{
	Super::BeginPlay();
	InteractionTrigger->OnComponentBeginOverlap.AddDynamic(this, &ATrialMapFeature::OnTriggerBeginOverlap);
	InteractionTrigger->OnComponentEndOverlap.AddDynamic(this, &ATrialMapFeature::OnTriggerEndOverlap);
}

void ATrialMapFeature::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	WoRInteract::HideHint(*this);
	PawnInRange.Reset();
	Super::EndPlay(EndPlayReason);
}

ATrialMapFeature *ATrialMapFeature::FindNearestInRange(APawn *Pawn)
{
	return WoRInteract::FindNearestOverlapping<ATrialMapFeature>(Pawn); // B-256
}

FName ATrialMapFeature::GetFeatureId() const
{
	return FName(*UWorld::RemovePIEPrefix(GetPathName()));
}

bool ATrialMapFeature::IsWithinReach(const APawn *Pawn) const
{
	return Pawn && InteractionTrigger && InteractionTrigger->IsOverlappingActor(Pawn);
}

namespace
{
	/** Q-187d: some player already opened this chest (the host's key is the feature id; a friend's is "id@player"). */
	bool IsOpenedByAnyone(const UTrialRunSubsystem &TrialRun, FName FeatureId)
	{
		const FString FriendPrefix = FeatureId.ToString() + TEXT("@");
		for (const TPair<FName, FText> &Found : TrialRun.GetFoundFeatures())
		{
			if (Found.Key == FeatureId || Found.Key.ToString().StartsWith(FriendPrefix))
			{
				return true;
			}
		}
		return false;
	}
}

FName ATrialMapFeature::GetFeatureIdFor(const APawn *Pawn) const
{
	// Review fix: keyed from the player state (a pawn whose controller is gone mid-travel still names its player).
	const APlayerState *Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	const APlayerController *PC = Player ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
	if (!Player || (PC && PC->IsLocalController()))
	{
		return GetFeatureId(); // the host's or solo player's (or no player at all): unchanged
	}
	return FName(*FString::Printf(TEXT("%s@%s"), *GetFeatureId().ToString(), *WoRDefeat::PlayerDropKey(Player)));
}

void ATrialMapFeature::ShowMessageTo(const APawn *Pawn, const FText &Message)
{
	const APlayerController *PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Pawn || (PC && PC->IsLocalController()))
	{
		ShowMessage(Message);
		return;
	}
	// Review: the pawn's player state, as GetFeatureIdFor reads it, so a controller missing mid-travel can't put
	// another player's message on the host's screen.
	if (AWoRPlayerState *Player = Pawn->GetPlayerState<AWoRPlayerState>())
	{
		Player->Client_ShowFeatureMessage(Message); // B-235c: on that player's own screen
	}
}

UTrialRunSubsystem *ATrialMapFeature::GetTrialRun() const
{
	const UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UTrialRunSubsystem>() : nullptr;
}

void ATrialMapFeature::RefreshHint()
{
	WoRInteract::ShowHint(*this, FColor::Cyan, PawnInRange.IsValid() ? GetHintText() : FString());
}

void ATrialMapFeature::OnTriggerBeginOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
											 UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/,
											 bool /*bFromSweep*/, const FHitResult & /*SweepResult*/)
{
	APawn *Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return;
	}
	PawnInRange = Pawn;
	RefreshHint();
}

void ATrialMapFeature::OnTriggerEndOverlap(UPrimitiveComponent * /*OverlappedComponent*/, AActor *OtherActor,
										   UPrimitiveComponent * /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (OtherActor != PawnInRange.Get())
	{
		return;
	}
	PawnInRange.Reset();
	RefreshHint();
}

// ==================== CHEST ====================

FGameplayTag ATrialChest::GetLootPool() const
{
	return LootPool.IsValid() ? LootPool : AEnemyZone::FindZoneAt(GetWorld(), GetActorLocation()).LootPool;
}

void ATrialChest::Interact(APawn *Pawn)
{
	UTrialRunSubsystem *TrialRun = GetTrialRun();
	// B-235c (D-103): loot is personal: each player opens the chest once for their own roll.
	const FName PlayerFeatureId = GetFeatureIdFor(Pawn);
	if (!TrialRun || TrialRun->HasFound(PlayerFeatureId))
	{
		return;
	}
	const FGameplayTag Pool = GetLootPool();
	// The run seed and the chest's own id (the player's, B-235c): the same chest gives the same item on a replayed
	// seed; the host's (and solo) roll is unchanged. The id is hashed as text (an FName's hash changes between launches).
	const int32 Seed = static_cast<int32>(
		HashCombine(GetTypeHash(TrialRun->GetRunSeed()), FCrc::StrCrc32(*PlayerFeatureId.ToString())));
	UObject *Item = TrialLoot::PickFromPool(TrialLoot::GatherPool(Pool), Pool, Seed);
	if (!Item)
	{
		UE_LOG(LogWoR, Warning, TEXT("[TrialChest] %s - loot pool '%s' has no items; opened empty"), *GetName(),
			   *Pool.ToString());
		TrialRun->RecordFound(PlayerFeatureId, LOCTEXT("ChestEmpty", "An empty chest"));
		ShowMessageTo(Pawn, LOCTEXT("ChestEmptyMessage", "The chest is empty."));
		RefreshHint();
		return;
	}
	if (!TrialLoot::GrantTo(Pawn, Item))
	{
		// Left closed so the player can come back with room.
		UE_LOG(LogWoR, Warning, TEXT("[TrialChest] %s - could not give %s to %s"), *GetName(), *Item->GetName(),
			   Pawn ? *Pawn->GetName() : TEXT("<none>"));
		ShowMessageTo(Pawn, LOCTEXT("ChestNoRoom", "No room for what is in the chest."));
		return;
	}
	const FText ItemName = TrialLoot::GetItemName(Item);
	// Merge B-187c + B-235c, Q-187d: B-073's chest tally counts a chest once (its first open by anyone: the run-end
	// payout already pays every player in full), then B-235c's per-player key and message.
	if (!IsOpenedByAnyone(*TrialRun, GetFeatureId()))
	{
		TrialRun->RecordChestReward(); // B-073 (D-094): chests add more to the run's tally
	}
	TrialRun->RecordFound(PlayerFeatureId, FText::Format(LOCTEXT("ChestFound", "Chest: {0}"), ItemName));
	ShowMessageTo(Pawn, FText::Format(LOCTEXT("ChestFoundMessage", "Found {0}."), ItemName));
	RefreshHint();
}

bool ATrialChest::IsOpenedForLocalPlayer() const
{
	// B-232d: the run state's copy, so a client's hint follows the run. Merge B-232d + B-235c: a client reads its own
	// copy's key ("<id>@<player key>", as GetFeatureIdFor records it on the host), not the host's.
	FName OwnKey = GetFeatureId();
	const APlayerController *LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (GetNetMode() == NM_Client && LocalPC && LocalPC->PlayerState)
	{
		OwnKey = FName(*FString::Printf(TEXT("%s@%s"), *GetFeatureId().ToString(), *WoRDefeat::PlayerDropKey(LocalPC->PlayerState)));
	}
	return URunStateComponent::HasFoundFeature(this, OwnKey);
}

FString ATrialChest::GetHintText() const
{
	return (IsOpenedForLocalPlayer() ? LOCTEXT("ChestOpenedHint", "Opened chest") : LOCTEXT("ChestHint", "Press E to open the chest"))
		.ToString();
}

FInteractPrompt ATrialChest::GetPrompt() const
{
	return IsOpenedForLocalPlayer() ? FInteractPrompt() : FInteractPrompt(EInteractVerb::Open, LOCTEXT("ChestPromptName", "Chest"));
}

// ==================== LORE ====================

ATrialLoreObject::ATrialLoreObject()
{
	Title = LOCTEXT("LoreDefaultTitle", "Weathered inscription");
	Body = LOCTEXT("LoreDefaultBody", "The words are worn away. (Placeholder lore.)");
}

void ATrialLoreObject::Interact(APawn * /*Pawn*/)
{
	ShowMessage(FText::Format(LOCTEXT("LoreMessage", "{0}: {1}"), Title, Body));
	// B-194: kept in the journal for rereading in the hub, whatever happens to the run.
	const UGameInstance *GameInstance = GetGameInstance();
	if (UMemoryJournalSubsystem *Journal = GameInstance ? GameInstance->GetSubsystem<UMemoryJournalSubsystem>() : nullptr)
	{
		Journal->RecordLore(GetFeatureId(), Title, Body);
	}
	UTrialRunSubsystem *TrialRun = GetTrialRun();
	if (TrialRun && !TrialRun->HasFound(GetFeatureId()))
	{
		TrialRun->RecordFound(GetFeatureId(), FText::Format(LOCTEXT("LoreFound", "Lore: {0}"), Title));
		RefreshHint();
	}
}

FString ATrialLoreObject::GetHintText() const
{
	return FText::Format(LOCTEXT("LoreHint", "Press E to read: {0}"), Title).ToString();
}

FInteractPrompt ATrialLoreObject::GetPrompt() const
{
	return FInteractPrompt(EInteractVerb::Read, Title);
}

// ==================== BOSS ARENA ====================

UCharacterData *ABossArenaTrigger::GetBoss() const
{
	const UTrialRunSubsystem *TrialRun = GetTrialRun();
	const FTrialVariation *Variation = TrialRun ? TrialRun->GetChosenVariation() : nullptr;
	return Variation && Variation->Boss ? Variation->Boss.Get() : FallbackBoss.Get();
}

void ABossArenaTrigger::Interact(APawn * /*Pawn*/)
{
	UTrialRunSubsystem *TrialRun = GetTrialRun();
	UCharacterData *Boss = GetBoss();
	const UGameInstance *GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
	const UParty *LocalParty = PartySession ? PartySession->GetLocalParty() : nullptr;
	const TArray<UCharacterData *> BattleParty =
		LocalParty ? LocalParty->GetLivingMemberCharacterData() : TArray<UCharacterData *>();
	if (!TrialRun || TrialRun->IsEncounterBeaten(GetFeatureId()))
	{
		return;
	}
	if (!Boss || BattleParty.Num() == 0 || !TrialRun->GetActiveTrial())
	{
		UE_LOG(LogWoR, Warning, TEXT("[BossArena] %s - not starting: %s"), *GetName(),
			   !Boss				 ? TEXT("no boss (no variation boss and no FallbackBoss)")
			   : BattleParty.Num() == 0 ? TEXT("no living party")
										: TEXT("no active trial (enter through the trial door)"));
		ShowMessage(LOCTEXT("BossNotStarted", "The boss fight can't start (see the Output Log)."));
		return;
	}

	UE_LOG(LogWoR, Log, TEXT("[BossArena] %s - challenging %s (%d in the party)"), *GetName(), *Boss->Name,
		   BattleParty.Num());
	// Same hand-off as a map encounter (UEncounterComponent): a win marks the arena beaten.
	TrialRun->SetPendingEncounterSources({GetFeatureId()});
	TrialRun->SetPendingEnemyRanks({}); // B-224b: no stale list from an earlier fight; the boss takes the fight's rank
	TrialRun->SetPendingFirstStrike(EFirstStrike::None);
	TrialRun->SetPendingZone(AEnemyZone::FindZoneAt(GetWorld(), GetActorLocation()));
	const EAIDifficulty BossAI = TrialGate::ResolveEncounterAIDifficulty(TrialRun->GetTrialDifficulty(), bOverrideAIDifficulty, Difficulty);
	TrialRun->EnterEncounter(TrialRun->GetActiveTrial(), BattleParty, {Boss}, BossAI, EEncounterRank::Boss);
	if (!TrialRun->HasPendingEncounter())
	{
		TrialRun->SetPendingEncounterSources({});
		TrialRun->SetPendingZone(FEncounterZoneInfo());
		ShowMessage(LOCTEXT("BossNotStarted", "The boss fight can't start (see the Output Log)."));
	}
}

FString ABossArenaTrigger::GetHintText() const
{
	const UTrialRunSubsystem *TrialRun = GetTrialRun();
	if (TrialRun && TrialRun->IsEncounterBeaten(GetFeatureId()))
	{
		return LOCTEXT("BossBeatenHint", "The boss is beaten").ToString();
	}
	const UCharacterData *Boss = GetBoss();
	return Boss ? FText::Format(LOCTEXT("BossHint", "Press E to challenge {0} (boss fight)"), FText::FromString(Boss->Name)).ToString()
				: LOCTEXT("BossMissingHint", "No boss is waiting here").ToString();
}

FInteractPrompt ABossArenaTrigger::GetPrompt() const
{
	const UTrialRunSubsystem *TrialRun = GetTrialRun();
	const bool bBeaten = TrialRun && TrialRun->IsEncounterBeaten(GetFeatureId());
	// D-187: no rank word; the opponent's own name says who waits here.
	const UCharacterData *Boss = GetBoss();
	return bBeaten || !Boss ? FInteractPrompt() : FInteractPrompt(EInteractVerb::Challenge, FText::FromString(Boss->Name));
}

#undef LOCTEXT_NAMESPACE
