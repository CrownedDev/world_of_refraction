// CurrencyWallet.cpp

#include "Currency/CurrencyWallet.h"

#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyComponentDebug.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"

namespace CurrencyWalletInternal
{
	UCurrencyComponent *PlayerWalletOf(const APlayerState *PlayerState)
	{
		const AWoRPlayerState *WoRState = Cast<AWoRPlayerState>(PlayerState);
		return WoRState ? WoRState->GetWallet() : nullptr;
	}

	UCurrencyComponent *PartyLeaderWalletOf(const AActor *Actor)
	{
		const UWorld *World = Actor->GetWorld();
		const UGameInstance *GameInstance = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GameInstance ? GameInstance->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		if (!PartySession || !PartySession->IsTrialPartyMember(Actor))
		{
			return nullptr;
		}
		// TODO(co-op): an enemy built from a party member's CharacterData (a mirror fight) also
		// matches here; exclude the opposing team when such fights exist.
		// After OpenLevel the leader pointer can be stale; ResolveLeader finds the same player's
		// controller in this world (B-132), never player 0.
		const UParty *Party = PartySession->GetLocalParty();
		const APlayerController *Leader = Party ? Party->ResolveLeader(World) : nullptr;
		return Leader ? PlayerWalletOf(Leader->PlayerState) : nullptr;
	}

	enum class EWalletSource : uint8
	{
		Player,
		PartyLeader,
		Own,
	};

	UCurrencyComponent *FindWithSource(const AActor *Actor, EWalletSource &OutSource)
	{
		const APawn *Pawn = Cast<APawn>(Actor);
		if (UCurrencyComponent *Wallet = Pawn ? PlayerWalletOf(Pawn->GetPlayerState()) : nullptr)
		{
			OutSource = EWalletSource::Player;
			return Wallet;
		}
		if (UCurrencyComponent *Wallet = PartyLeaderWalletOf(Actor))
		{
			OutSource = EWalletSource::PartyLeader;
			return Wallet;
		}
		OutSource = EWalletSource::Own;
		return Actor->FindComponentByClass<UCurrencyComponent>();
	}

	void PrintLocalWallet(UWorld *World)
	{
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		UE_LOG(LogTemp, Log, TEXT("%s"), *CurrencyWallet::GetWalletOwnerString(PC ? PC->GetPawn() : nullptr));
	}

	/** wor.Wallet.Add <Gold|Prisms|Diamond|GearEssence|SkillEssence> <amount>: a test grant (B-090 PIE checks). */
	void AddToLocalWallet(const TArray<FString> &Args, UWorld *World)
	{
		const int64 Currency = Args.Num() == 2 ? StaticEnum<ECurrencyType>()->GetValueByNameString(Args[0]) : INDEX_NONE;
		const int32 Amount = Args.Num() == 2 ? FCString::Atoi(*Args[1]) : 0;
		const APlayerController *PC = World ? World->GetFirstPlayerController() : nullptr;
		UCurrencyComponent *Wallet = PC ? PlayerWalletOf(PC->PlayerState) : nullptr;
		if (Currency == INDEX_NONE || Currency == static_cast<int64>(ECurrencyType::EssenceTyped) || Amount <= 0 || !Wallet)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Wallet] Usage: wor.Wallet.Add <Gold|Prisms|Diamond|GearEssence|SkillEssence> <amount> (needs the player's wallet)"));
			return;
		}
		Wallet->Add(static_cast<ECurrencyType>(Currency), Amount);
		UE_LOG(LogTemp, Log, TEXT("[Wallet] Added %d %s: now %d"), Amount, *Args[0],
			   Wallet->GetBalance(static_cast<ECurrencyType>(Currency)));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GWalletAddCommand(
	TEXT("wor.Wallet.Add"),
	TEXT("DEBUG: wor.Wallet.Add <Gold|Prisms|Diamond|GearEssence|SkillEssence> <amount> - add to the local player's wallet."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CurrencyWalletInternal::AddToLocalWallet));

static FAutoConsoleCommandWithWorld GWalletPrintCommand(
	TEXT("wor.Wallet.Print"),
	TEXT("Print the wallet the local player's pawn spends from (D-046: one per player)."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&CurrencyWalletInternal::PrintLocalWallet));

UCurrencyComponent *CurrencyWallet::FindFor(const AActor *Actor)
{
	if (!Actor)
	{
		return nullptr;
	}
	CurrencyWalletInternal::EWalletSource Source;
	return CurrencyWalletInternal::FindWithSource(Actor, Source);
}

FString CurrencyWallet::GetWalletOwnerString(const AActor *Actor)
{
	if (!Actor)
	{
		return TEXT("[Wallet] <no actor>");
	}
	using CurrencyWalletInternal::EWalletSource;
	EWalletSource Source;
	const UCurrencyComponent *Wallet = CurrencyWalletInternal::FindWithSource(Actor, Source);
	const TCHAR *SourceName = Source == EWalletSource::Player		 ? TEXT("player wallet")
							  : Source == EWalletSource::PartyLeader ? TEXT("party leader's player wallet")
																	 : TEXT("own component");
	return FString::Printf(TEXT("[Wallet] %s uses the %s (%s): %s"), *Actor->GetName(), SourceName,
						   Wallet ? *GetNameSafe(Wallet->GetOwner()) : TEXT("none"),
						   *UCurrencyComponentDebug::GetWalletString(Wallet));
}
