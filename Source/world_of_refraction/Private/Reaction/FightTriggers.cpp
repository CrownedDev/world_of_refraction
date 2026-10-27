// FightTriggers.cpp

#include "Reaction/FightTriggers.h"

#include "Character/CharacterDataComponent.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Boss/BossData.h"
#include "Combat/Boss/BossPhaseComponent.h"
#include "Combat/TurnManager.h"
#include "Engine/GameInstance.h"
#include "Memory/MemoryConditions.h"
#include "Memory/MemoryData.h"
#include "Memory/MemoryJournalSubsystem.h"
#include "Memory/MemoryUnlock.h"
#include "WoRLog.h"

namespace
{
	/** Percent of max HP; -1 when it can't be read. */
	float GetHPPercent(const AActor *Actor)
	{
		const UCharacterDataComponent *Stats = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return Stats && Stats->MaxHP > 0 ? 100.0f * Stats->CurrentHP / Stats->MaxHP : -1.0f;
	}

	bool IsPastLine(const FFightMoment &When, float HPPercent)
	{
		if (HPPercent < 0.0f)
		{
			return false;
		}
		return When.Moment == EFightMoment::HPBelow ? HPPercent < When.ThresholdPercent : HPPercent > When.ThresholdPercent;
	}

	bool IsHPLine(const FFightMoment &When)
	{
		return When.Moment == EFightMoment::HPBelow || When.Moment == EFightMoment::HPAbove;
	}
}

TArray<FFiredReaction> UFightTriggers::StartReactions(const TArray<FReactionTrigger> &InReactions, const TArray<AActor *> &Party,
													  const TArray<AActor *> &Opposing)
{
	// Copied once and never resized: the fired reactions point into it.
	Reactions = InReactions;
	TArray<FFiredReaction> Now;
	for (const FFiredReaction &Match : ReactionTriggers::FindFired(Reactions, Party, Opposing))
	{
		if (!GameInstance.IsValid() && Match.Wearer)
		{
			GameInstance = Match.Wearer->GetGameInstance();
		}
		if (Match.Trigger->When.IsAtFightStart())
		{
			Now.Add(Match);
			continue;
		}
		FWaitingTrigger &Entry = Waiting.AddDefaulted_GetRef();
		Entry.When = Match.Trigger->When;
		Entry.Ours.Add(Match.Wearer);
		Entry.Theirs.Add(Match.Faced);
		Entry.Reaction = Match;
		Entry.TriggerId = Match.Trigger->TriggerId;
	}
	return Now;
}

int32 UFightTriggers::StartBoss(AActor *Boss, const UBossData &Data, const TArray<AActor *> &Party)
{
	FBossFightState &State = BossStates.FindOrAdd(Boss);
	const int32 Fired = BossTriggers::ApplyTo(Boss, Data, Party, &State);
	FMemoryEvent FightStart;
	for (const AActor *Member : Party)
	{
		FightStart.Party.Add(MemoryConditions::MakeMember(Member));
	}
	for (const FBossTrigger &Trigger : Data.Triggers)
	{
		if (Trigger.When.IsAtFightStart() || !BossTriggers::Fires(Trigger, FightStart))
		{
			continue;
		}
		BossDatas.AddUnique(&Data);
		FWaitingTrigger &Entry = Waiting.AddDefaulted_GetRef();
		Entry.When = Trigger.When;
		for (AActor *Member : Party)
		{
			Entry.Ours.Add(Member);
		}
		Entry.Theirs.Add(Boss);
		Entry.Boss = Boss;
		Entry.BossTrigger = &Trigger;
		Entry.TriggerId = Trigger.TriggerId;
	}
	if (!GameInstance.IsValid() && Boss)
	{
		GameInstance = Boss->GetGameInstance();
	}
	return Fired;
}

int32 UFightTriggers::StartBosses(const TArray<AActor *> &Party, const TArray<AActor *> &Opposing)
{
	int32 Fired = 0;
	for (AActor *Enemy : Opposing)
	{
		const UCharacterDataComponent *EnemyData = Enemy ? Enemy->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (const UBossData *BossData = EnemyData ? UBossData::FindFor(EnemyData->CharacterData) : nullptr)
		{
			Fired += StartBoss(Enemy, *BossData, Party);
		}
	}
	return Fired;
}

void UFightTriggers::Watch(bool bWatch)
{
	if (bWatch == bWatching || (bWatch && Waiting.IsEmpty()))
	{
		return;
	}
	bWatching = bWatch;
	UGameInstance *GI = GameInstance.Get();
	if (UTurnManager *TurnManager = GI ? GI->GetSubsystem<UTurnManager>() : nullptr)
	{
		bWatch ? TurnManager->OnTurnStarted.AddUniqueDynamic(this, &UFightTriggers::HandleTurnStarted)
			   : TurnManager->OnTurnStarted.RemoveDynamic(this, &UFightTriggers::HandleTurnStarted);
	}
	if (UActionExecutor *Executor = GI ? GI->GetSubsystem<UActionExecutor>() : nullptr)
	{
		bWatch ? Executor->OnDamageDealt.AddUniqueDynamic(this, &UFightTriggers::HandleDamageDealt)
			   : Executor->OnDamageDealt.RemoveDynamic(this, &UFightTriggers::HandleDamageDealt);
	}
	TSet<AActor *> Combatants;
	for (FWaitingTrigger &Entry : Waiting)
	{
		for (const TArray<TWeakObjectPtr<AActor>> *Side : {&Entry.Ours, &Entry.Theirs})
		{
			for (const TWeakObjectPtr<AActor> &Subject : *Side)
			{
				if (AActor *Actor = Subject.Get())
				{
					Combatants.Add(Actor);
					// HP lines count from the HP now: a boss already below one doesn't fire it.
					if (bWatch && IsHPLine(Entry.When))
					{
						Entry.WasPastLine.Add(Subject, IsPastLine(Entry.When, GetHPPercent(Actor)));
					}
				}
			}
		}
	}
	for (AActor *Actor : Combatants)
	{
		BindCombatant(Actor, bWatch);
	}
	if (bWatch)
	{
		UE_LOG(LogWoR, Log, TEXT("[FightTriggers] Waiting:%s"), *GetFightTriggersString());
	}
}

