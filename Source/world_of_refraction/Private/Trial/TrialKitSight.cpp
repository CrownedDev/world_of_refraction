// TrialKitSight.cpp

#include "Trial/TrialKitSight.h"

#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Encounter/EncounterComponent.h"
#include "Combat/Encounter/EncounterData.h"
#include "Combat/Encounter/EnemyZone.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Equipment/EquipmentDataBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Trial/EnemyKitRoll.h"
#include "Trial/TrialRunSubsystem.h"
#include "UI/Trial/EnemyKitCardWidget.h"
#include "WoRGameNames.h"
#include "WoRLog.h"

#define LOCTEXT_NAMESPACE "TrialKitSight"

namespace
{
	constexpr int32 KIT_SIGHT_FROM_SKILLS = -1;

	int32 GForcedKitSight = KIT_SIGHT_FROM_SKILLS;
#if !UE_BUILD_SHIPPING
	FAutoConsoleVariableRef CVarForcedKitSight(
		TEXT("wor.Trial.KitSight"), GForcedKitSight,
		TEXT("DEBUG (B-572): -1 (default) reads the variation from the character's known skills; 0 none, 1 the field variation, "
			 "2 always on. For PIE while no skill asset carries KitSight."));
#endif

	template <class TSubsystem>
	TSubsystem *KitSightSubsystem(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<TSubsystem>() : nullptr;
	}

	/** The forced variation (wor.Trial.KitSight), else what the character knows. */
	EKitSight GetSightOf(const UWorld *World, const UCharacterDataComponent *CharComp)
	{
		if (GForcedKitSight != KIT_SIGHT_FROM_SKILLS)
		{
			return static_cast<EKitSight>(FMath::Clamp(GForcedKitSight, 0, static_cast<int32>(EKitSight::AlwaysOn)));
		}
		const UPoolSubsystem *Pool = KitSightSubsystem<UPoolSubsystem>(World);
		return Pool && CharComp ? TrialKitSight::GetKnownSight(*Pool, UPoolSubsystem::GetCharacterKey(CharComp)) : EKitSight::None;
	}

	const TCHAR *KitSightName(EKitSight Sight)
	{
		switch (Sight)
		{
		case EKitSight::Field: return TEXT("the field variation");
		case EKitSight::AlwaysOn: return TEXT("always on");
		default: return TEXT("none");
		}
	}
}

EKitSight TrialKitSight::Strongest(EKitSight A, EKitSight B)
{
	return static_cast<uint8>(A) >= static_cast<uint8>(B) ? A : B;
}

EKitSight TrialKitSight::GetKnownSight(const UPoolSubsystem &Pool, FName CharacterKey)
{
	EKitSight Sight = EKitSight::None;
	if (const FPoolSkills *Skills = Pool.FindSkills(CharacterKey))
	{
		for (const FSpellInstance &Known : Skills->Spells)
		{
			Sight = Known.Spell ? Strongest(Sight, Known.Spell->KitSight) : Sight;
		}
		for (const FAbilityInstance &Known : Skills->Abilities)
		{
			Sight = Known.Ability ? Strongest(Sight, Known.Ability->KitSight) : Sight;
		}
	}
	return Sight;
}

