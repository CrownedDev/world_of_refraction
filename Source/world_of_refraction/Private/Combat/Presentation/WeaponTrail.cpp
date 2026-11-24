// WeaponTrail.cpp

#include "Combat/Presentation/WeaponTrail.h"
#include "Animation/AnimMontage.h"
#include "Combat/Actions/ActionExecutor.h"
#include "Combat/Actions/ActionStructs.h"
#include "Combat/Camera/CombatCameraTypes.h"
#include "Combat/CombatNotify.h"
#include "Combat/Presentation/ImpactVisualDefaults.h"
#include "Combat/WoRFightSubsystem.h"
#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Equipment/Weapons/WeaponMeshComponent.h"
#include "Infusion/ElementColors.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "WoRLog.h"

namespace
{
	UMeshComponent *FindWeaponMesh(const AActor *Owner)
	{
		const UWeaponMeshComponent *Weapons = Owner ? Owner->FindComponentByClass<UWeaponMeshComponent>() : nullptr;
		return Weapons ? Weapons->GetWeaponVFXMesh() : nullptr;
	}

	bool WeaponShownHasTrail(const AActor *Owner)
	{
		const UWeaponMeshComponent *Weapons = Owner ? Owner->FindComponentByClass<UWeaponMeshComponent>() : nullptr;
		const UWeaponData *Weapon = Weapons ? Weapons->GetDisplayedWeapon() : nullptr;
		return Weapon && WeaponTrail::HasTrail(Weapon->WeaponType);
	}

	/** The ribbon's ends in the mesh's local space; true when both came from the sockets. */
	bool GetTrailPoints(const UMeshComponent &Mesh, FVector &OutStart, FVector &OutEnd)
	{
		if (Mesh.DoesSocketExist(WeaponTrail::START_SOCKET) && Mesh.DoesSocketExist(WeaponTrail::END_SOCKET))
		{
			OutStart = Mesh.GetSocketTransform(WeaponTrail::START_SOCKET, RTS_Component).GetLocation();
			OutEnd = Mesh.GetSocketTransform(WeaponTrail::END_SOCKET, RTS_Component).GetLocation();
			return true;
		}
		WeaponTrail::LongAxisPoints(Mesh.CalcLocalBounds().GetBox(), OutStart, OutEnd);
		return false;
	}
}

bool WeaponTrail::HasTrail(EWeaponType Type)
{
	return Type != EWeaponType::Bow && Type != EWeaponType::Gun && Type != EWeaponType::Shield;
}

FLinearColor WeaponTrail::ColorFor(ESpellElement Element, const FLinearColor &Plain)
{
	return (Element == ESpellElement::None || Element == ESpellElement::Generic) ? Plain : ElementColors::GetColorForElement(Element);
}

bool WeaponTrail::FindFallbackWindow(const TArray<float> &ImpactTimes, float WindUp, float &OutStart, float &OutEnd)
{
	if (ImpactTimes.IsEmpty())
	{
		return false;
	}
	OutStart = FMath::Max(0.0f, ImpactTimes[0] - WindUp);
	OutEnd = ImpactTimes.Last();
	return OutEnd > OutStart;
}

TArray<float> WeaponTrail::GetImpactTimes(const UAnimMontage &Montage)
{
	TArray<float> Times;
	for (const FAnimNotifyEvent &Event : Montage.Notifies)
	{
		const UCombatNotify *Notify = Cast<UCombatNotify>(Event.Notify);
		if (Notify && Notify->Family == ECombatNotifyFamily::Impact)
		{
			Times.Add(Event.GetTriggerTime());
		}
	}
	Times.Sort();
	return Times;
}

bool WeaponTrail::HasTrailWindow(const UAnimMontage &Montage)
{
	for (const FAnimNotifyEvent &Event : Montage.Notifies)
	{
		if (Cast<UWeaponTrailWindow>(Event.NotifyStateClass))
		{
			return true;
		}
	}
	return false;
}

void WeaponTrail::LongAxisPoints(const FBox &Box, FVector &OutStart, FVector &OutEnd)
{
	const FVector Extent = Box.GetExtent();
	const int32 Axis = (Extent.X >= Extent.Y && Extent.X >= Extent.Z) ? 0 : (Extent.Y >= Extent.Z ? 1 : 2);
	FVector HalfLength = FVector::ZeroVector;
	HalfLength[Axis] = Extent[Axis];
	OutStart = Box.GetCenter() - HalfLength;
	OutEnd = Box.GetCenter() + HalfLength;
}

ESpellElement WeaponTrail::GetInfusedElement(AActor *Owner)
{
	UActionExecutor *Executor = Owner ? WoRFight::Get<UActionExecutor>(Owner) : nullptr;
	const FActionExecutionContext *Context = Executor ? Executor->GetCurrentExecutionContext() : nullptr;
	if (!Context || Context->Executor.Get() != Owner ||
		FMath::Max(Context->Action.SpellInfusionLevel, Context->Action.AbilityInfusionLevel) <= 0)
	{
		return ESpellElement::None;
	}
	return Executor->GetElementForSourceOption(Owner, Context->Action.SelectedSource);
}

