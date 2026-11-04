// GearGive.cpp

#include "Inventory/GearGive.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/MentorTokens.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/TurnManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Player/WoRPlayerState.h"
#include "Pool/PoolSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

#define LOCTEXT_NAMESPACE "GearGive"

namespace
{
	UPoolSubsystem *PoolOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
	}

	const UParty *GiveLocalPartyOf(const UWorld *World)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		return PartySession ? PartySession->GetLocalParty() : nullptr;
	}

	/** The character's name as the party shows it; the asset's base name when it isn't a party member. */
	FText NameOf(const UWorld *World, FName CharacterKey)
	{
		if (const UParty *Party = GiveLocalPartyOf(World))
		{
			for (const FPartyMember &Member : Party->Members)
			{
				if (Member.CharacterData && UPoolSubsystem::GetCharacterKey(Member) == CharacterKey)
				{
					return FText::FromString(Member.CharacterData->Name);
				}
			}
		}
		return FText::FromString(FPaths::GetBaseFilename(CharacterKey.ToString()));
	}

	/** Q-321: a run-only recruit owns nothing (what it has goes back when it leaves), so it can't be given gear. */
	bool IsRunOnlyMember(const UWorld *World, FName CharacterKey)
	{
		if (const UParty *Party = GiveLocalPartyOf(World))
		{
			for (const FPartyMember &Member : Party->Members)
			{
				if (Member.CharacterData && UPoolSubsystem::GetCharacterKey(Member) == CharacterKey)
				{
					return Member.bRunOnly;
				}
			}
		}
		return false;
	}

	/** B-316b: the local party member whose character key is Key, or null. */
	const FPartyMember *GiveMemberOf(const UWorld *World, FName CharacterKey)
	{
		const UParty *Party = GiveLocalPartyOf(World);
		return Party ? Party->Members.FindByPredicate([CharacterKey](const FPartyMember &Member)
													  { return Member.CharacterData && UPoolSubsystem::GetCharacterKey(Member) == CharacterKey; })
					 : nullptr;
	}

	/** B-361 (D-153): the player who claims the member (co-op, D-150), or null when nobody does. */
	const APlayerState *ClaimantOf(const UWorld *World, FName CharacterKey)
	{
		const FPartyMember *Member = GiveMemberOf(World, CharacterKey);
		const AGameStateBase *GameState = World ? World->GetGameState() : nullptr;
		if (!GameState || !Member || !Member->MemberId.IsValid())
		{
			return nullptr;
		}
		for (const APlayerState *PS : GameState->PlayerArray)
		{
			const AWoRPlayerState *WoRPS = Cast<AWoRPlayerState>(PS);
			if (WoRPS && WoRPS->ClaimsMember(Member->MemberId))
			{
				return PS;
			}
		}
		return nullptr;
	}

	/** B-361 (D-153, D-152): the player a member belongs to: its claimant, else the host (an AI member or a run-only
	 *  recruit nobody claims is the host's). The host is whoever claims the host's own character (the claims
	 *  replicate, so a client reads the same answer); before anyone claims it, the server's first player. Null with no
	 *  players (a headless test, solo without claims): every member is then the same player's. */
	const APlayerState *PlayerOfMember(const UWorld *World, FName CharacterKey)
	{
		if (const APlayerState *Claimant = ClaimantOf(World, CharacterKey))
		{
			return Claimant;
		}
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		if (const APlayerState *Host = PartySession ? ClaimantOf(World, PartySession->GetHostKey()) : nullptr)
		{
			return Host;
		}
		const APlayerController *ServerPC = World && World->GetNetMode() != NM_Client ? World->GetFirstPlayerController() : nullptr;
		return ServerPC ? ServerPC->PlayerState.Get() : nullptr;
	}

	/** B-361b: Key knows Skill (a spell or an ability). */
	bool KnowsSkill(const UPoolSubsystem &Pool, FName Key, const UObject *Skill)
	{
		const USpellData *Spell = Cast<USpellData>(Skill);
		return Spell ? Pool.KnowsSpell(Key, Spell) : Pool.KnowsAbility(Key, Cast<UAbilityData>(Skill));
	}

	/** B-316b: a skill's name for the player. TODO: FText names with B-127 (D-061). */
	FText SkillNameOf(const UObject *Skill)
	{
		if (const USpellData *Spell = Cast<USpellData>(Skill))
		{
			return FText::FromString(Spell->Name);
		}
		const UAbilityData *Ability = Cast<UAbilityData>(Skill);
		return Ability ? FText::FromString(Ability->Name) : FText::GetEmpty();
	}

	/** wor.Give.Print: what every local party member could give. */
	FAutoConsoleCommandWithWorld GGivePrintCmd(
		TEXT("wor.Give.Print"),
		TEXT("DEBUG: list what each party member could give (weapons, adapters, pouch stacks) (B-323)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
		{
			const UParty *Party = GiveLocalPartyOf(World);
			if (!Party)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Give] wor.Give.Print: no local party"));
				return;
			}
			for (const FPartyMember &Member : Party->Members)
			{
				if (Member.CharacterData)
				{
					UE_LOG(LogTemp, Log, TEXT("%s"), *WoRGive::GetGiveableString(World, UPoolSubsystem::GetCharacterKey(Member)));
				}
			}
		}));
}

