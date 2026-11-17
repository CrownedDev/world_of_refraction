// RecruitSpiritSpec.cpp
// B-558 (D-238, one-player stage 4b): a recruit wears its own spirit from the player's unlocks. The Spirit screen's page
// shows the worn spirit and body it is given (the recruit's, on its party member); the member saves them.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Character/CharacterData.h"
#include "Character/Forms/CharacterFormData.h"
#include "Character/Forms/CharacterFormSubsystem.h"
#include "Character/Forms/SpiritLook.h"
#include "Engine/GameInstance.h"
#include "Infusion/ClassColors.h"
#include "Infusion/ElementColors.h"
#include "Party/Party.h"
#include "Save/SaveSubsystem.h"
#include "Save/WoRSaveTypes.h"
#include "UI/Hub/FormsScreenModel.h"

namespace
{
	constexpr int32 RECRUIT_SPIRIT_MALE_ROW = 0; // Male Nobody
	constexpr int32 RECRUIT_SPIRIT_FEMALE_ROW = 1; // Female Nobody
	constexpr int32 RECRUIT_SPIRIT_FORM_ROW = 2; // the one registered form
}

BEGIN_DEFINE_SPEC(FRecruitSpiritSpec, "WoR.Party.RecruitSpirit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UCharacterFormSubsystem *Forms = nullptr;
	UCharacterFormData *Form = nullptr;
END_DEFINE_SPEC(FRecruitSpiritSpec)

void FRecruitSpiritSpec::Define()
{
	BeforeEach([this]()
	{
		Form = NewObject<UCharacterFormData>(GetTransientPackage());
		Form->Character = NewObject<UCharacterData>(GetTransientPackage());
		Form->FormName = FText::FromString(TEXT("Amethyst Prisma"));
		Forms = NewObject<UCharacterFormSubsystem>(NewObject<UGameInstance>(GetTransientPackage()));
		Forms->RegisterForm(Form);
	});

	It("shows the spirit and body it is given, not the player's", [this]()
	{
		const auto None = [](const UObject *) { return false; };
		const FFormsScreenPage OnQuinn = WoRFormsScreen::BuildPage(*Forms, FText::FromString(TEXT("Fire Spirit")), nullptr, EPlayerBaseForm::Quinn, None, None);
		if (!TestEqual(TEXT("two own spirits and the form"), OnQuinn.Rows.Num(), RECRUIT_SPIRIT_FORM_ROW + 1))
		{
			return;
		}
		TestTrue(TEXT("Female Nobody worn, Male Nobody not"), OnQuinn.Rows[RECRUIT_SPIRIT_FEMALE_ROW].bWorn && !OnQuinn.Rows[RECRUIT_SPIRIT_MALE_ROW].bWorn);
		TestEqual(TEXT("the player still wears Male Nobody"), Forms->GetBaseForm(), EPlayerBaseForm::Manny);

		const FFormsScreenPage InForm = WoRFormsScreen::BuildPage(*Forms, FText::FromString(TEXT("Fire Spirit")), Form, EPlayerBaseForm::Quinn, None, None);
		TestTrue(TEXT("the worn form, and neither body"), InForm.Rows[RECRUIT_SPIRIT_FORM_ROW].bWorn && !InForm.Rows[RECRUIT_SPIRIT_FEMALE_ROW].bWorn);
		TestTrue(TEXT("the wearing line names the form"), InForm.WearingLine.ToString().Contains(TEXT("Amethyst Prisma")));
	});

	It("colours a body by its element, Broken Darkness or class (B-560)", [this]()
	{
		UCharacterData *Fire = NewObject<UCharacterData>(GetTransientPackage());
		Fire->CharacterClass = ECharacterClass::Caster;
		Fire->InnateElement = ESpellElement::Fire;
		TestEqual(TEXT("a Fire Refractor is fire"), WoRSpiritLook::GetBodyColour(Fire), ElementColors::GetColorForElement(ESpellElement::Fire));
		UCharacterData *Broken = DuplicateObject(Fire, GetTransientPackage());
		Broken->InnateElement = ESpellElement::Darkness;
		Broken->bBrokenDarknessInnate = true;
		TestEqual(TEXT("Broken Darkness is its own"), WoRSpiritLook::GetBodyColour(Broken), ElementColors::BrokenDarkness);
		UCharacterData *Resonator = NewObject<UCharacterData>(GetTransientPackage());
		Resonator->CharacterClass = ECharacterClass::Resonator;
		TestEqual(TEXT("a Resonator its class colour"), WoRSpiritLook::GetBodyColour(Resonator), ClassColors::ResonatorSilver);
	});

	It("refuses a locked spirit by the player's unlocks, without wearing anything", [this]()
	{
		TestNotEqual(TEXT("a locked form is refused"), Forms->CanWearForm(Form), EFormSwitchRefusal::None);
		TestNull(TEXT("and the player wears nothing new"), Forms->GetWornForm());
	});

	It("names what a recruit wears, and saves it with the member", [this]()
	{
		FPartyMember Member;
		Member.Picks.Name = TEXT("Fire Spirit");
		Member.Picks.bRecruit = true;
		Member.Picks.Body = EPlayerBaseForm::Manny;
		Member.CharacterData = NewObject<UCharacterData>(GetTransientPackage());
		TestTrue(TEXT("its body in its colour"), WoRSpiritLook::GetMemberSpiritString(Member).Contains(TEXT("its colour")));
		Member.SpiritForm = Form->GetFName();
		Member.SpiritBody = EPlayerBaseForm::Quinn;
		TestTrue(TEXT("a worn spirit is named"), WoRSpiritLook::GetMemberSpiritString(Member).Contains(Form->GetName()));

		UParty *Party = NewObject<UParty>(GetTransientPackage());
		Party->Members.Add(Member);
		const FSavedParty Saved = USaveSubsystem::CaptureParty(Party, /*bKeepVitals=*/false);
		TestTrue(TEXT("saved with the member"), Saved.Members.Num() == 1 && Saved.Members[0].SpiritForm == Form->GetFName() &&
				 Saved.Members[0].SpiritBody == EPlayerBaseForm::Quinn);
	});
}

#endif
