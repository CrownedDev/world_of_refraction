// WoRGameplayEffects.cpp

#include "GAS/WoRGameplayEffects.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "Tags/WoRGameplayTags.h"

UWoRGE_Stun::UWoRGE_Stun()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	// TODO: B-042b - no stacking policy yet, so a second Stun is a second instance with its own
	// span; the identity / refresh rules (D-012, D-053, Q-044) decide it in the spec builder.

	UTargetTagsGameplayEffectComponent *GrantedTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantedTags"));
	GEComponents.Add(GrantedTags);

	FInheritedTagContainer Tags;
	Tags.Added.AddTag(WoRTags::Status_Stun);
	GrantedTags->SetAndApplyTargetTagChanges(Tags);
}
