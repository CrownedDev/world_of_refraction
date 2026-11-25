// FriendlyNoDamageSkillSpec.cpp
// B-621: a no-damage ability aimed at the caster's own side (Self / Ally) lands no hit: no defend window (so no prompt),
// no damage and no fallback impact, while its buff still applies. And a skill's Self / Ally target survives a save and
// reload: the retired AbilityData / SpellData.TargetType redirect no longer sends it into LegacyTargetType, and a skill
// re-saved while it did takes its target back from the legacy field in PostLoad. Headless and montage-less, as
// WoR.Combat.PerImpactDefense.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "WoRTestWorld.h"
#include "DefenseOutcomeRecorder.h"
#include "ElementDamageRecorder.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/CombatOrchestrator.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Defense/DefenseSystem.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GAS/Abilities/WoRSkillAbilities.h"
#include "Inventory/InventoryComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Definitions/SupportSkillRules.h"
#include "Skills/Effects/EffectDefinition.h"
#include "Skills/Effects/SkillEffectManager.h"

namespace
{
	constexpr int32 FRIENDLY_SPEC_STRIKE_DAMAGE = 30;
	constexpr int32 FRIENDLY_SPEC_BUFF_EP = 4;
	constexpr int32 FRIENDLY_SPEC_START_EP = 40;
	constexpr float FRIENDLY_SPEC_BUFF_PERCENT = 20.0f;
	constexpr int32 FRIENDLY_SPEC_BUFF_TURNS = 3;
	constexpr int32 FRIENDLY_SPEC_BATTLE_SEED = 621;
	/** Spans the executor's 10 s action timeout at 0.01 s a frame; a run stops once the turn moves on. */
	constexpr int32 FRIENDLY_SPEC_SETTLE_FRAMES = 1100;
	/** Two impacts, so a fallback would have had something to land. */
	constexpr int32 FRIENDLY_SPEC_TWO_HITS = 2;

	/** Two shipped support spells whose Self / Ally target the redirect turned into Enemy on load. */
	const TCHAR *FRIENDLY_SPEC_ALLY_SPELL = TEXT("/Game/Data/Pool/Generic/Spells/Enhancement/DA_Spell_Might.DA_Spell_Might");
	const TCHAR *FRIENDLY_SPEC_SELF_SPELL = TEXT("/Game/Data/Spells/Fire/Restoration/DA_Spells_Fire_Cauterize.DA_Spells_Fire_Cauterize");

	void SetFriendlySpecCVar(const TCHAR *Name, int32 Value)
	{
		if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	}

	/** Saves Source's properties and loads them into a fresh object of its class, as a package save and reload does. */
	template <typename TSkill>
	TSkill *SaveAndReload(TSkill *Source)
	{
		TArray<uint8> Bytes;
		FObjectWriter Writer(Source, Bytes);
		TSkill *Reloaded = NewObject<TSkill>(GetTransientPackage());
		FObjectReader Reader(Reloaded, Bytes);
		return Reloaded;
	}
}