FEnemyKitCard TrialKitSight::MakeCard(const UCharacterData &Enemy, const FRolledEnemyKit &Kit, bool bFull)
{
	FEnemyKitCard Card;
	// TODO: character, gear and skill names are FString assets; FText with B-127 (D-061).
	Card.Name = FText::FromString(Enemy.Name);
	Card.ClassLine = FText::Format(LOCTEXT("ClassLine", "{0}  ·  Mind {1}  ·  Body {2}  ·  Spirit {3}"), WoRGameNames::GetCharacterClassName(Enemy.CharacterClass),
								   FText::AsNumber(Enemy.WorldMindLevel), FText::AsNumber(Enemy.WorldBodyLevel), FText::AsNumber(Enemy.WorldSpiritLevel));
	for (const FRolledKitPiece &Piece : Kit.Pieces)
	{
		if (Piece.Piece && (bFull || Piece.bAlwaysHas))
		{
			Card.Pieces.Add(FText::Format(LOCTEXT("PieceLine", "{0}  ·  {1}  ·  {2} slots"), FText::FromString(Piece.Piece->Name),
										  WoRGameNames::GetTierLetter(Piece.Tier), FText::AsNumber(EnemyKitRoll::GetPieceSlots(Piece))));
		}
	}
	if (bFull)
	{
		for (const UAbilityData *Ability : Kit.Abilities)
		{
			if (Ability)
			{
				Card.Skills.Add(FText::FromString(Ability->Name));
			}
		}
		for (const USpellData *Spell : Kit.Spells)
		{
			if (Spell)
			{
				Card.Skills.Add(FText::FromString(Spell->Name));
			}
		}
	}
	return Card;
}

FText TrialKitSight::GetFieldRefusal(EKitSight Sight, int32 CurrentEP, float Distance)
{
	if (Sight == EKitSight::None)
	{
		return LOCTEXT("NoSight", "You don't know how to read an enemy's kit.");
	}
	if (Distance > FIELD_RANGE)
	{
		return LOCTEXT("TooFar", "No enemy close enough to read.");
	}
	if (CurrentEP < FIELD_EP_COST)
	{
		return FText::Format(LOCTEXT("NoEP", "Not enough EP ({0} needed)."), FText::AsNumber(FIELD_EP_COST));
	}
	return FText::GetEmpty();
}

FText TrialKitSight::UseOnNearestEnemy(APlayerController *PC)
{
	UWorld *World = PC ? PC->GetWorld() : nullptr;
	const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	UTrialRunSubsystem *TrialRun = KitSightSubsystem<UTrialRunSubsystem>(World);
	if (!CharComp || !TrialRun || !TrialRun->GetActiveTrial())
	{
		return LOCTEXT("NotInTrial", "There is no enemy to read here.");
	}
	// The nearest encounter still standing on this map.
	const UEncounterComponent *Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UEncounterComponent *Encounter = It->FindComponentByClass<UEncounterComponent>();
		if (!Encounter || TrialRun->IsEncounterBeaten(Encounter->GetEncounterId()))
		{
			continue;
		}
		const float Distance = static_cast<float>(FVector::Dist(It->GetActorLocation(), Pawn->GetActorLocation()));
		if (Distance < NearestDistance)
		{
			Nearest = Encounter;
			NearestDistance = Distance;
		}
	}
	// The team the fight builds (UEncounterComponent::GetOwnTeam): the roster, else the owner's own character.
	const TArray<UCharacterData *> Team = Nearest ? Nearest->GetOwnTeam() : TArray<UCharacterData *>();
	const FText Refusal = GetFieldRefusal(GetSightOf(World, CharComp), CharComp->CurrentEP, NearestDistance);
	if (!Refusal.IsEmpty())
	{
		return Refusal;
	}
	if (Team.IsEmpty() || !Team[0])
	{
		return LOCTEXT("NoKit", "There is nothing to read on that enemy.");
	}
	if (!Pawn->HasAuthority())
	{
		// TODO: co-op (D-103): a guest's use needs a server call to pay its EP; host only for now.
		return LOCTEXT("HostOnly", "Only the host can read an enemy's kit for now.");
	}
	// The run's roll for that encounter fought on its own, from where the enemy stands (as wor.Trial.PrintEnemyKits).
	// shortcut: a fight that catches a neighbouring encounter too, or starts across a zone border, rolls another key or
	// zone and so another kit (Q-572); roll with the fight's caught set when the map can know it.
	const FEncounterZoneInfo Zone = AEnemyZone::FindZoneAt(World, Nearest->GetOwner()->GetActorLocation());
	const TArray<FRolledEnemyKit> Kits = EnemyKitRoll::RollTeam(Team, EnemyKitRoll::MakeInput(TrialRun->GetActiveTrial(), Zone), TrialRun->GetRunSeed(),
																EnemyKitRoll::MakeEncounterKey({Nearest->GetEncounterId()}));
	if (!Kits.IsValidIndex(0))
	{
		return LOCTEXT("NoKitRolled", "There is nothing to read on that enemy.");
	}
	CharComp->ServerSpendEnergy(FIELD_EP_COST);
	// The map pawn's vitals are read from the party session at each level load and only a fight writes them back, so
	// the cost is written through here or the next fight would start with the EP unspent.
	if (UPartySessionSubsystem *PartySession = KitSightSubsystem<UPartySessionSubsystem>(World))
	{
		PartySession->SetMemberVitals(PartySession->GetMemberSlotOf(CharComp), CharComp->CurrentHP, CharComp->CurrentEP);
	}
	TrialRun->MarkKitTypeSeen(UPoolSubsystem::GetCharacterKey(Team[0]));
	// shortcut: the card shows the encounter's first enemy; a team's others are in the log until the card can page.
	const FEnemyKitCard Card = MakeCard(*Team[0], Kits[0], /*bFull=*/true);
	UE_LOG(LogWoR, Log, TEXT("[KitSight] %s read (%d EP): %s"), *GetNameSafe(Nearest->GetOwner()), FIELD_EP_COST, *WoRTrialPoolLook::GetKitCardString(Card));
	for (int32 Index = 1; Index < Team.Num() && Index < Kits.Num(); ++Index)
	{
		if (Team[Index])
		{
			UE_LOG(LogWoR, Log, TEXT("[KitSight]   also there: %s"), *WoRTrialPoolLook::GetKitCardString(MakeCard(*Team[Index], Kits[Index], /*bFull=*/true)));
		}
	}
	UEnemyKitCardWidget::Open(PC, Card);
	return FText::GetEmpty();
}

