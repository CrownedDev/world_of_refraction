// ElementColorDebugComponent.cpp
// Debug component for visualizing character element via mesh color

#include "Infusion/ElementColorDebugComponent.h"
#include "Character/CharacterDataComponent.h"
#include "Character/CharacterData.h"
#include "Infusion/ElementColors.h"
#include "Infusion/HybridSpellColors.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Class.h"

UElementColorDebugComponent::UElementColorDebugComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    
    ColorParameterName = FName("BaseColor");
    MaterialIndex = 0;
    bApplyOnBeginPlay = true;
    TargetMesh = nullptr;
    DynamicMaterial = nullptr;
    CurrentColor = FLinearColor::White;
}

void UElementColorDebugComponent::BeginPlay()
{
    Super::BeginPlay();
    
    if (bApplyOnBeginPlay)
    {
        ApplyElementColor();
    }
}

void UElementColorDebugComponent::ApplyElementColor()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ElementColorDebug] No owner actor"));
        return;
    }

    // Get CharacterDataComponent
    UCharacterDataComponent* CharDataComp = Owner->FindComponentByClass<UCharacterDataComponent>();
    if (!CharDataComp || !CharDataComp->CharacterData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ElementColorDebug] No CharacterDataComponent or CharacterData on %s"), *Owner->GetName());
        return;
    }

    ESpellElement Element = CharDataComp->CharacterData->InnateElement;
    FLinearColor CharacterColor;

    // Use IsBrokenDarkness() helper to handle both runtime-transformed and
    // character-created BD characters uniformly. Runtime-transformed
    // characters retain their asset's InnateElement (Darkness, etc.) but the
    // helper returns true via the bIsBrokenDarkness flag.
    if (CharDataComp->IsBrokenDarkness())
    {
        UBrokenDarknessManager* BDManager = Owner->FindComponentByClass<UBrokenDarknessManager>();
        ESpellElement AbsorbedElement = BDManager ? BDManager->GetHybridElement() : ESpellElement::Generic;

        if (AbsorbedElement != ESpellElement::Generic && AbsorbedElement != ESpellElement::BrokenDarkness)
        {
            FHybridSpellColorData Colors = UHybridSpellColors::GetHybridSpellColors(AbsorbedElement);
            CharacterColor = Colors.BlendedColor;
            UE_LOG(LogTemp, Log, TEXT("[ElementColorDebug] %s: BD with absorbed %s -> Blended color"),
                *Owner->GetName(), *UEnum::GetValueAsString(AbsorbedElement));
        }
        else
        {
            CharacterColor = ElementColors::BrokenDarkness;
            UE_LOG(LogTemp, Log, TEXT("[ElementColorDebug] %s: Pure BD -> Black"), *Owner->GetName());
        }
    }
    else
    {
        // Normal element
        CharacterColor = ElementColors::GetColorForElement(Element);
        UE_LOG(LogTemp, Log, TEXT("[ElementColorDebug] %s: Element %d -> Color applied"), *Owner->GetName(), (int32)Element);
    }

    ApplyColorToMaterial(CharacterColor);
}

void UElementColorDebugComponent::UpdateAbsorptionColor(ESpellElement AbsorbedElement)
{
    if (AbsorbedElement == ESpellElement::Generic || AbsorbedElement == ESpellElement::BrokenDarkness)
    {
        // No absorption or pure BD
        ApplyColorToMaterial(ElementColors::BrokenDarkness);
    }
    else
    {
        // Has absorbed element
        FHybridSpellColorData Colors = UHybridSpellColors::GetHybridSpellColors(AbsorbedElement);
        ApplyColorToMaterial(Colors.BlendedColor);
    }

    UE_LOG(LogTemp, Log, TEXT("[ElementColorDebug] Updated BD absorption color for element %d"), (int32)AbsorbedElement);
}

void UElementColorDebugComponent::SetColorDirect(FLinearColor Color)
{
    ApplyColorToMaterial(Color);
    UE_LOG(LogTemp, Log, TEXT("[ElementColorDebug] Direct color set: R=%.2f G=%.2f B=%.2f"), Color.R, Color.G, Color.B);
}

UMeshComponent* UElementColorDebugComponent::FindMeshComponent() const
{
    if (TargetMesh)
    {
        return TargetMesh;
    }

    AActor* Owner = GetOwner();
    if (!Owner) return nullptr;

    // Try skeletal mesh first (characters)
    USkeletalMeshComponent* SkelMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (SkelMesh)
    {
        return SkelMesh;
    }

    // Fall back to static mesh
    UStaticMeshComponent* StaticMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
    if (StaticMesh)
    {
        return StaticMesh;
    }

    return nullptr;
}

UMaterialInstanceDynamic* UElementColorDebugComponent::GetOrCreateDynamicMaterial()
{
    if (DynamicMaterial)
    {
        return DynamicMaterial;
    }

    UMeshComponent* Mesh = FindMeshComponent();
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ElementColorDebug] No mesh component found on %s"), 
            GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
        return nullptr;
    }

    // Create dynamic material instance
    DynamicMaterial = Mesh->CreateDynamicMaterialInstance(MaterialIndex);
    
    if (!DynamicMaterial)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ElementColorDebug] Failed to create dynamic material for %s"), 
            GetOwner() ? *GetOwner()->GetName() : TEXT("None"));
    }

    return DynamicMaterial;
}

void UElementColorDebugComponent::ApplyColorToMaterial(FLinearColor Color)
{
    UMaterialInstanceDynamic* Mat = GetOrCreateDynamicMaterial();
    if (!Mat)
    {
        return;
    }

    Mat->SetVectorParameterValue(ColorParameterName, Color);
    CurrentColor = Color;
}