BEGIN_DEFINE_SPEC(FFriendlyNoDamageSkillSpec, "WoR.Combat.FriendlyNoDamageSkill",
				  EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
	ACombatOrchestrator *Orchestrator = nullptr;
	UAbilityData *Strike = nullptr;
	UAbilityData *Buff = nullptr;

	/** What one use of the buff did. */
	struct FBuffOutcome
	{
		bool bRan = false;
		FString Refusal;
		bool bWindowOpenedOnTarget = false;
		int32 TargetDamage = 0;
		int32 Hits = 0;
		int32 DefendedImpacts = 0;
		bool bTargetBuffed = false;
		bool bTurnMovedOn = false;
	};

	/** A montage-less weapon buff: no damage, one Defense Buff on its target. */
	UAbilityData *MakeBuff(ETargetType Target)
	{
		FSkillEffectPayload Payload;
		Payload.EffectType = ESkillEffectType::DefenseBuff;
		Payload.Value = FRIENDLY_SPEC_BUFF_PERCENT;
		Payload.Duration = FRIENDLY_SPEC_BUFF_TURNS;
		Payload.Target = Target;
		FSkillEffect Effect;
		Effect.Payloads.Add(Payload);
		UEffectDefinition *Definition = NewObject<UEffectDefinition>(GetTransientPackage());
		Definition->Effects.Add(Effect); // kept alive by the rooted skill's ReferencedEffects
		UAbilityData *NewBuff = NewObject<UAbilityData>(GetTransientPackage());
		NewBuff->Name = TEXT("Montage-less Guard Up");
		NewBuff->BaseDamage = 0;
		NewBuff->BaseEnergyCost = FRIENDLY_SPEC_BUFF_EP;
		NewBuff->ExecutionType = EAbilityExecutionType::Melee;
		NewBuff->TargetType = Target;
		NewBuff->HitCount = FRIENDLY_SPEC_TWO_HITS;
		NewBuff->ReferencedEffects.Add(Definition);
		NewBuff->AddToRoot();
		return NewBuff;
	}

	AActor *SpawnArmed()
	{
		UWeaponData *Weapon = NewObject<UWeaponData>(GetTransientPackage());
		Weapon->WeaponAttack = Strike;
		ULoadoutComponent *Loadout = TestWorld->SpawnCombatantWithLoadout(FWoRTestWorld::NewCharacterData(), Weapon, /*bAIControlled*/ false);
		AActor *Actor = Loadout ? Loadout->GetOwner() : nullptr;
		if (UInventoryComponent *Inventory = Actor ? Actor->FindComponentByClass<UInventoryComponent>() : nullptr)
		{
			FWeaponInventoryEntry Owned;
			Owned.Weapon = Weapon;
			Inventory->Weapons.Add(Owned);
		}
		return Actor;
	}

	static int32 CurrentHPOf(const AActor *Actor)
	{
		const UCharacterDataComponent *Stats = Actor ? Actor->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		return Stats ? Stats->CurrentHP : 0;
	}

	/** The hero (with an ally beside him) uses Skill on Self or the ally, until the turn moves on. bInstantImpacts 0
	 *  checks the window right after the submit and then runs the animation-end fallback, as a montage's end does. */
	FBuffOutcome RunBuff(UAbilityData *Skill, bool bOnAlly, bool bInstantImpacts)
	{
		FBuffOutcome Outcome;
		SetFriendlySpecCVar(TEXT("wor.Combat.InstantImpacts"), bInstantImpacts ? 1 : 0);
		TestWorld = MakeUnique<FWoRTestWorld>();
		if (!TestWorld->Create() || !TestWorld->GetWorld())
		{
			return Outcome;
		}
		Orchestrator = TestWorld->GetWorld()->SpawnActor<ACombatOrchestrator>();
		AActor *Hero = SpawnArmed();
		AActor *Ally = SpawnArmed();
		AActor *Foe = SpawnArmed();
		UDefenseSystem *Defense = TestWorld->GetSubsystem<UDefenseSystem>();
		UActionExecutor *Executor = TestWorld->GetSubsystem<UActionExecutor>();
		USkillEffectManager *Effects = TestWorld->GetSubsystem<USkillEffectManager>();
		UCharacterDataComponent *HeroStats = Hero ? Hero->FindComponentByClass<UCharacterDataComponent>() : nullptr;
		if (!Orchestrator || !Defense || !Executor || !Effects || !HeroStats || !Ally || !Foe)
		{
			return Outcome;
		}
		Orchestrator->StartCombat({Hero, Ally}, {Foe});
		if (Orchestrator->GetCurrentActor() != Hero)
		{
			return Outcome;
		}
		HeroStats->MaxEP = FRIENDLY_SPEC_START_EP;
		HeroStats->CurrentEP = FRIENDLY_SPEC_START_EP;
		AActor *Target = bOnAlly ? Ally : Hero;
		TStrongObjectPtr<UDefenseOutcomeRecorder> Defenses(NewObject<UDefenseOutcomeRecorder>());
		Defenses->Listen(Defense, Target);
		TStrongObjectPtr<UElementDamageRecorder> Hits(NewObject<UElementDamageRecorder>());
		Hits->Listen(Executor);

		FAction Action;
		Action.ActionType = EActionType::Ability;
		Action.SkillData = Skill;
		Action.Targets.Add(Target);
		Outcome.Refusal = Executor->ValidateAction(Hero, Action).ErrorMessage;
		const int32 TargetHPBefore = CurrentHPOf(Target);
		Orchestrator->SubmitActionAsync(Action);
		Outcome.bWindowOpenedOnTarget = Defense->IsDefenseWindowOpen(Target);
		if (!bInstantImpacts)
		{
			TestWorld->Tick();
			Outcome.bWindowOpenedOnTarget |= Defense->IsDefenseWindowOpen(Target);
			Executor->ReconcileDefenseWindowsAtAnimationEnd(); // what Skill-end runs: the fallback impacts
		}
		for (int32 Frame = 0; Frame < FRIENDLY_SPEC_SETTLE_FRAMES && Orchestrator->GetCurrentActor() == Hero; ++Frame)
		{
			TestWorld->Tick();
		}

		Outcome.bRan = true;
		Outcome.TargetDamage = TargetHPBefore - CurrentHPOf(Target);
		Outcome.Hits = Hits->Entries.Num();
		Outcome.DefendedImpacts = Defenses->ImpactIndices.Num();
		Outcome.bTargetBuffed = Effects->HasEffectOfType(Target, ESkillEffectType::DefenseBuff);
		Outcome.bTurnMovedOn = Orchestrator->GetCurrentActor() != Hero;
		return Outcome;
	}

	void TestNoHit(const FString &What, const FBuffOutcome &Outcome)
	{
		if (!TestTrue(FString::Printf(TEXT("%s: the buff ran (refusal: %s)"), *What, *Outcome.Refusal), Outcome.bRan && Outcome.Refusal.IsEmpty()))
		{
			return;
		}
		TestFalse(FString::Printf(TEXT("%s: no defend window opened on the target"), *What), Outcome.bWindowOpenedOnTarget);
		TestEqual(FString::Printf(TEXT("%s: no damage"), *What), Outcome.TargetDamage, 0);
		TestEqual(FString::Printf(TEXT("%s: no hit"), *What), Outcome.Hits, 0);
		TestEqual(FString::Printf(TEXT("%s: no impact defended"), *What), Outcome.DefendedImpacts, 0);
		TestTrue(FString::Printf(TEXT("%s: the buff landed"), *What), Outcome.bTargetBuffed);
		TestTrue(FString::Printf(TEXT("%s: the turn moved on"), *What), Outcome.bTurnMovedOn);
	}
END_DEFINE_SPEC(FFriendlyNoDamageSkillSpec)

void FFriendlyNoDamageSkillSpec::Define()
{
	BeforeEach([this]()
	{
		Strike = NewObject<UAbilityData>(GetTransientPackage());
		Strike->Name = TEXT("Montage-less Strike");
		Strike->BaseDamage = FRIENDLY_SPEC_STRIKE_DAMAGE;
		Strike->ExecutionType = EAbilityExecutionType::Melee;
		Strike->bIsAttack = true;
		Strike->AddToRoot();
		SetFriendlySpecCVar(TEXT("wor.Battle.Seed"), FRIENDLY_SPEC_BATTLE_SEED);
	});

	AfterEach([this]()
	{
		SetFriendlySpecCVar(TEXT("wor.Combat.InstantImpacts"), 0);
		SetFriendlySpecCVar(TEXT("wor.Battle.Seed"), 0);
		if (Orchestrator && Orchestrator->GetCombatState() != ECombatState::Idle)
		{
			Orchestrator->ForceEndCombat();
		}
		Orchestrator = nullptr;
		TestWorld.Reset();
		if (Strike)
		{
			Strike->RemoveFromRoot();
			Strike = nullptr;
		}
		if (Buff)
		{
			Buff->RemoveFromRoot();
			Buff = nullptr;
		}
	});

	Describe("a no-damage Self / Ally ability", [this]()
	{
		It("is told apart from a damaging or enemy one", [this]()
		{
			Buff = MakeBuff(ETargetType::Self);
			TestTrue(TEXT("No damage at Self"), SupportSkillRules::IsFriendlyNoDamageSkill(Buff));
			Buff->TargetType = ETargetType::Ally;
			TestTrue(TEXT("No damage at an ally"), SupportSkillRules::IsFriendlyNoDamageSkill(Buff));
			Buff->TargetType = ETargetType::Enemy;
			TestFalse(TEXT("No damage at an enemy (a debuff swing): still a hit"), SupportSkillRules::IsFriendlyNoDamageSkill(Buff));
			TestFalse(TEXT("A damaging strike"), SupportSkillRules::IsFriendlyNoDamageSkill(Strike));
		});

		It("on Self: no defend window, no damage, no fallback impact; the buff lands", [this]()
		{
			Buff = MakeBuff(ETargetType::Self);
			TestNoHit(TEXT("Self, animation-end fallback"), RunBuff(Buff, /*bOnAlly*/ false, /*bInstantImpacts*/ false));
			TestNoHit(TEXT("Self, instant impacts"), RunBuff(Buff, /*bOnAlly*/ false, /*bInstantImpacts*/ true));
		});

		It("on an ally: no defend window, no damage, no fallback impact; the buff lands", [this]()
		{
			Buff = MakeBuff(ETargetType::Ally);
			TestNoHit(TEXT("Ally, animation-end fallback"), RunBuff(Buff, /*bOnAlly*/ true, /*bInstantImpacts*/ false));
			TestNoHit(TEXT("Ally, instant impacts"), RunBuff(Buff, /*bOnAlly*/ true, /*bInstantImpacts*/ true));
		});
	});

	Describe("a skill's target type", [this]()
	{
		It("survives a save and reload as Self and as Ally, on an ability and a spell", [this]()
		{
			for (const ETargetType Target : {ETargetType::Self, ETargetType::Ally})
			{
				const FString Name = UEnum::GetDisplayValueAsText(Target).ToString();
				UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
				Ability->TargetType = Target;
				Ability->bTargetAxisMigrated = true;
				TestEqual(FString::Printf(TEXT("Ability at %s, reloaded"), *Name), SaveAndReload(Ability)->TargetType, Target);
				USpellData *Spell = NewObject<USpellData>(GetTransientPackage());
				Spell->TargetType = Target;
				Spell->bTargetAxisMigrated = true;
				TestEqual(FString::Printf(TEXT("Spell at %s, reloaded"), *Name), SaveAndReload(Spell)->TargetType, Target);
			}
		});

		It("takes a Self / Ally target back from the legacy field when a re-save lost it, and keeps an enemy one", [this]()
		{
			const TPair<ETargetType_Legacy, ETargetType> Cases[] = {
				{ETargetType_Legacy::Self, ETargetType::Self},
				{ETargetType_Legacy::SingleAlly, ETargetType::Ally},
				{ETargetType_Legacy::AllAllies, ETargetType::Ally},
				{ETargetType_Legacy::Everyone, ETargetType::Anyone},
				{ETargetType_Legacy::SingleEnemy, ETargetType::Enemy},
				{ETargetType_Legacy::AllEnemies, ETargetType::Enemy},
			};
			for (const TPair<ETargetType_Legacy, ETargetType> &Case : Cases)
			{
				UAbilityData *Ability = NewObject<UAbilityData>(GetTransientPackage());
				Ability->bTargetAxisMigrated = true;
				Ability->TargetType = ETargetType::Enemy; // what a re-save under the redirect kept
				Ability->TargetCount = ETargetCount::All;
				Ability->LegacyTargetType = Case.Key;
				Ability->PostLoad();
				const FString Legacy = UEnum::GetValueAsString(Case.Key);
				TestEqual(FString::Printf(TEXT("%s: target"), *Legacy), Ability->TargetType, Case.Value);
				TestEqual(FString::Printf(TEXT("%s: the saved count stays"), *Legacy), Ability->TargetCount, ETargetCount::All);
			}
			UAbilityData *Kept = NewObject<UAbilityData>(GetTransientPackage());
			Kept->bTargetAxisMigrated = true;
			Kept->TargetType = ETargetType::Ally;
			Kept->LegacyTargetType = ETargetType_Legacy::Self;
			Kept->PostLoad();
			TestEqual(TEXT("A target that loaded is never overwritten"), Kept->TargetType, ETargetType::Ally);
		});

		It("loads the shipped support spells at their own targets", [this]()
		{
			const USpellData *Might = LoadObject<USpellData>(nullptr, FRIENDLY_SPEC_ALLY_SPELL);
			const USpellData *Cauterize = LoadObject<USpellData>(nullptr, FRIENDLY_SPEC_SELF_SPELL);
			if (TestNotNull(TEXT("Might"), Might))
			{
				TestEqual(TEXT("Might targets an ally"), Might->TargetType, ETargetType::Ally);
			}
			if (TestNotNull(TEXT("Cauterize"), Cauterize))
			{
				TestEqual(TEXT("Cauterize targets Self"), Cauterize->TargetType, ETargetType::Self);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
