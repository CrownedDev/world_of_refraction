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
	if (const TSubclassOf<UCommonButtonStyle> JobStyle = GetStyleForJob(Job))
	{
		SetStyle(JobStyle);
	}
}

FString UWoRButtonBase::GetButtonString() const
{
	return FString::Printf(TEXT("[WoRButton] %s: job %s, style %s"), *GetName(), *UEnum::GetValueAsString(Job),
						   *GetNameSafe(GetStyleForJob(Job).Get()));
}
