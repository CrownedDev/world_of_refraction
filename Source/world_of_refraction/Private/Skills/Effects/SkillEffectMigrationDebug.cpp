// SkillEffectMigrationDebug.cpp

#include "Skills/Effects/SkillEffectMigrationDebug.h"
#include "Skills/Effects/SkillEffectDebug.h"
#include "Skills/Definitions/SkillDataBase.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Crystals/EvolutionItemData.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"
#include "UObject/Package.h"
#endif

DEFINE_LOG_CATEGORY(LogWoRMigration);

TArray<UObject *> USkillEffectMigrationDebug::GetAllEffectCarriers()
{
	TArray<UObject *> Carriers;
#if WITH_EDITOR
	IAssetRegistry &AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	AR.SearchAllAssets(/*bSynchronousSearch=*/true);

	auto Collect = [&AR, &Carriers](UClass *BaseClass)
	{
		FARFilter Filter;
		Filter.bRecursiveClasses = true; // USpellData/UAbilityData, UWeaponData/URingData included
		Filter.ClassPaths.Add(BaseClass->GetClassPathName());

		TArray<FAssetData> Found;
		AR.GetAssets(Filter, Found);
		for (const FAssetData &AD : Found)
		{
			if (UObject *Obj = AD.GetAsset())
			{
				Carriers.AddUnique(Obj);
			}
			else
			{
				UE_LOG(LogWoRMigration, Warning, TEXT("[Migration] failed to load %s — skipped"),
					   *AD.GetObjectPathString());
			}
		}
	};

	Collect(USkillDataBase::StaticClass());
	Collect(UEquipmentDataBase::StaticClass());
	Collect(UEvolutionItemData::StaticClass());
#endif
	return Carriers;
}

void USkillEffectMigrationDebug::ResaveAllEffectAssets()
{
#if WITH_EDITOR
	const TArray<UObject *> Carriers = GetAllEffectCarriers();
	int32 Saved = 0;
	int32 Skipped = 0;
	for (UObject *Obj : Carriers)
	{
		if (!Obj)
		{
			++Skipped;
			continue;
		}
		if (UPackage *Pkg = Obj->GetPackage())
		{
			Pkg->MarkPackageDirty();
		}
		// SaveLoadedAsset persists the migrated Conditions[]/Payloads[] populated on load.
		if (UEditorAssetLibrary::SaveLoadedAsset(Obj, /*bOnlyIfIsDirty=*/false))
		{
			++Saved;
			UE_LOG(LogWoRMigration, Log, TEXT("[Resave] saved %s"), *Obj->GetPathName());
		}
		else
		{
			++Skipped;
			UE_LOG(LogWoRMigration, Warning, TEXT("[Resave] FAILED to save %s — skipped"), *Obj->GetPathName());
		}
	}
	UE_LOG(LogWoRMigration, Log, TEXT("=== F2 RESAVE: %d carriers, %d saved, %d skipped ==="),
		   Carriers.Num(), Saved, Skipped);
#else
	UE_LOG(LogWoRMigration, Warning, TEXT("[Migration] ResaveAllEffectAssets is editor-only."));
#endif
}

void USkillEffectMigrationDebug::VerifyAllEffectAssets()
{
#if WITH_EDITOR
	const TArray<UObject *> Carriers = GetAllEffectCarriers();
	int32 NumSkill = 0;
	int32 NumEquip = 0;
	int32 NumEvo = 0;
	int32 TotalEffects = 0;
	int32 Failures = 0;

	auto ScanEffects = [&](const TArray<FSkillEffect> &Effects, const FString &AssetPath)
	{
		for (int32 i = 0; i < Effects.Num(); ++i)
		{
			const FSkillEffect &E = Effects[i];
			++TotalEffects;

			// (a) on-disk payloads must exist for any real effect — proves the re-save wrote them.
			if (E.IsValid() && E.Payloads.Num() == 0)
			{
				++Failures;
				UE_LOG(LogWoRMigration, Error, TEXT("FAIL %s effect[%d]: empty Payloads[] (not re-saved?)"),
					   *AssetPath, i);
			}

			// (b) migration parity (legacy vs new shape) must still hold on disk.
			FString Report;
			if (!SkillEffectDebug::CompareEffect(E, Report))
			{
				++Failures;
				UE_LOG(LogWoRMigration, Error, TEXT("FAIL %s effect[%d]: CompareEffect mismatch\n%s"),
					   *AssetPath, i, *Report);
			}
		}
	};

	for (UObject *Obj : Carriers)
	{
		if (!Obj)
		{
			continue;
		}
		const FString Path = Obj->GetPathName();
		if (const USkillDataBase *S = Cast<USkillDataBase>(Obj))
		{
			++NumSkill;
			ScanEffects(S->Effects, Path);
		}
		else if (const UEquipmentDataBase *Q = Cast<UEquipmentDataBase>(Obj))
		{
			++NumEquip;
			ScanEffects(Q->Effects, Path);
		}
		else if (const UEvolutionItemData *V = Cast<UEvolutionItemData>(Obj))
		{
			++NumEvo;
			ScanEffects(V->Effects, Path);
		}
	}

	UE_LOG(LogWoRMigration, Log,
		   TEXT("[Verify] carriers: %d (SkillDataBase=%d, Equipment=%d, Evolution=%d), effects scanned: %d"),
		   Carriers.Num(), NumSkill, NumEquip, NumEvo, TotalEffects);

	if (Failures == 0)
	{
		UE_LOG(LogWoRMigration, Log, TEXT("=== F2 VERIFY: 0 failures — F3 CLEARED ==="));
	}
	else
	{
		UE_LOG(LogWoRMigration, Error, TEXT("=== F2 VERIFY: %d FAILURES — DO NOT PROCEED ==="), Failures);
	}
#else
	UE_LOG(LogWoRMigration, Warning, TEXT("[Migration] VerifyAllEffectAssets is editor-only."));
#endif
}
