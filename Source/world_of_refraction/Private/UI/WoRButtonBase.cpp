// WoRButtonBase.cpp

#include "UI/WoRButtonBase.h"

#include "CommonButtonBase.h"

void UWoRButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyJobStyle(); // designer preview and runtime both show the job's look
}

void UWoRButtonBase::SetJob(EWoRButtonJob NewJob)
{
	Job = NewJob;
	ApplyJobStyle();
}

TSubclassOf<UCommonButtonStyle> UWoRButtonBase::GetStyleForJob(EWoRButtonJob ForJob) const
{
	return JobStyles.FindRef(ForJob);
}

void UWoRButtonBase::ApplyJobStyle()
{
	// SetStyle overwrites Style, so the authored one is kept for jobs without their own.
	if (!bAuthoredStyleCaptured)
	{
		AuthoredStyle = Style;
		bAuthoredStyleCaptured = true;
	}
	const TSubclassOf<UCommonButtonStyle> JobStyle = GetStyleForJob(Job);
	SetStyle(JobStyle ? JobStyle : AuthoredStyle);
}

FString UWoRButtonBase::GetButtonString() const
{
	return FString::Printf(TEXT("[WoRButton] %s: job %s, style %s"), *GetName(), *UEnum::GetValueAsString(Job),
						   *GetNameSafe(GetStyleForJob(Job).Get()));
}
