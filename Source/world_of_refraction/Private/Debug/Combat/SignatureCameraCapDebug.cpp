// SignatureCameraCapDebug.cpp
// B-407: wor.Camera.SignatureCap prints the signature-sequence cap and what it did to the last skill sequence
// (UCombatCameraDirector::GetSignatureCapString). Its own line, beside wor.Camera.Print.

#include "Combat/Camera/CombatCameraDirector.h"
#include "Debug/Combat/CombatCameraDebug.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "WoRLog.h"

namespace
{
	constexpr float SIGNATURE_CAP_SCREEN_SECONDS = 8.0f;

	FAutoConsoleCommandWithWorld GPrintSignatureCapCmd(
		TEXT("wor.Camera.SignatureCap"),
		TEXT("DEBUG: print the signature-sequence cap (B-407) and the last skill sequence: its length, caster, and whether it was cut."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld *World)
			{
				const UCombatCameraDirector *Director = UCombatCameraDebug::FindDirector(World);
				const FString State = Director ? Director->GetSignatureCapString() : TEXT("No combat camera (no combat orchestrator in this world)");
				UE_LOG(LogWoRCombat, Display, TEXT("[CombatCamera] signature %s"), *State);
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(INDEX_NONE, SIGNATURE_CAP_SCREEN_SECONDS, FColor::Cyan, State);
				}
			}));
}
