// CombatCameraDebug.h
// Debug utilities for the combat camera (UCombatCameraDirector, B-156). Console:
// wor.Camera.Print (state; B-398: where the character and the enemy centre land in the turn view),
// wor.Camera.Style 0|1|2 (centred-wide / over-the-shoulder / tactical; B-398: the saved player setting, also outside a
// fight), wor.Camera.MovementOff 0|1, wor.Camera.Handover [N] (B-404: preview the wide beat and the handover blend to
// party member N; Print shows the pending beat and the handovers sent), wor.Camera.MenuDepth 0|1 (B-399: what the command menu sends; Print shows the
// depth and the menu moves this fight), wor.Camera.TargetCycle [1|-1] (B-400: cycles the target picker's highlight;
// Print shows the highlighted target and the target-view moves).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "CombatCameraDebug.generated.h"

class UCombatCameraDirector;
class USkillDataBase;

UCLASS()
class WORLD_OF_REFRACTION_API UCombatCameraDebug : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** e.g. "In combat | mode Turn | viewing Hero_1 | style CentredWide | movement on | sequence none | 4 cameras". */
	static FString GetCameraStateString(const UCombatCameraDirector *Director);

	/** The director on the world's combat orchestrator, or null. */
	static UCombatCameraDirector *FindDirector(const UObject *WorldContextObject);

	/** Logs GetCameraStateString and shows it on screen. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Combat", meta = (WorldContext = "WorldContextObject"))
	static void PrintCameraState(const UObject *WorldContextObject);

	/** B-600: Style's shots in order for the sample fight: per moment, cut or blend, still or moving, FOV punch, or
	 *  "holds" where the style plays nothing. */
	static FString GetCameraStyleShotOrderString(ESkillCameraStyle Style);

	/** B-600: "<skill>: style <Style>" and its shot order. */
	static FString GetSkillCameraStyleString(const USkillDataBase *Skill);

	/** B-600: logs GetSkillCameraStyleString. Console: wor.Camera.StylePreview prints every style's order. */
	UFUNCTION(BlueprintCallable, Category = "Debug|Combat")
	static void PrintSkillCameraStyle(const USkillDataBase *Skill);
};
