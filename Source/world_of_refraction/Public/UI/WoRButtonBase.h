// WoRButtonBase.h
// Shared base for every CommonUI button (B-103). A button states its JOB, and the
// job picks its look from JobStyles, so the four kinds read the same on every
// screen (D-024): Play (start or commit to an action), Confirm (accept), Danger
// (destructive: dismantle, quit, discard) and Navigate (move between screens, Back).
//
// Styles are UCommonButtonStyle classes set on the WBP (or a shared parent WBP);
// a job with no style keeps whatever Style the button already has.

#pragma once

#include "CommonButtonBase.h"
#include "CoreMinimal.h"
#include "WoRButtonBase.generated.h"

UENUM(BlueprintType)
enum class EWoRButtonJob : uint8
{
	Play UMETA(DisplayName = "Play"),
	Confirm UMETA(DisplayName = "Confirm"),
	Danger UMETA(DisplayName = "Danger"),
	Navigate UMETA(DisplayName = "Navigate")
};

UCLASS(Abstract)
class WORLD_OF_REFRACTION_API UWoRButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/** Change the job and apply its style. */
	UFUNCTION(BlueprintCallable, Category = "WoR Button")
	void SetJob(EWoRButtonJob NewJob);

	UFUNCTION(BlueprintPure, Category = "WoR Button")
	EWoRButtonJob GetJob() const { return Job; }

	/** The style a job uses; null when JobStyles has none for it. */
	TSubclassOf<UCommonButtonStyle> GetStyleForJob(EWoRButtonJob ForJob) const;

	/** Debug: the button's job and the style it resolved to. */
	FString GetButtonString() const;

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WoR Button")
	EWoRButtonJob Job = EWoRButtonJob::Confirm;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WoR Button")
	TMap<EWoRButtonJob, TSubclassOf<UCommonButtonStyle>> JobStyles;

private:
	void ApplyJobStyle();
};
