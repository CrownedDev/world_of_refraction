// ClassDefaultLoadoutData.cpp

#include "Loadout/ClassDefaultLoadoutData.h"
#include "Equipment/Apparel/ApparelData.h"
#include "Equipment/EquipmentDataBase.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Skills/Definitions/AbilityData.h"
#include "WoRLog.h"

FString UClassDefaultLoadoutData::GetDefaultsString() const
{
	FString Out = FString::Printf(TEXT("[ClassDefaults] %s: %d class kit(s)"), *GetName(), Defaults.Num());
	for (const TPair<ECharacterClass, FClassDefaultLoadout> &Pair : Defaults)
	{
		const FClassDefaultLoadout &Kit = Pair.Value;
		Out += FString::Printf(TEXT("\n  %s: %d place(s), ACTIVE %d, %d apparel"), *UEnum::GetDisplayValueAsText(Pair.Key).ToString(),
							   Kit.Pieces.Num(), Kit.ActiveIndex, Kit.Apparel.Num());
		for (int32 Index = 0; Index < Kit.Pieces.Num(); ++Index)
		{
			const FClassDefaultPiece &Piece = Kit.Pieces[Index];
			TArray<FString> Skills;
			for (const UAbilityData *Skill : Piece.SkillPicks)
			{
				Skills.Add(GetNameSafe(Skill));
			}
			Out += FString::Printf(TEXT("\n    [%d] %s (tier %s)%s%s"), Index, Piece.Piece ? *Piece.Piece->GetName() : TEXT("<empty>"),
								   Piece.Piece ? *UEnum::GetDisplayValueAsText(Piece.Piece->Tier).ToString() : TEXT("-"),
								   Piece.OffHand ? *FString::Printf(TEXT(" + off hand %s"), *Piece.OffHand->GetName()) : TEXT(""),
								   Skills.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", skills %s"), *FString::Join(Skills, TEXT(", "))));
		}
		for (const UApparelData *Apparel : Kit.Apparel)
		{
			Out += FString::Printf(TEXT("\n    apparel %s"), *GetNameSafe(Apparel));
		}
		if (!Kit.Pieces.IsValidIndex(Kit.ActiveIndex) || !Kit.Pieces[Kit.ActiveIndex].Piece)
		{
			Out += TEXT("\n    WARNING: the ACTIVE place holds no piece");
		}
	}
	return Out;
}

void UClassDefaultLoadoutData::PrintDefaults() const
{
	UE_LOG(LogWoRLoadout, Display, TEXT("%s"), *GetDefaultsString());
}
