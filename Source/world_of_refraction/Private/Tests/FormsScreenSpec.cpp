// FormsScreenSpec.cpp
// B-444: the "Spirit of {character}" screen's model. Your own form first and worn by default; a form's meter and
// unlock; a character not met is "???" with no pieces; their pieces named once owned or worn, the rest "?", all named
// once unlocked. The screen is the PIE sheet.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Engine/GameInstance.h"
#include "Equipment/Weapons/WeaponData.h"
#include "Inventory/InventoryData.h"
#include "UI/Hub/FormsScreenModel.h"

namespace
{
	constexpr float FORMS_SCREEN_TEST_PARTIAL = 0.5f;
	constexpr int32 FORMS_SCREEN_TEST_OWN_ROW = 0;
	constexpr int32 FORMS_SCREEN_TEST_FORM_ROW = 1;
}

BEGIN_DEFINE_SPEC(FFormsScreenSpec, "WoR.UI.FormsScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UCharacterFormSubsystem *Forms = nullptr;
	UCharacterFormData *Form = nullptr;
	UWeaponData *Blade = nullptr;
	UWeaponData *Hammer = nullptr;

	FFormsScreenPage Build(const UObject *Owned)
	{
		return WoRFormsScreen::BuildPage(*Forms, FText::FromString(TEXT("Sera")), [Owned](const UObject *Asset) { return Asset == Owned; },
										 [](const UObject *) { return false; });
	}
END_DEFINE_SPEC(FFormsScreenSpec)

void FFormsScreenSpec::Define()
{
	BeforeEach([this]()
	{
		Blade = NewObject<UWeaponData>(GetTransientPackage());
		Hammer = NewObject<UWeaponData>(GetTransientPackage());
		UInventoryData *Inventory = NewObject<UInventoryData>(GetTransientPackage());
		Inventory->Weapons = {Blade, Hammer};
		UCharacterData *Character = NewObject<UCharacterData>(GetTransientPackage());
		Character->Inventory = Inventory;
		Form = NewObject<UCharacterFormData>(GetTransientPackage());
		Form->Character = Character;
		Form->FormName = FText::FromString(TEXT("Amethyst Prisma"));
		Forms = NewObject<UCharacterFormSubsystem>(NewObject<UGameInstance>(GetTransientPackage()));
		Forms->RegisterForm(Form);
	});

	It("titles the screen for the character and puts your own form first, worn", [this]()
	{
		const FFormsScreenPage Page = Build(nullptr);
		TestTrue(TEXT("Spirit of Sera"), Page.Title.ToString().Contains(TEXT("Sera")));
		if (!TestEqual(TEXT("your own form, then the one form"), Page.Rows.Num(), 2))
		{
			return;
		}
		const FFormsScreenRow &Own = Page.Rows[FORMS_SCREEN_TEST_OWN_ROW];
		TestNull(TEXT("your own form has no form asset"), Own.Form);
		TestTrue(TEXT("worn while no skin is"), Own.bWorn);
		TestFalse(TEXT("the wearing line names it"), Page.WearingLine.IsEmpty());
	});

	It("shows a character not met as ??? with no pieces, until one of their pieces is owned", [this]()
	{
		const FFormsScreenRow Unmet = Build(nullptr).Rows[FORMS_SCREEN_TEST_FORM_ROW];
		TestFalse(TEXT("not met"), Unmet.bMet);
		TestEqual(TEXT("???"), Unmet.Name.ToString(), FString(TEXT("???")));
		TestEqual(TEXT("no pieces shown"), Unmet.Pieces.Num(), 0);

		const FFormsScreenRow Met = Build(Blade).Rows[FORMS_SCREEN_TEST_FORM_ROW];
		TestTrue(TEXT("owning a piece meets them"), Met.bMet);
		TestTrue(TEXT("named"), Met.Name.EqualTo(Form->FormName));
		if (!TestEqual(TEXT("their two pieces"), Met.Pieces.Num(), 2))
		{
			return;
		}
		TestTrue(TEXT("the owned one is known"), Met.Pieces[0].bKnown && Met.Pieces[0].bOwned);
		TestFalse(TEXT("the other is ?"), Met.Pieces[1].bKnown);
		TestEqual(TEXT("shown as ?"), Met.Pieces[1].Name.ToString(), FString(TEXT("?")));
	});

	It("meets a character once the meter moves, and names every piece once unlocked", [this]()
	{
		Forms->RestoreProgress({{Form->GetFName(), FORMS_SCREEN_TEST_PARTIAL}});
		const FFormsScreenRow Partial = Build(nullptr).Rows[FORMS_SCREEN_TEST_FORM_ROW];
		TestTrue(TEXT("a moved meter meets them"), Partial.bMet);
		TestEqual(TEXT("the meter"), Partial.Progress, FORMS_SCREEN_TEST_PARTIAL);
		TestFalse(TEXT("locked"), Partial.bUnlocked);

		Forms->RestoreProgress({{Form->GetFName(), CharacterFormMeter::FULL}});
		const FFormsScreenRow Unlocked = Build(nullptr).Rows[FORMS_SCREEN_TEST_FORM_ROW];
		TestTrue(TEXT("unlocked"), Unlocked.bUnlocked);
		TestTrue(TEXT("the unlock reveals every piece"),
				 Unlocked.Pieces.Num() == 2 && Unlocked.Pieces[0].bKnown && Unlocked.Pieces[1].bKnown);
	});
}

#endif
