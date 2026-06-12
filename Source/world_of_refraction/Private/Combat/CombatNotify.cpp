// CombatNotify.cpp

#include "Combat/CombatNotify.h"
#include "Combat/CombatAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

UCombatNotify::UCombatNotify()
{
#if WITH_EDITORONLY_DATA
	RefreshNotifyColor();
#endif
}

const TCHAR *UCombatNotify::FamilyToString(ECombatNotifyFamily InFamily)
{
	switch (InFamily)
	{
	case ECombatNotifyFamily::Hit:
		return TEXT("Hit");
	case ECombatNotifyFamily::Cast:
		return TEXT("Cast");
	case ECombatNotifyFamily::VFX:
		return TEXT("VFX");
	case ECombatNotifyFamily::SFX:
		return TEXT("SFX");
	default:
		return TEXT("Unknown");
	}
}

void UCombatNotify::Notify(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
						   const FAnimNotifyEventReference &EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	UCombatAnimInstance *CombatAnim = Cast<UCombatAnimInstance>(MeshComp->GetAnimInstance());
	if (!CombatAnim)
	{
		// Editor preview windows and non-combat meshes land here — no-op.
		UE_LOG(LogTemp, Verbose, TEXT("[CombatNotify] %s%d fired with no UCombatAnimInstance (mesh %s)"),
			   FamilyToString(Family), Index, *MeshComp->GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[CombatNotify] %s%d fired — montage %s, actor %s"),
		   FamilyToString(Family), Index,
		   Animation ? *Animation->GetName() : TEXT("<null>"),
		   MeshComp->GetOwner() ? *MeshComp->GetOwner()->GetName() : TEXT("<no owner>"));

	CombatAnim->OnCombatNotify.Broadcast(Family, Index);
}

FString UCombatNotify::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("%s%d"), FamilyToString(Family), Index);
}

#if WITH_EDITOR
void UCombatNotify::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshNotifyColor();
}
#endif

#if WITH_EDITORONLY_DATA
void UCombatNotify::RefreshNotifyColor()
{
	switch (Family)
	{
	case ECombatNotifyFamily::Hit:
		NotifyColor = FColor(220, 60, 60); // red — contact
		break;
	case ECombatNotifyFamily::Cast:
		NotifyColor = FColor(255, 170, 40); // orange — launched sub-attack
		break;
	case ECombatNotifyFamily::VFX:
		NotifyColor = FColor(80, 160, 255); // blue — visual
		break;
	case ECombatNotifyFamily::SFX:
		NotifyColor = FColor(120, 220, 120); // green — audio
		break;
	}
}
#endif
