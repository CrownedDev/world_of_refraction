// OutfitSpec.cpp
// B-279c (Design/CharacterCustomisation.md): outfits. A swapped piece replaces only its own slot; a set saves and
// loads (six soft paths); a skin comes with its authored outfit; pieces swap only where skins do.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Character/Outfit/OutfitPieceData.h"
#include "Engine/GameInstance.h"

namespace
{
	UOutfitPieceData *NewOutfitPiece(EOutfitSlot Slot)
	{
		UOutfitPieceData *Piece = NewObject<UOutfitPieceData>(GetTransientPackage());
		Piece->Slot = Slot;
		return Piece;
	}
}

BEGIN_DEFINE_SPEC(FOutfitSpec, "WoR.Character.Outfit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UGameInstance *GameInstance = nullptr;
	UCharacterFormSubsystem *Forms = nullptr;
END_DEFINE_SPEC(FOutfitSpec)

void FOutfitSpec::Define()
{
	BeforeEach([this]()
	{
		GameInstance = NewObject<UGameInstance>(GetTransientPackage());
		Forms = NewObject<UCharacterFormSubsystem>(GameInstance);
	});

	AfterEach([this]()
	{
		Forms = nullptr;
		GameInstance = nullptr;
	});

	It("replaces only the swapped piece's slot", [this]()
	{
		FOutfitSet Set;
		UOutfitPieceData *Jacket = NewOutfitPiece(EOutfitSlot::Jacket);
		UOutfitPieceData *Waist = NewOutfitPiece(EOutfitSlot::Waist);
		UOutfitPieceData *OtherJacket = NewOutfitPiece(EOutfitSlot::Jacket);
		TestTrue(TEXT("jacket worn"), Set.SetPiece(Jacket));
		TestTrue(TEXT("waist worn"), Set.SetPiece(Waist));
		TestTrue(TEXT("another jacket swapped in"), Set.SetPiece(OtherJacket));
		TestTrue(TEXT("the jacket slot holds the new one"), Set.Get(EOutfitSlot::Jacket).Get() == OtherJacket);
		TestTrue(TEXT("the waist is untouched"), Set.Get(EOutfitSlot::Waist).Get() == Waist);
		TestTrue(TEXT("other slots stay empty"), Set.Get(EOutfitSlot::Helmet).IsNull() && Set.Get(EOutfitSlot::Footwear).IsNull());
		TestFalse(TEXT("no piece, no change"), Set.SetPiece(nullptr));
		Set.ClearSlot(EOutfitSlot::Waist);
		TestTrue(TEXT("a cleared slot is empty"), Set.Get(EOutfitSlot::Waist).IsNull());
		TestTrue(TEXT("the jacket stays"), Set.Get(EOutfitSlot::Jacket).Get() == OtherJacket);
	});

	It("saves and loads a set as six soft paths", [this]()
	{
		FOutfitSet Set;
		Set.SetPiece(NewOutfitPiece(EOutfitSlot::Helmet));
		Set.SetPiece(NewOutfitPiece(EOutfitSlot::Trousers));
		const TArray<FSoftObjectPath> Saved = Set.ToSaved();
		TestEqual(TEXT("one path per slot"), Saved.Num(), OutfitSlots::SLOT_COUNT);
		const FOutfitSet Loaded = FOutfitSet::FromSaved(Saved);
		TestTrue(TEXT("the same set comes back"), Loaded == Set);
		TestTrue(TEXT("an older save has no outfit"), FOutfitSet::FromSaved({}).IsEmpty());

		TArray<FSoftObjectPath> WrongSlot = Saved;
		WrongSlot[static_cast<int32>(EOutfitSlot::Shirt)] = WrongSlot[static_cast<int32>(EOutfitSlot::Helmet)];
		TestTrue(TEXT("a helmet saved in the shirt slot loads empty"), FOutfitSet::FromSaved(WrongSlot).Get(EOutfitSlot::Shirt).IsNull());
	});

	It("wears a skin's authored outfit, and swaps pieces only where skins switch", [this]()
	{
		UCharacterFormData *Form = NewObject<UCharacterFormData>(GetTransientPackage());
		Form->Character = NewObject<UCharacterData>(GetTransientPackage());
		UOutfitPieceData *AuthoredJacket = NewOutfitPiece(EOutfitSlot::Jacket);
		Form->AuthoredOutfit.SetPiece(AuthoredJacket);
		Forms->RegisterForm(Form);
		Forms->RestoreProgress({{Form->GetFName(), CharacterFormMeter::FULL}});
		Forms->SetAtRestSpot(true);
		TestEqual(TEXT("skin worn"), Forms->TryWearForm(Form), EFormSwitchRefusal::None);
		TestTrue(TEXT("its authored outfit came with it"), Forms->GetCurrentOutfit() == Form->AuthoredOutfit);

		UOutfitPieceData *Boots = NewOutfitPiece(EOutfitSlot::Footwear);
		TestTrue(TEXT("boots swapped in at a rest spot"), Forms->TryWearOutfitPiece(Boots));
		TestTrue(TEXT("the authored jacket stays"), Forms->GetCurrentOutfit().Get(EOutfitSlot::Jacket).Get() == AuthoredJacket);
		Forms->SetAtRestSpot(false);
		TestFalse(TEXT("away from the hub and rest spots: refused"), Forms->TryWearOutfitPiece(NewOutfitPiece(EOutfitSlot::Footwear)));
		TestTrue(TEXT("the boots stay"), Forms->GetCurrentOutfit().Get(EOutfitSlot::Footwear).Get() == Boots);

		const FOutfitSet Saved = FOutfitSet::FromSaved(Forms->GetCurrentOutfit().ToSaved());
		UCharacterFormSubsystem *Loaded = NewObject<UCharacterFormSubsystem>(GameInstance);
		Loaded->RestoreOutfit(Saved);
		TestTrue(TEXT("the outfit survives a save"), Loaded->GetCurrentOutfit() == Forms->GetCurrentOutfit());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
