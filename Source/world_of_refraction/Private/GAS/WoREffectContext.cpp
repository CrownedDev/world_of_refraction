// WoREffectContext.cpp

#include "GAS/WoREffectContext.h"

void FWoREffectIdentity::Serialize(FArchive &Ar)
{
	Ar << EffectType;
	Ar << Element;
	Ar << EffectID;
	Ar << SourceKey;
	Ar << bFromGear;
}

FString FWoREffectIdentity::ToString() const
{
	return FString::Printf(TEXT("%s %s id=%d source=%d%s"),
						   *UEnum::GetValueAsString(EffectType), *UEnum::GetValueAsString(Element),
						   EffectID, SourceKey, bFromGear ? TEXT(" gear") : TEXT(""));
}

const FWoREffectContext *FWoREffectContext::Get(const FGameplayEffectContextHandle &Handle)
{
	const FGameplayEffectContext *Context = Handle.Get();
	return Context && Context->GetScriptStruct()->IsChildOf(StaticStruct())
			   ? static_cast<const FWoREffectContext *>(Context)
			   : nullptr;
}

FWoREffectContext *FWoREffectContext::GetMutable(FGameplayEffectContextHandle &Handle)
{
	FGameplayEffectContext *Context = Handle.Get();
	return Context && Context->GetScriptStruct()->IsChildOf(StaticStruct())
			   ? static_cast<FWoREffectContext *>(Context)
			   : nullptr;
}

FWoREffectContext *FWoREffectContext::Duplicate() const
{
	FWoREffectContext *NewContext = new FWoREffectContext();
	*NewContext = *this;
	if (GetHitResult())
	{
		NewContext->AddHitResult(*GetHitResult(), true);
	}
	return NewContext;
}

bool FWoREffectContext::NetSerialize(FArchive &Ar, UPackageMap *Map, bool &bOutSuccess)
{
	const bool bBase = Super::NetSerialize(Ar, Map, bOutSuccess);
	Identity.Serialize(Ar);
	return bBase;
}

FString FWoREffectContext::ToString() const
{
	return FString::Printf(TEXT("%s [%s]"), *Super::ToString(), *Identity.ToString());
}

FGameplayEffectContext *UWoRAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FWoREffectContext();
}