UNiagaraComponent *WeaponTrail::Start(AActor *Owner, ESpellElement Element)
{
	UMeshComponent *Mesh = FindWeaponMesh(Owner);
	const UImpactVisualDefaults *Defaults = UImpactVisualDefaults::Get();
	if (!Mesh || !Defaults || !WeaponShownHasTrail(Owner))
	{
		return nullptr;
	}
	// shortcut: loads the slot on first use, which may hitch once; preload with the fight's assets when the art lands.
	UNiagaraSystem *System = Defaults->WeaponTrailVFX.LoadSynchronous();
	if (!System)
	{
		return nullptr;
	}
	UNiagaraComponent *Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	if (!Trail)
	{
		return nullptr;
	}
	FVector TrailStart, TrailEnd;
	GetTrailPoints(*Mesh, TrailStart, TrailEnd);
	Trail->ComponentTags.Add(COMPONENT_TAG);
	Trail->SetVariableVec3(START_PARAM, TrailStart);
	Trail->SetVariableVec3(END_PARAM, TrailEnd);
	Trail->SetVariableLinearColor(COLOR_PARAM, ColorFor(Element, Defaults->PlainTrailColor));
	UE_LOG(LogWoRCombat, Verbose, TEXT("[WeaponTrail] %s plays %s (%s)"), *GetNameSafe(Owner), *System->GetName(), *UEnum::GetValueAsString(Element));
	return Trail;
}

UNiagaraComponent *WeaponTrail::Start(AActor *Owner)
{
	return Start(Owner, GetInfusedElement(Owner));
}

void WeaponTrail::Stop(AActor *Owner)
{
	const UMeshComponent *Mesh = FindWeaponMesh(Owner);
	if (!Mesh)
	{
		return;
	}
	for (USceneComponent *Child : Mesh->GetAttachChildren())
	{
		UNiagaraComponent *Trail = Cast<UNiagaraComponent>(Child);
		if (Trail && Trail->ComponentHasTag(COMPONENT_TAG))
		{
			Trail->Deactivate(); // the ribbon fades, then auto-destroys
		}
	}
}

void WeaponTrail::ScheduleFallback(AActor *Owner, const UAnimMontage *Montage, float PlayRate, TArray<FTimerHandle> &OutHandles)
{
	const UImpactVisualDefaults *Defaults = UImpactVisualDefaults::Get();
	UWorld *World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !Montage || !Defaults || HasTrailWindow(*Montage))
	{
		return;
	}
	const float Rate = FMath::Max(FCombatCameraRules::MinPlayRate, PlayRate);
	TArray<float> Times = GetImpactTimes(*Montage);
	for (float &Time : Times)
	{
		Time /= Rate; // wall-clock, so the wind-up stays the same at any play rate
	}
	float WindowStart = 0.0f, WindowEnd = 0.0f;
	if (!FindFallbackWindow(Times, Defaults->TrailWindUpSeconds, WindowStart, WindowEnd))
	{
		return;
	}
	const TWeakObjectPtr<AActor> WeakOwner = Owner;
	const float Duration = WindowEnd - WindowStart;
	const FTimerDelegate Begin = FTimerDelegate::CreateLambda([WeakOwner, Duration]()
	{
		AActor *Actor = WeakOwner.Get();
		UNiagaraComponent *Trail = Actor ? Start(Actor) : nullptr;
		if (!Trail)
		{
			return;
		}
		// Stops this trail only, so a later one isn't cut short.
		FTimerHandle StopHandle;
		Actor->GetWorldTimerManager().SetTimer(StopHandle, FTimerDelegate::CreateLambda([WeakTrail = TWeakObjectPtr<UNiagaraComponent>(Trail)]()
		{
			if (UNiagaraComponent *Still = WeakTrail.Get())
			{
				Still->Deactivate();
			}
		}), Duration, false);
	});
	if (WindowStart <= 0.0f)
	{
		Begin.Execute();
		return;
	}
	FTimerHandle StartHandle;
	World->GetTimerManager().SetTimer(StartHandle, Begin, WindowStart, false);
	OutHandles.Add(StartHandle);
}

FString WeaponTrail::GetTrailString(AActor *Owner)
{
	const UMeshComponent *Mesh = FindWeaponMesh(Owner);
	if (!Mesh)
	{
		return FString::Printf(TEXT("%s: no weapon mesh shown"), *GetNameSafe(Owner));
	}
	const UImpactVisualDefaults *Defaults = UImpactVisualDefaults::Get();
	FVector TrailStart, TrailEnd;
	const bool bSockets = GetTrailPoints(*Mesh, TrailStart, TrailEnd);
	return FString::Printf(TEXT("%s: mesh %s, %s, system %s, ends %s to %s (%s), element %s"), *GetNameSafe(Owner),
						   *Mesh->GetName(), WeaponShownHasTrail(Owner) ? TEXT("has a trail") : TEXT("no trail (bow, gun or shield)"),
						   !Defaults ? TEXT("(no defaults asset)") : (Defaults->WeaponTrailVFX.IsNull() ? TEXT("(empty slot)") : *Defaults->WeaponTrailVFX.GetAssetName()),
						   *TrailStart.ToCompactString(), *TrailEnd.ToCompactString(), bSockets ? TEXT("sockets") : TEXT("long axis"),
						   *UEnum::GetValueAsString(GetInfusedElement(Owner)));
}

void UWeaponTrailWindow::NotifyBegin(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation, float TotalDuration,
									 const FAnimNotifyEventReference &EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (AActor *Owner = MeshComp ? MeshComp->GetOwner() : nullptr)
	{
		WeaponTrail::Start(Owner);
	}
}

void UWeaponTrailWindow::NotifyEnd(USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
								   const FAnimNotifyEventReference &EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (AActor *Owner = MeshComp ? MeshComp->GetOwner() : nullptr)
	{
		WeaponTrail::Stop(Owner);
	}
}
