// IconTableSpec.cpp
// B-485: the UI icon table. An icon that is set is returned, one that isn't is null (the screen keeps its text); the
// debug string says what is set and what is missing; the active table's getters are null with no table.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "Equipment/Crystals/FCrystalId.h"
#include "Inventory/ItemEffectType.h"
#include "UI/WoRIconTable.h"

BEGIN_DEFINE_SPEC(FIconTableSpec, "WoR.UI.IconTable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
	UWoRIconTable *Table = nullptr;
	UTexture2D *Texture = nullptr;
	UTexture2D *ElementTexture = nullptr;
END_DEFINE_SPEC(FIconTableSpec)

void FIconTableSpec::Define()
{
	BeforeEach([this]()
	{
		Table = NewObject<UWoRIconTable>(GetTransientPackage());
		Texture = NewObject<UTexture2D>(GetTransientPackage());
		Table->ElementBadges.Add(ESpellElement::Fire, Texture);
		Table->TierBadges.Add(EItemTier::C_Tier, Texture);
		Table->WeaponTypes.Add(EWeaponType::Sword, Texture);
		ElementTexture = NewObject<UTexture2D>(GetTransientPackage());
		FWoREffectIcons Burn;
		Burn.Default = Texture;
		Burn.ByElement.Add(ESpellElement::Fire, ElementTexture);
		Table->EffectKinds.Add(ESkillEffectType::DOT, Burn);
		Table->EmptySocket = Texture;
	});
	AfterEach([this]()
	{
		WoRIcons::SetActiveForTests(nullptr);
		Table = nullptr;
		Texture = nullptr;
		ElementTexture = nullptr;
	});

	It("returns an icon that is set and null for one that isn't", [this]()
	{
		TestTrue(TEXT("Fire's badge"), Table->GetElementBadge(ESpellElement::Fire) == Texture);
		TestNull(TEXT("no Water badge"), Table->GetElementBadge(ESpellElement::Water));
		TestTrue(TEXT("tier C"), Table->GetTierBadge(EItemTier::C_Tier) == Texture);
		TestNull(TEXT("no tier S"), Table->GetTierBadge(EItemTier::S_Tier));
		TestTrue(TEXT("the sword"), Table->GetWeaponType(EWeaponType::Sword) == Texture);
		TestNull(TEXT("no hammer"), Table->GetWeaponType(EWeaponType::Hammer));
		TestTrue(TEXT("damage over time"), Table->GetEffectKind(ESkillEffectType::DOT) == Texture);
		TestTrue(TEXT("in Fire's colour where there is that variant"), Table->GetEffectKind(ESkillEffectType::DOT, ESpellElement::Fire) == ElementTexture);
		TestTrue(TEXT("an element without a variant takes the one for any"), Table->GetEffectKind(ESkillEffectType::DOT, ESpellElement::Water) == Texture);
		TestNull(TEXT("no skip-turn icon"), Table->GetEffectKind(ESkillEffectType::SkipTurn));
		TestTrue(TEXT("the empty socket"), Table->GetEmptySocket() == Texture);
		TestNull(TEXT("no stone icon"), Table->GetStoneSocket());
		Table->ElementBadges.Add(ESpellElement::Water, TSoftObjectPtr<UTexture2D>());
		TestNull(TEXT("an empty reference is no icon"), Table->GetElementBadge(ESpellElement::Water));
	});

	It("says which icons are set and which are missing", [this]()
	{
		const FString Report = Table->GetIconTableString();
		TestTrue(TEXT("the element badges, counted"), Report.Contains(TEXT("Element badges: 1 of")));
		TestTrue(TEXT("Fire is set"), Report.Contains(TEXT("set: Fire")));
		TestTrue(TEXT("what is missing is named"), Report.Contains(TEXT("missing:")) && Report.Contains(TEXT("Water")));
		TestTrue(TEXT("the stone socket is missing"), Report.Contains(TEXT("Stone socket: missing")));
		TestFalse(TEXT("the empty socket is not"), Report.Contains(TEXT("Empty socket: missing")));
	});

	It("finds the texture for what an icon stands for, and maps an item's effect to its icon kind (B-487)", [this]()
	{
		WoRIcons::SetActiveForTests(Table);
		FWoRIcon Fire;
		Fire.Kind = EWoRIconKind::Element;
		Fire.Element = ESpellElement::Fire;
		TestTrue(TEXT("an element's diamond"), WoRIcons::Find(Fire) == Texture);
		FWoRIcon Sword;
		Sword.Kind = EWoRIconKind::WeaponType;
		Sword.WeaponType = EWeaponType::Sword;
		TestTrue(TEXT("a weapon type"), WoRIcons::Find(Sword) == Texture);
		FWoRIcon Burn;
		Burn.Kind = EWoRIconKind::Effect;
		Burn.EffectType = ESkillEffectType::DOT;
		Burn.Element = ESpellElement::Fire;
		TestTrue(TEXT("an effect, in its element's colour"), WoRIcons::Find(Burn) == ElementTexture);
		FWoRIcon Empty;
		Empty.Kind = EWoRIconKind::EmptySocket;
		TestTrue(TEXT("the empty socket"), WoRIcons::Find(Empty) == Texture);
		FWoRIcon Stone;
		Stone.Kind = EWoRIconKind::Stone;
		TestNull(TEXT("a stone has no icon here"), WoRIcons::Find(Stone));
		TestNull(TEXT("nothing stands for nothing"), WoRIcons::Find(FWoRIcon()));

		TestTrue(TEXT("a damage crystal burns"), WoRIcons::GetItemEffectKind(EItemEffectType::Damage) == ESkillEffectType::DOT);
		TestTrue(TEXT("defy death is the last stand"), WoRIcons::GetItemEffectKind(EItemEffectType::DefyDeath) == ESkillEffectType::LastStand);
		TestTrue(TEXT("a repair has no icon kind"), WoRIcons::GetItemEffectKind(EItemEffectType::Repair) == ESkillEffectType::None);
		const FWoRIcon GarnetEffect = WoRIcons::MakeItemEffectIcon(FCrystalId(ECrystalType::Garnet, EItemTier::C_Tier));
		TestTrue(TEXT("a Garnet's effect icon: damage over time, with a name for its tooltip"),
				 GarnetEffect.Kind == EWoRIconKind::Effect && GarnetEffect.EffectType == ESkillEffectType::DOT && !GarnetEffect.Name.IsEmpty());
	});

	It("gives the active table's icons, and none from a table with nothing set", [this]()
	{
		WoRIcons::SetActiveForTests(Table);
		TestTrue(TEXT("the active table"), UWoRIconTable::GetActive() == Table);
		TestTrue(TEXT("its Fire badge"), WoRIcons::GetElementBadge(ESpellElement::Fire) == Texture);
		TestNull(TEXT("an unset icon"), WoRIcons::GetTierBadge(EItemTier::S_Tier));
		// A table with nothing set stands in for "no icons": the spec never loads the game's own table asset, which
		// may be missing, or an LFS pointer in a worktree.
		WoRIcons::SetActiveForTests(NewObject<UWoRIconTable>(GetTransientPackage()));
		TestNull(TEXT("nothing set: no badge"), WoRIcons::GetElementBadge(ESpellElement::Fire));
		TestNull(TEXT("nothing set: no weapon icon"), WoRIcons::GetWeaponType(EWeaponType::Sword));
		TestNull(TEXT("nothing set: no effect icon"), WoRIcons::GetEffectKind(ESkillEffectType::DOT));
		TestNull(TEXT("nothing set: no socket"), WoRIcons::GetEmptySocket());
	});

	It("draws an adapter as the socket frame with its crystal's badge inside (B-530, D-229)", [this]()
	{
		UTexture2D *Frame = NewObject<UTexture2D>(GetTransientPackage());
		Table->EmptySocket = Frame;
		WoRIcons::SetActiveForTests(Table);
		const FWoRIcon Fire = WoRIcons::MakeAdapterIcon(ESpellElement::Fire, FText::FromString(TEXT("Fire Ring")));
		TestTrue(TEXT("an adapter icon"), Fire.Kind == EWoRIconKind::Adapter && Fire.Name.ToString() == TEXT("Fire Ring"));
		TestTrue(TEXT("its frame is the socket frame"), WoRIcons::Find(Fire) == Frame);
		TestTrue(TEXT("its centre is the crystal's badge"), WoRIcons::FindCentre(Fire) == Texture);
		const FWoRIcon Empty = WoRIcons::MakeAdapterIcon(ESpellElement::None, FText::FromString(TEXT("Adapter Ring")));
		TestTrue(TEXT("an empty adapter: the frame"), WoRIcons::Find(Empty) == Frame);
		TestNull(TEXT("with an empty centre"), WoRIcons::FindCentre(Empty));
		FWoRIcon Badge;
		Badge.Kind = EWoRIconKind::Element;
		Badge.Element = ESpellElement::Fire;
		TestNull(TEXT("no other icon has a centre"), WoRIcons::FindCentre(Badge));
	});

	It("draws a crystal out of a socket without the frame (B-584, D-256)", [this]()
	{
		WoRIcons::SetActiveForTests(Table);
		FWoRIcon Slotted;
		Slotted.Kind = EWoRIconKind::Element;
		Slotted.Element = ESpellElement::Fire;
		const FWoRIcon Loose = WoRIcons::AsLoose(Slotted);
		TestTrue(TEXT("out of a socket it is a loose gem"), Loose.Kind == EWoRIconKind::LooseGem && Loose.Element == ESpellElement::Fire);
		TestTrue(TEXT("no gem picture yet: the badge stands in"), WoRIcons::Find(Loose) == Texture);
		UTexture2D *Gem = NewObject<UTexture2D>(GetTransientPackage());
		Table->LooseGems.Add(ESpellElement::Fire, Gem);
		TestTrue(TEXT("the gem alone"), WoRIcons::Find(Loose) == Gem);
		TestTrue(TEXT("in a socket it keeps the frame"), WoRIcons::Find(Slotted) == Texture);
		TestTrue(TEXT("another icon stays as it is"), WoRIcons::AsLoose(WoRIcons::MakeAdapterIcon(ESpellElement::Fire, FText())).Kind == EWoRIconKind::Adapter);
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