FString TrialKitSight::GetKitSightString(const UWorld *World)
{
	const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn *Pawn = PC ? PC->GetPawn() : nullptr;
	const UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
	const UTrialRunSubsystem *TrialRun = KitSightSubsystem<UTrialRunSubsystem>(World);
	return FString::Printf(TEXT("[KitSight] %s: %s%s; field use %d EP within %.0f cm; always on within %.0f cm; %d enemy type(s) seen this run"),
						   *GetNameSafe(Pawn), KitSightName(GetSightOf(World, CharComp)), GForcedKitSight != KIT_SIGHT_FROM_SKILLS ? TEXT(" (forced by wor.Trial.KitSight)") : TEXT(""),
						   TrialKitSight::FIELD_EP_COST, TrialKitSight::FIELD_RANGE, TrialKitSight::ALWAYS_ON_RANGE, TrialRun ? TrialRun->GetSeenKitTypeCount() : 0);
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GInspectEnemyCommand(
	TEXT("wor.Trial.InspectEnemy"),
	TEXT("PLACEHOLDER (B-572): use the kit-sight field variation on the nearest enemy on the trial map (no key bound yet, Q-572)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		const FText Refusal = TrialKitSight::UseOnNearestEnemy(World ? World->GetFirstPlayerController() : nullptr);
		UE_CLOG(!Refusal.IsEmpty(), LogWoR, Display, TEXT("[KitSight] %s"), *Refusal.ToString());
	}));

static FAutoConsoleCommandWithWorld GPrintKitSightCommand(
	TEXT("wor.Trial.PrintKitSight"),
	TEXT("DEBUG (B-572): the kit-sight variation the player's character has, its cost and ranges, and how many enemy types were seen this run."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
	{
		UE_LOG(LogWoR, Display, TEXT("%s"), *TrialKitSight::GetKitSightString(World));
	}));
#endif

#undef LOCTEXT_NAMESPACE
