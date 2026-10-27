// EffectTextDebug.cpp

#include "Debug/Skills/EffectTextDebug.h"
#include "Character/CharacterDataComponent.h"
#include "Loadout/LoadoutComponent.h"
#include "Skills/Effects/SkillEffectManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr int32 INDENT_SPACES_PER_LEVEL = 2;

	FAutoConsoleCommandWithWorld GDescribeEffectsCmd(
		TEXT("wor.DescribeEffects"),
		TEXT("DEBUG: print every combatant's gear effects as generated text, lit by what holds now (B-045)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World) { UEffectTextDebug::DescribeEffects(World); }));

	const TCHAR *LightTag(EEffectTextLight Light)
	{
		switch (Light)
		{
		case EEffectTextLight::Held: return TEXT(" [held]");
		case EEffectTextLight::NotHeld: return TEXT(" [not held]");
		default: return TEXT("");
		}
	}
}

FString UEffectTextDebug::GetEffectTextString(const TArray<FEffectTextLine> &Lines, const TArray<FText> &ShortForm)
{
	TArray<FString> Out;
	for (const FEffectTextLine &Line : Lines)
	{
		const FString Indent = FString::ChrN(Line.Depth * INDENT_SPACES_PER_LEVEL, TEXT(' '));
		const TCHAR *Bullet = Line.Kind == EEffectTextLineKind::Effect ? TEXT("- ") : TEXT("");
		Out.Add(FString::Printf(TEXT("%s%s%s%s"), *Indent, Bullet, *Line.Text.ToString(), LightTag(Line.Light)));
	}
	Out.Add(TEXT("Short form:"));
	for (const FText &Short : ShortForm)
	{
		Out.Add(FString::Printf(TEXT("  %s"), *Short.ToString()));
	}
	return FString::Join(Out, TEXT("\n"));
}

FString UEffectTextDebug::GetActorEffectTextString(AActor *Actor)
{
	const ULoadoutComponent *Loadout = Actor ? Actor->FindComponentByClass<ULoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return TEXT("no LoadoutComponent");
	}
	const TArray<FSkillEffect> Effects = Loadout->GetActiveEffects(Actor);
	if (Effects.Num() == 0)
	{
		return TEXT("no gear effects");
	}

	TArray<FEffectTextLine> Lines = EffectText::BuildTree(Effects, EEffectTextSource::Gear);
	const UWorld *World = Actor->GetWorld();
	const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
	if (const USkillEffectManager *Manager = GI ? GI->GetSubsystem<USkillEffectManager>() : nullptr)
	{
		EffectText::LightTree(Lines, Effects, [Manager, Actor](const FSkillCondition &C)
		{
			return Manager->IsConditionHeld(C, Actor, nullptr);
		});
	}
	return GetEffectTextString(Lines, EffectText::BuildShortForm(Effects, EEffectTextSource::Gear));
}

int32 UEffectTextDebug::DescribeEffects(const UObject *WorldContextObject)
{
	UWorld *World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogWoR, Warning, TEXT("[EffectTextDebug] wor.DescribeEffects: no world."));
		return 0;
	}
	int32 Described = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->FindComponentByClass<UCharacterDataComponent>())
		{
			UE_LOG(LogWoR, Display, TEXT("[EffectTextDebug] %s\n%s"), *It->GetName(), *GetActorEffectTextString(*It));
			++Described;
		}
	}
	return Described;
}
