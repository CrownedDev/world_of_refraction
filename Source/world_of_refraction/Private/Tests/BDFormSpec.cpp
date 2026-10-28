// BDFormSpec.cpp
// B-255 (DRY audit A15): one "is this actor in Broken Darkness form" check, UBrokenDarknessManager::IsInBDForm,
// read by the executor and the character panel alike. False with no BD manager or before the transformation;
// true once a born-BD character's manager has flipped, false again after a revert. The panel reads the
// manager's own flag; the character component's IsBrokenDarkness() moves with it (BrokenDarknessManager.cpp).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WoRTestWorld.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Combat/Mechanics/BrokenDarknessManager.h"

BEGIN_DEFINE_SPEC(FBDFormSpec, "WoR.BrokenDarkness.FormCheck", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	TUniquePtr<FWoRTestWorld> TestWorld;
END_DEFINE_SPEC(FBDFormSpec)

void FBDFormSpec::Define()
{
	BeforeEach([this]()
	{
		TestWorld = MakeUnique<FWoRTestWorld>();
		TestTrue(TEXT("Test world created"), TestWorld->Create());
	});

	AfterEach([this]() { TestWorld.Reset(); });

	It("is false with no BD manager, or a manager that hasn't transformed", [this]()
	{
		AActor *Plain = TestWorld->SpawnCombatant(FWoRTestWorld::NewCharacterData());
		if (!TestNotNull(TEXT("Plain"), Plain)) return;
		TestFalse(TEXT("no manager"), UBrokenDarknessManager::IsInBDForm(Plain));
		FWoRTestWorld::AddComponent<UBrokenDarknessManager>(Plain);
		TestFalse(TEXT("not transformed"), UBrokenDarknessManager::IsInBDForm(Plain));
		TestFalse(TEXT("null actor"), UBrokenDarknessManager::IsInBDForm(nullptr));
	});

	It("is true for a born Broken Darkness character once its manager flips", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->bBrokenDarknessInnate = true;
		Data->InnateElement = ESpellElement::Darkness;
		AActor *Born = TestWorld->SpawnCombatant(Data);
		if (!TestNotNull(TEXT("Born"), Born)) return;
		UBrokenDarknessManager *Manager = FWoRTestWorld::AddComponent<UBrokenDarknessManager>(Born);
		Manager->InitializeBornBrokenDarkness();
		TestTrue(TEXT("the character is BD"), Born->FindComponentByClass<UCharacterDataComponent>()->IsBrokenDarkness());
		TestTrue(TEXT("in BD form"), UBrokenDarknessManager::IsInBDForm(Born));
	});

	It("is false again once the transformation reverts", [this]()
	{
		UCharacterData *Data = FWoRTestWorld::NewCharacterData();
		Data->bBrokenDarknessInnate = true;
		Data->InnateElement = ESpellElement::Darkness;
		AActor *Born = TestWorld->SpawnCombatant(Data);
		if (!TestNotNull(TEXT("Born"), Born)) return;
		UBrokenDarknessManager *Manager = FWoRTestWorld::AddComponent<UBrokenDarknessManager>(Born);
		Manager->InitializeBornBrokenDarkness();
		Manager->RevertTransformation();
		TestFalse(TEXT("reverted"), UBrokenDarknessManager::IsInBDForm(Born));
		TestEqual(TEXT("the panel's flag agrees"), Manager->IsTransformed(), UBrokenDarknessManager::IsInBDForm(Born));
	});
}

#endif