namespace WoRGive
{
	FName GetPouchKey(const UWorld *World, FName MemberKey)
	{
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
		return PartySession ? PartySession->GetPouchKey(MemberKey) : MemberKey;
	}

	UInventoryComponent *FindMemberInventory(UWorld *World, FName Key)
	{
		if (!World || Key.IsNone())
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const UCharacterDataComponent *CharComp = It->FindComponentByClass<UCharacterDataComponent>();
			if (CharComp && UPoolSubsystem::GetCharacterKey(CharComp) == Key)
			{
				if (UInventoryComponent *Inventory = It->FindComponentByClass<UInventoryComponent>())
				{
					return Inventory;
				}
			}
		}
		return nullptr;
	}

	FText TryGiveGear(UWorld *World, FName FromKey, FName ToKey, const FGuid &GearID)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoPool", "No armoury to give from");
		}
		if (FromKey.IsNone() || ToKey.IsNone() || FromKey == ToKey)
		{
			return LOCTEXT("SameCharacter", "Choose another character to give to");
		}
		if (Pool->FindEvolutionInPool(GearID))
		{
			return LOCTEXT("NoEvolutions", "Evolutions can't be given yet");
		}
		if (!Pool->FindWeaponInPool(GearID) && !Pool->FindRingInPool(GearID))
		{
			return LOCTEXT("NotGear", "That isn't a weapon or adapter");
		}
		if (Pool->GetGearOwner(GearID) != FromKey)
		{
			return FText::Format(LOCTEXT("NotTheirs", "{0} doesn't own that"), NameOf(World, FromKey));
		}
		if (IsRunOnlyMember(World, ToKey))
		{
			return FText::Format(LOCTEXT("RecruitOwnsNothing", "{0} is only here for the run and can't own gear"), NameOf(World, ToKey));
		}
		// D-116: a loadout's primary stays until another primary is swapped in. Checked on the spawned giver; an
		// unspawned giver's next draw refills its primary instead.
		if (const UInventoryComponent *Giver = FindMemberInventory(World, FromKey))
		{
			if (Giver->IsPrimaryInAnyLoadout(GearID))
			{
				return LOCTEXT("GivePrimary", "That is a loadout's primary. Swap in another primary before giving it");
			}
		}
		if (!Pool->GiveGear(GearID, ToKey))
		{
			return LOCTEXT("GiveFailed", "Couldn't give that");
		}
		return FText::GetEmpty();
	}

	FText TryPutInChest(UWorld *World, FName FromKey, const FGuid &GearID)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoChest", "No chest to put that in");
		}
		if (FromKey.IsNone())
		{
			return LOCTEXT("NoOneToPut", "Choose a character");
		}
		if (Pool->FindEvolutionInPool(GearID))
		{
			return LOCTEXT("NoEvolutionsChest", "Evolutions can't go in the chest yet");
		}
		if (!Pool->FindWeaponInPool(GearID) && !Pool->FindRingInPool(GearID))
		{
			return LOCTEXT("NotGearChest", "That isn't a weapon or adapter");
		}
		if (Pool->GetGearOwner(GearID) != FromKey)
		{
			return FText::Format(LOCTEXT("NotTheirsChest", "{0} doesn't own that"), NameOf(World, FromKey));
		}
		// Worn gear can't be put away (B-324): a piece anyone wears (spawned or not), or in any of the member's
		// loadouts, stays.
		if (!Pool->GetWearer(GearID).IsNone())
		{
			return LOCTEXT("PutWornByAnyone", "Someone is wearing that. Take it off before putting it in the chest");
		}
		if (const UInventoryComponent *Member = FindMemberInventory(World, FromKey))
		{
			if (!Member->GetLoadoutsUsing(GearID).IsEmpty())
			{
				return LOCTEXT("PutWorn", "That is in a loadout. Take it off before putting it in the chest");
			}
		}
		if (!Pool->GiveGear(GearID, UPoolSubsystem::GetChestKey()))
		{
			return LOCTEXT("PutFailed", "Couldn't put that in the chest");
		}
		// An unspawned member's loadouts can't be checked; the piece leaves them at its next draw, so no wearer stays.
		Pool->ReleaseWearer(GearID);
		return FText::GetEmpty();
	}

	FText TryTakeFromChest(UWorld *World, FName ToKey, const FGuid &GearID)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoChestTake", "No chest to take from");
		}
		if (ToKey.IsNone())
		{
			return LOCTEXT("NoOneToTake", "Choose a character");
		}
		if (!Pool->IsInChest(GearID))
		{
			return LOCTEXT("NotInChest", "That isn't in the chest");
		}
		if (IsRunOnlyMember(World, ToKey))
		{
			return FText::Format(LOCTEXT("RecruitOwnsNothingChest", "{0} is only here for the run and can't own gear"), NameOf(World, ToKey));
		}
		if (!Pool->GiveGear(GearID, ToKey))
		{
			return LOCTEXT("TakeFailed", "Couldn't take that");
		}
		return FText::GetEmpty();
	}

	FText TryGiveCarried(UWorld *World, FName FromMember, FName ToMember, const FCrystalId &Id, int32 Count)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoPouches", "No pouches to give from");
		}
		if (FromMember.IsNone() || ToMember.IsNone() || FromMember == ToMember)
		{
			return LOCTEXT("SameCharacterItems", "Choose another character to give to");
		}
		const FName FromKey = GetPouchKey(World, FromMember);
		const FName ToKey = GetPouchKey(World, ToMember);
		if (FromKey == ToKey)
		{
			return LOCTEXT("SharedPouch", "They carry the same pouch: a recruit uses the host's");
		}
		if (Count <= 0 || Pool->GetCarriedCount(FromKey, Id) < Count)
		{
			return FText::Format(LOCTEXT("NotCarried", "{0} doesn't carry that many"), NameOf(World, FromKey));
		}
		if (Pool->GetCarryRoom(ToKey, Id) < Count)
		{
			return FText::Format(LOCTEXT("PouchFull", "{0}'s pouch has no room for {1}"), NameOf(World, ToKey),
								 FText::FromString(ItemIdentity::GetDisplayName(Id)));
		}
		if (!Pool->GiveCarried(FromKey, ToKey, Id, Count))
		{
			return LOCTEXT("GiveItemsFailed", "Couldn't give those");
		}
		return FText::GetEmpty();
	}

	FText TryGiveSkill(UWorld *World, FName FromKey, FName ToKey, const UObject *Skill)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoSkillStore", "No skills to give from");
		}
		if (FromKey.IsNone() || ToKey.IsNone() || FromKey == ToKey)
		{
			return LOCTEXT("SameCharacterSkills", "Choose another character to give to");
		}
		const USpellData *Spell = Cast<USpellData>(Skill);
		const UAbilityData *Ability = Cast<UAbilityData>(Skill);
		if (!Spell && !Ability)
		{
			return LOCTEXT("NotASkill", "That isn't a spell or an ability");
		}
		// Between fights only: the giver's loadouts drop the skill in the pre-fight clean (ClearInvalidSlots).
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UTurnManager *TurnManager = GI ? GI->GetSubsystem<UTurnManager>() : nullptr;
		if (TurnManager && TurnManager->IsCombatActive())
		{
			return LOCTEXT("SkillsInFight", "Skills can't change hands during a fight");
		}
		if (PlayerOfMember(World, FromKey) != PlayerOfMember(World, ToKey))
		{
			return LOCTEXT("SkillsBetweenPlayers", "Skills can't be traded between players");
		}
		for (const FName Key : {FromKey, ToKey})
		{
			if (!Pool->HasBankedSkills(Key))
			{
				return FText::Format(LOCTEXT("SkillsNotKept", "{0}'s skills aren't kept yet (they join at the next level)"), NameOf(World, Key));
			}
		}
		if (!(Spell ? Pool->KnowsSpell(FromKey, Spell) : Pool->KnowsAbility(FromKey, Ability)))
		{
			return FText::Format(LOCTEXT("DoesntKnow", "{0} doesn't know {1}"), NameOf(World, FromKey), SkillNameOf(Skill));
		}
		if (Spell ? Pool->KnowsSpell(ToKey, Spell) : Pool->KnowsAbility(ToKey, Ability))
		{
			return FText::Format(LOCTEXT("AlreadyKnows", "{0} already knows {1}"), NameOf(World, ToKey), SkillNameOf(Skill));
		}
		if (!(Spell ? Pool->MoveSpell(FromKey, ToKey, Spell) : Pool->MoveAbility(FromKey, ToKey, Ability)))
		{
			return FText::Format(LOCTEXT("SkillsFull", "{0} can't learn any more"), NameOf(World, ToKey));
		}
		return FText::GetEmpty();
	}

	FText TryRequestSkill(UWorld *World, FName RequesterKey, FName HolderKey, const UObject *Skill)
	{
		return TryGiveSkill(World, HolderKey, RequesterKey, Skill); // D-153: another player's member refuses there
	}

	FText TryTeachSkill(UWorld *World, FName TeacherKey, FName LearnerKey, const UObject *Skill)
	{
		UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return LOCTEXT("NoSkillStoreTeach", "No skills to teach from");
		}
		if (TeacherKey.IsNone() || LearnerKey.IsNone() || TeacherKey == LearnerKey)
		{
			return LOCTEXT("SameCharacterTeach", "Choose another player's character to teach");
		}
		if (!Cast<USpellData>(Skill) && !Cast<UAbilityData>(Skill))
		{
			return LOCTEXT("NotASkillTeach", "That isn't a spell or an ability");
		}
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		const UTurnManager *TurnManager = GI ? GI->GetSubsystem<UTurnManager>() : nullptr;
		if (TurnManager && TurnManager->IsCombatActive())
		{
			return LOCTEXT("TeachInFight", "Skills can't be taught during a fight");
		}
		// The wallet and the skill store change on the host; a client's teach waits for the co-op screens' request.
		// TODO: B-233+: a client asks the server to teach (the pool is per game instance, D-065 / D-103).
		if (World->GetNetMode() == NM_Client)
		{
			return LOCTEXT("TeachOnHost", "Only the host can teach for now (co-op requests come with the co-op screens)");
		}
		const APlayerState *Teacher = PlayerOfMember(World, TeacherKey);
		if (!Teacher || Teacher == PlayerOfMember(World, LearnerKey))
		{
			return LOCTEXT("TeachIsBetweenPlayers", "Teaching is between players: give it to your own characters instead");
		}
		if (!Pool->HasBankedSkills(TeacherKey) || !Pool->HasBankedSkills(LearnerKey))
		{
			return LOCTEXT("SkillsNotKeptTeach", "Both characters' skills must be kept first (they join at the next level)");
		}
		if (!KnowsSkill(*Pool, TeacherKey, Skill))
		{
			return FText::Format(LOCTEXT("DoesntKnowTeach", "{0} doesn't know {1}"), NameOf(World, TeacherKey), SkillNameOf(Skill));
		}
		if (KnowsSkill(*Pool, LearnerKey, Skill))
		{
			return FText::Format(LOCTEXT("AlreadyKnowsTeach", "{0} already knows {1}"), NameOf(World, LearnerKey), SkillNameOf(Skill));
		}
		const int32 Cost = WoRMentorTokens::GetTeachCost(Skill);
		UCurrencyComponent *Wallet = WoRMentorTokens::WalletOf(Teacher);
		if (!Wallet || !Wallet->Spend(ECurrencyType::MentorTokens, Cost))
		{
			return FText::Format(LOCTEXT("TooFewTokens", "Teaching {0} costs {1} Mentor Tokens"), SkillNameOf(Skill), Cost);
		}
		if (!Pool->LearnSkillCopy(LearnerKey, Skill))
		{
			Wallet->Add(ECurrencyType::MentorTokens, Cost); // nothing learned, nothing spent
			return FText::Format(LOCTEXT("SkillsFullTeach", "{0} can't learn any more"), NameOf(World, LearnerKey));
		}
		UE_LOG(LogTemp, Log, TEXT("[Give] %s teaches %s to %s for %d Mentor Token(s)"), *TeacherKey.ToString(), *GetNameSafe(Skill),
			   *LearnerKey.ToString(), Cost);
		return FText::GetEmpty();
	}

	TArray<const UObject *> GetGiveableSkills(const UWorld *World, FName FromKey)
	{
		TArray<const UObject *> Skills;
		const UPoolSubsystem *Pool = PoolOf(World);
		const FPoolSkills *Known = Pool ? Pool->FindSkills(FromKey) : nullptr;
		if (!Known)
		{
			return Skills;
		}
		for (const FSpellInstance &Spell : Known->Spells)
		{
			if (Spell.Spell)
			{
				Skills.Add(Spell.Spell);
			}
		}
		for (const FAbilityInstance &Ability : Known->Abilities)
		{
			if (Ability.Ability)
			{
				Skills.Add(Ability.Ability);
			}
		}
		return Skills;
	}

	FString GetGiveableString(const UWorld *World, FName FromKey)
	{
		const UPoolSubsystem *Pool = PoolOf(World);
		if (!Pool)
		{
			return TEXT("[Give] no pool");
		}
		FString Out = FString::Printf(TEXT("[Give] %s can give:"), *FPaths::GetBaseFilename(FromKey.ToString()));
		for (const FWeaponInventoryEntry &Weapon : Pool->GetOwnedWeapons())
		{
			if (Pool->GetGearOwner(Weapon.PersistentID) == FromKey)
			{
				Out += FString::Printf(TEXT("\n  weapon %s (%s)"), Weapon.Weapon ? *Weapon.Weapon->Name : TEXT("?"), *Weapon.PersistentID.ToString());
			}
		}
		for (const FRingInventoryEntry &Ring : Pool->GetOwnedRings())
		{
			if (Pool->GetGearOwner(Ring.PersistentID) == FromKey)
			{
				Out += FString::Printf(TEXT("\n  adapter %s (%s)"), Ring.Ring ? *Ring.Ring->Name : TEXT("?"), *Ring.PersistentID.ToString());
			}
		}
		for (const TPair<FCrystalId, int32> &Stack : Pool->GetCarriedItems(GetPouchKey(World, FromKey)))
		{
			Out += FString::Printf(TEXT("\n  pouch %s x%d"), *ItemIdentity::GetDisplayName(Stack.Key), Stack.Value);
		}
		for (const UObject *Skill : GetGiveableSkills(World, FromKey)) // B-316b
		{
			Out += FString::Printf(TEXT("\n  %s %s"), Skill->IsA<USpellData>() ? TEXT("spell") : TEXT("ability"), *SkillNameOf(Skill).ToString());
		}
		return Out;
	}
}

#undef LOCTEXT_NAMESPACE
