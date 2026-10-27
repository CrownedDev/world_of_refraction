// WoRGameplayEffects.cpp

#include "GAS/WoRGameplayEffects.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GAS/WoRAttributeSets.h"
#include "Tags/WoRGameplayTags.h"

void UWoRGameplayEffect::AddAssetTag(const FGameplayTag &Tag)
{
	UAssetTagsGameplayEffectComponent *AssetTags =
		CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("AssetTags"));
	GEComponents.Add(AssetTags);

	FInheritedTagContainer Tags;
	Tags.Added.AddTag(Tag);
	AssetTags->SetAndApplyAssetTagChanges(Tags);
}

void UWoRGameplayEffect::AddGrantedTag(const FGameplayTag &Tag)
{
	UTargetTagsGameplayEffectComponent *GrantedTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("GrantedTags"));
	GEComponents.Add(GrantedTags);

	FInheritedTagContainer Tags;
	Tags.Added.AddTag(Tag);
	GrantedTags->SetAndApplyTargetTagChanges(Tags);
}

// TODO: B-042c - no stacking policy on the lasting GEs: the facade mirrors the enum path's
// identity / refresh / stack decision (D-012, D-053, Q-044) through FWoREffectSpecBuilder::Apply,
// which replaces the GE with the same identity, so GAS stacking stays off.

UWoRGE_Stun::UWoRGE_Stun()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Status);
	AddGrantedTag(WoRTags::Status_Stun);
}

UWoRGE_Silence::UWoRGE_Silence()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Status);
	AddGrantedTag(WoRTags::Status_Silence);
}

UWoRGE_HealBlock::UWoRGE_HealBlock()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Status);
	AddGrantedTag(WoRTags::Status_HealBlock);
}

UWoRGE_Immunity::UWoRGE_Immunity()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Immunity);
}

UWoRGE_StatModifier::UWoRGE_StatModifier()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_StatModifier);
}

UWoRGE_DamageOverTime::UWoRGE_DamageOverTime()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_DOT);
	AddGrantedTag(WoRTags::Status_DOT);
}

UWoRGE_HealOverTime::UWoRGE_HealOverTime()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_HOT);
}

UWoRGE_HealthTick::UWoRGE_HealthTick()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Magnitude;
	Magnitude.DataTag = WoRTags::SetByCaller_Magnitude;

	FGameplayModifierInfo HealthChange;
	HealthChange.Attribute = UWoRAttributeSet_Vitals::GetHealthAttribute();
	HealthChange.ModifierOp = EGameplayModOp::Additive;
	HealthChange.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude);
	Modifiers.Add(HealthChange);
}

UWoRGE_HealTick::UWoRGE_HealTick()
{
	UTargetTagRequirementsGameplayEffectComponent *Requirements =
		CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TargetTagRequirements"));
	GEComponents.Add(Requirements);
	Requirements->ApplicationTagRequirements.IgnoreTags.AddTag(WoRTags::Status_HealBlock);
}

UWoRGE_Reflect::UWoRGE_Reflect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Reflect);
}

UWoRGE_LastStand::UWoRGE_LastStand()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_LastStand);
}

UWoRGE_OnHit::UWoRGE_OnHit()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_OnHit);
}

UWoRGE_AttackModifier::UWoRGE_AttackModifier()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_AttackModifier);
}

UWoRGE_Control::UWoRGE_Control()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Control);
}

UWoRGE_Periodic::UWoRGE_Periodic()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Periodic);
}

UWoRGE_Action::UWoRGE_Action()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddAssetTag(WoRTags::Effect_Family_Action);
}
