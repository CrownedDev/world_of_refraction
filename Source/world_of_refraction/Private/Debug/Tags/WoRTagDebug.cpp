// WoRTagDebug.cpp

#include "Debug/Tags/WoRTagDebug.h"
#include "Tags/WoRGameplayTags.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr float TAG_DEBUG_SCREEN_SECONDS = 8.0f;

	const ESpellElement TAG_DEBUG_ELEMENTS[] = {
		ESpellElement::Generic, ESpellElement::Fire, ESpellElement::Water, ESpellElement::Earth,
		ESpellElement::Wind, ESpellElement::Light, ESpellElement::Darkness, ESpellElement::Lightning,
		ESpellElement::Void, ESpellElement::Reality, ESpellElement::None};

	const ECharacterClass TAG_DEBUG_CLASSES[] = {ECharacterClass::Generic, ECharacterClass::Caster, ECharacterClass::Resonator};

	/** The enum queries that have a tag answer: an effect type and the tags that must say the same. */
	struct FTagDebugImmunityCheck
	{
		ESkillEffectType IncomingType;
		const TCHAR *Label;
	};

	const FTagDebugImmunityCheck TAG_DEBUG_IMMUNITY_CHECKS[] = {
		{ESkillEffectType::Stun, TEXT("immune to Stun")},
		{ESkillEffectType::Silenced, TEXT("immune to Silence")},
		{ESkillEffectType::DOT, TEXT("immune to DOT")},
		{ESkillEffectType::HealthRestore, TEXT("can't be healed")}};

	FAutoConsoleCommand GDumpTagsCmd(
		TEXT("wor.DumpTags"),
		TEXT("DEBUG: print every enum value and its gameplay tag (B-040)."),
		FConsoleCommandDelegate::CreateLambda([]() { UE_LOG(LogTemp, Display, TEXT("[TagDebug]\n%s"), *UWoRTagDebug::GetTagMapString()); }));

	FAutoConsoleCommandWithWorld GCompareTagsCmd(
		TEXT("wor.CompareTags"),
		TEXT("DEBUG: check every combatant's tag queries give the same answers as the enum queries (B-040)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UWoRTagDebug::CompareTags(World); }));

	USkillEffectManager *TagDebugEffectManager(const AActor *Actor)
	{
		const UWorld *World = Actor ? Actor->GetWorld() : nullptr;
		const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr;
	}

	FString TagDebugEnumName(const UEnum *Enum, int64 Value)
	{
		return Enum ? Enum->GetNameStringByValue(Value) : FString::Printf(TEXT("%lld"), Value);
	}

	/** The tag answer to IsImmuneToEffectType: Immunity.All, or the type's own immunity / block tag. */
	bool TagDebugTagSaysImmune(const FGameplayTagContainer &Tags, ESkillEffectType IncomingType)
	{
		if (Tags.HasTagExact(WoRTags::Immunity_All))
		{
			return true;
		}
		switch (IncomingType)
		{
		case ESkillEffectType::Stun:
			return Tags.HasTagExact(WoRTags::Immunity_Stun);
		case ESkillEffectType::Silenced:
			return Tags.HasTagExact(WoRTags::Immunity_Silence);
		case ESkillEffectType::DOT:
			return Tags.HasTagExact(WoRTags::Immunity_DOT);
		case ESkillEffectType::HealthRestore:
			return Tags.HasTagExact(WoRTags::Status_HealBlock);
		default:
			return false;
		}
	}

	void TagDebugCheck(TArray<FString> &Out, const TCHAR *What, bool bEnum, bool bTag)
	{
		if (bEnum != bTag)
		{
			Out.Add(FString::Printf(TEXT("%s: enum says %s, tags say %s"), What,
									bEnum ? TEXT("yes") : TEXT("no"), bTag ? TEXT("yes") : TEXT("no")));
		}
	}
}

FString UWoRTagDebug::GetTagMapString()
{
	TArray<FString> Lines;
	for (const ESpellElement Element : TAG_DEBUG_ELEMENTS)
	{
		const FGameplayTag Tag = WoRTags::ElementToTag(Element);
		ESpellElement Back = ESpellElement::None;
		const bool bRoundTrip = WoRTags::TagToElement(Tag, Back) && Back == Element;
		Lines.Add(FString::Printf(TEXT("%s -> %s%s"), *TagDebugEnumName(StaticEnum<ESpellElement>(), static_cast<int64>(Element)),
								  *Tag.ToString(), bRoundTrip ? TEXT("") : TEXT("  ROUND-TRIP FAIL")));
	}
	for (const ECharacterClass CharacterClass : TAG_DEBUG_CLASSES)
	{
		const FGameplayTag Tag = WoRTags::ClassToTag(CharacterClass);
		ECharacterClass Back = ECharacterClass::Generic;
		const bool bRoundTrip = WoRTags::TagToClass(Tag, Back) && Back == CharacterClass;
		Lines.Add(FString::Printf(TEXT("%s -> %s%s"), *TagDebugEnumName(StaticEnum<ECharacterClass>(), static_cast<int64>(CharacterClass)),
								  *Tag.ToString(), bRoundTrip ? TEXT("") : TEXT("  ROUND-TRIP FAIL")));
	}
	for (const ESkillEffectType Type : WoRTags::TaggedEffectTypes())
	{
		Lines.Add(FString::Printf(TEXT("%s -> %s"), *TagDebugEnumName(StaticEnum<ESkillEffectType>(), static_cast<int64>(Type)),
								  *WoRTags::EffectTypeToTag(Type).ToString()));
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString UWoRTagDebug::GetActorTagsString(AActor *Actor)
{
	const USkillEffectManager *SEM = TagDebugEffectManager(Actor);
	if (!SEM)
	{
		return TEXT("no effect manager");
	}
	const FGameplayTagContainer Tags = SEM->GetEffectTags(Actor);
	return Tags.IsEmpty() ? FString(TEXT("no tags")) : Tags.ToStringSimple();
}

FString UWoRTagDebug::GetTagMismatchString(AActor *Actor)
{
	USkillEffectManager *SEM = TagDebugEffectManager(Actor);
	if (!SEM)
	{
		return TEXT("no effect manager");
	}
	const FGameplayTagContainer Tags = SEM->GetEffectTags(Actor);
	TArray<FString> Mismatches;

	for (const ESkillEffectType Type : WoRTags::TaggedEffectTypes())
	{
		const FString What = TagDebugEnumName(StaticEnum<ESkillEffectType>(), static_cast<int64>(Type));
		TagDebugCheck(Mismatches, *What, SEM->HasEffectOfType(Actor, Type), Tags.HasTagExact(WoRTags::EffectTypeToTag(Type)));
	}
	TagDebugCheck(Mismatches, TEXT("stunned"), SEM->IsStunned(Actor), Tags.HasTagExact(WoRTags::Status_Stun));
	TagDebugCheck(Mismatches, TEXT("silenced"), SEM->IsSilenced(Actor), Tags.HasTagExact(WoRTags::Status_Silence));
	TagDebugCheck(Mismatches, TEXT("has a DOT"), SEM->HasActiveDOT(Actor), Tags.HasTagExact(WoRTags::Status_DOT));
	for (const FTagDebugImmunityCheck &Check : TAG_DEBUG_IMMUNITY_CHECKS)
	{
		TagDebugCheck(Mismatches, Check.Label, SEM->IsImmuneToEffectType(Actor, Check.IncomingType),
					  TagDebugTagSaysImmune(Tags, Check.IncomingType));
	}

	const UCharacterDataComponent *CharComp = Actor->FindComponentByClass<UCharacterDataComponent>();
	if (CharComp && CharComp->CharacterData)
	{
		ECharacterClass Back = ECharacterClass::Generic;
		const ECharacterClass Class = CharComp->CharacterData->CharacterClass;
		TagDebugCheck(Mismatches, TEXT("class round-trip"), true,
					  WoRTags::TagToClass(WoRTags::ClassToTag(Class), Back) && Back == Class);
	}
	return FString::Join(Mismatches, TEXT("\n"));
}

int32 UWoRTagDebug::CompareTags(const UObject *WorldContextObject)
{
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	int32 Disagreeing = 0;
	int32 Checked = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (!It->FindComponentByClass<UCharacterDataComponent>())
		{
			continue;
		}
		++Checked;
		const FString Mismatches = GetTagMismatchString(*It);
		if (Mismatches.IsEmpty())
		{
			UE_LOG(LogTemp, Display, TEXT("[TagDebug] %s: tags agree (%s)"), *It->GetName(), *GetActorTagsString(*It));
			continue;
		}
		++Disagreeing;
		UE_LOG(LogTemp, Warning, TEXT("[TagDebug] %s: tags DISAGREE\n%s"), *It->GetName(), *Mismatches);
	}
	const FString Summary = FString::Printf(TEXT("wor.CompareTags: %d of %d combatant(s) disagree"), Disagreeing, Checked);
	UE_LOG(LogTemp, Display, TEXT("[TagDebug] %s"), *Summary);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, TAG_DEBUG_SCREEN_SECONDS, Disagreeing > 0 ? FColor::Red : FColor::Green, Summary);
	}
	return Disagreeing;
}