void UFightTriggers::BindCombatant(AActor *Actor, bool bWatch)
{
	if (UCharacterDataComponent *Stats = Actor->FindComponentByClass<UCharacterDataComponent>())
	{
		bWatch ? Stats->OnHPChanged.AddUniqueDynamic(this, &UFightTriggers::HandleHPChanged)
			   : Stats->OnHPChanged.RemoveDynamic(this, &UFightTriggers::HandleHPChanged);
	}
	if (UBossPhaseComponent *Phases = Actor->FindComponentByClass<UBossPhaseComponent>())
	{
		bWatch ? Phases->OnBossPhaseReached.AddUniqueDynamic(this, &UFightTriggers::HandleBossPhaseReached)
			   : Phases->OnBossPhaseReached.RemoveDynamic(this, &UFightTriggers::HandleBossPhaseReached);
	}
}

void UFightTriggers::HandleTurnStarted(AActor *Actor, int32 /*TurnNumber*/)
{
	FireMatching(EFightMoment::TurnStart, Actor);
	// A damage-over-time tick may not report its HP change: look at the lines again.
	CheckHPLines();
}

void UFightTriggers::HandleDamageDealt(AActor *Attacker, AActor *Target, int32 /*Damage*/, bool /*bCritical*/)
{
	FireMatching(EFightMoment::Hit, Attacker);
	FireMatching(EFightMoment::DamageTaken, Target);
	CheckHPLines();
}

void UFightTriggers::HandleHPChanged(int32 /*CurrentHP*/, int32 /*MaxHP*/)
{
	CheckHPLines();
}

void UFightTriggers::HandleBossPhaseReached(AActor *Boss, int32 /*PhaseIndex*/)
{
	FireMatching(EFightMoment::BossPhase, Boss);
}

const TArray<TWeakObjectPtr<AActor>> &UFightTriggers::GetSide(const FWaitingTrigger &Entry) const
{
	// A boss phase is always the boss's (for a reaction, the faced character's).
	return Entry.When.Side == EFightMomentSide::Ours && Entry.When.Moment != EFightMoment::BossPhase ? Entry.Ours : Entry.Theirs;
}

void UFightTriggers::FireMatching(EFightMoment Moment, const AActor *Subject)
{
	if (!Subject)
	{
		return;
	}
	// Index loop: firing can change HP and re-enter CheckHPLines; entries are never added mid-fight.
	for (int32 Index = 0; Index < Waiting.Num(); ++Index)
	{
		FWaitingTrigger &Entry = Waiting[Index];
		if (!Entry.bFired && Entry.When.Moment == Moment && GetSide(Entry).Contains(Subject))
		{
			Fire(Entry);
		}
	}
}

void UFightTriggers::CheckHPLines()
{
	for (int32 Index = 0; Index < Waiting.Num(); ++Index)
	{
		FWaitingTrigger &Entry = Waiting[Index];
		if (Entry.bFired || !IsHPLine(Entry.When))
		{
			continue;
		}
		for (const TWeakObjectPtr<AActor> &Subject : GetSide(Entry))
		{
			const bool bPast = IsPastLine(Entry.When, GetHPPercent(Subject.Get()));
			bool &bWasPast = Entry.WasPastLine.FindOrAdd(Subject, bPast);
			const bool bCrossed = bPast && !bWasPast;
			bWasPast = bPast;
			if (bCrossed)
			{
				Fire(Entry);
				break;
			}
		}
	}
}

void UFightTriggers::Fire(FWaitingTrigger &Entry)
{
	// Marked first: what it does can re-enter (an HP change) and must not fire it again.
	Entry.bFired = true;
	++FiredMidFight;
	if (Entry.Reaction.Trigger)
	{
		ReactionTriggers::ApplyFired({Entry.Reaction});
		MemoryUnlock::Unlock(GameInstance.Get(), Entry.Reaction.Trigger->UnlockMemory, EMemorySource::Reaction);
	}
	else if (AActor *Boss = Entry.Boss.Get(); Boss && Entry.BossTrigger)
	{
		FBossFightState &State = BossStates.FindOrAdd(Boss);
		if (BossTriggers::ApplyResults(Boss, *Entry.BossTrigger, State))
		{
			ReactionTriggers::ApplyBossSteps(Boss, State.NetSteps, /*bBeforeFight=*/false);
		}
	}
	UE_LOG(LogWoR, Log, TEXT("[FightTriggers] %s fired mid-fight (%s)"), *Entry.TriggerId.ToString(), *Entry.When.ToString());
}

int32 UFightTriggers::GetWaitingCount() const
{
	return Waiting.FilterByPredicate([](const FWaitingTrigger &Entry) { return !Entry.bFired; }).Num();
}

FString UFightTriggers::GetFightTriggersString() const
{
	FString Out;
	for (const FWaitingTrigger &Entry : Waiting)
	{
		Out += FString::Printf(TEXT("\n  %s %s at %s%s"), Entry.Reaction.Trigger ? TEXT("Reaction") : TEXT("Boss trigger"),
							   *Entry.TriggerId.ToString(), *Entry.When.ToString(), Entry.bFired ? TEXT(" [fired]") : TEXT(""));
	}
	return Out.IsEmpty() ? FString(TEXT(" none")) : Out;
}
