// WoRGameplayEffects.cpp

#include "GAS/WoRGameplayEffects.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/WoRGameplayTags.h"

UWoRGE_Stun::UWoRGE_Stun()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	UTargetTagsGameplayEffectComponent *GrantedTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantedTags"));
	GEComponents.Add(GrantedTags);

	FInheritedTagContainer Tags;
	Tags.Added.AddTag(WoRTags::Status_Stun);
	GrantedTags->SetAndApplyTargetTagChanges(Tags);
}
