// PieMenuButtonData.h
// Data structures for Dynamic PIE Menu integration
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "PieMenuButtonData.generated.h"

class UTexture2D;
class UObject;

/**
 * Spell schools for the Schools sub-menu
 * 4 schools total
 */
UENUM(BlueprintType)
enum class EPieMenuSpellSchool : uint8
{
	Destruction UMETA(DisplayName = "Destruction"),
	Conjuration UMETA(DisplayName = "Conjuration"),
	Enhancement UMETA(DisplayName = "Enhancement"),
	Restoration UMETA(DisplayName = "Restoration")
};

/**
 * Menu button category for routing selection handling
 */
UENUM(BlueprintType)
enum class EPieMenuCategory : uint8
{
	None UMETA(DisplayName = "None"),

	// Main menu actions
	Attack UMETA(DisplayName = "Attack"),				   // Basic attack with current weapon
	Abilities UMETA(DisplayName = "Abilities"),			   // Opens ability grid (Generic)
	Refractions UMETA(DisplayName = "Refractions"),		   // Opens schools list (Caster innate spells)
	Breakthrough UMETA(DisplayName = "Breakthrough"),	   // Opens schools list (Character evolution spells)
	ResonateWeapon UMETA(DisplayName = "Resonate Weapon"), // Opens schools list (Evolved weapon spells - "(W)")
	ResonateRing UMETA(DisplayName = "Resonate Ring"),	   // Opens schools list (Ring spells - "(R)")
	ChangeRing UMETA(DisplayName = "Change Ring"),		   // Opens ring grid (Resonator)
	Infuse UMETA(DisplayName = "Infuse"),				   // Opens infusion source grid
	Items UMETA(DisplayName = "Items"),					   // Opens item grid (All)
	SwitchWeapon UMETA(DisplayName = "Switch Weapon"),	   // Toggles bUsePrimary (Generic dual-wield only)

	// Sub-menu categories
	School UMETA(DisplayName = "School"),				   // A spell school button
	Spell UMETA(DisplayName = "Spell"),					   // A spell in the grid (max 6)
	Ability UMETA(DisplayName = "Ability"),				   // An ability in the grid
	Ring UMETA(DisplayName = "Ring"),					   // A ring in the grid (max 5)
	InfusionSource UMETA(DisplayName = "Infusion Source"), // An infusion source option
	Item UMETA(DisplayName = "Item"),					   // An item in the grid

	// Navigation
	Back UMETA(DisplayName = "Back") // Navigate back
};

/**
 * Current menu state for navigation
 */
UENUM(BlueprintType)
enum class EPieMenuState : uint8
{
	Closed UMETA(DisplayName = "Closed"),
	Main UMETA(DisplayName = "Main Menu"),
	Schools UMETA(DisplayName = "Schools List"),
	SpellGrid UMETA(DisplayName = "Spell Grid"), // Max 6 spells
	AbilityGrid UMETA(DisplayName = "Ability Grid"),
	RingGrid UMETA(DisplayName = "Ring Grid"), // Max 5 rings
	ItemGrid UMETA(DisplayName = "Item Grid"),
	InfusionGrid UMETA(DisplayName = "Infusion Grid") // Infusion source selection
};

/**
 * Data for a single pie menu button
 * Maps to plugin's SButon Information struct in Blueprint
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FPieMenuButtonData
{
	GENERATED_BODY()

	/** Unique identifier for this button (matches ButtonID in plugin) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FString ButtonID;

	/** Display name shown on the button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText DisplayName;

	/** Description shown when hovered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText Description;

	/** Icon texture for the button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	UTexture2D *Icon = nullptr;

	/** Tint color for the button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FLinearColor ButtonTint = FLinearColor::White;

	/** Is this button interactable? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bEnabled = true;

	/** Menu category for routing selection handling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	EPieMenuCategory Category = EPieMenuCategory::None;

	/** Spell school (when Category == School) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	EPieMenuSpellSchool School = EPieMenuSpellSchool::Destruction;

	/** Reference to underlying data asset (spell, ability, item, ring, weapon) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	UObject *DataReference = nullptr;

	/** Index in the source array */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	int32 DataIndex = -1;

	// ==================== CONSTRUCTORS ====================

	FPieMenuButtonData() = default;

	/** Main menu button constructor */
	FPieMenuButtonData(const FString &InID, const FText &InName, EPieMenuCategory InCategory)
		: ButtonID(InID), DisplayName(InName), Category(InCategory)
	{
	}

	/** Full constructor */
	FPieMenuButtonData(const FString &InID, const FText &InName, const FText &InDesc,
					   EPieMenuCategory InCategory, UTexture2D *InIcon = nullptr)
		: ButtonID(InID), DisplayName(InName), Description(InDesc), Icon(InIcon), Category(InCategory)
	{
	}

	// ==================== STATIC BUILDERS ====================

	/** Create a school button */
	static FPieMenuButtonData MakeSchoolButton(EPieMenuSpellSchool InSchool, int32 SpellCount)
	{
		FPieMenuButtonData Button;
		Button.Category = EPieMenuCategory::School;
		Button.School = InSchool;
		Button.DataIndex = static_cast<int32>(InSchool);

		switch (InSchool)
		{
		case EPieMenuSpellSchool::Destruction:
			Button.ButtonID = TEXT("School_Destruction");
			Button.DisplayName = FText::FromString(TEXT("Destruction"));
			Button.Description = FText::FromString(FString::Printf(TEXT("%d spells"), SpellCount));
			Button.ButtonTint = FLinearColor(1.0f, 0.3f, 0.1f, 1.0f); // Orange-red
			break;
		case EPieMenuSpellSchool::Conjuration:
			Button.ButtonID = TEXT("School_Conjuration");
			Button.DisplayName = FText::FromString(TEXT("Conjuration"));
			Button.Description = FText::FromString(FString::Printf(TEXT("%d spells"), SpellCount));
			Button.ButtonTint = FLinearColor(0.5f, 0.2f, 0.8f, 1.0f); // Purple
			break;
		case EPieMenuSpellSchool::Enhancement:
			Button.ButtonID = TEXT("School_Enhancement");
			Button.DisplayName = FText::FromString(TEXT("Enhancement"));
			Button.Description = FText::FromString(FString::Printf(TEXT("%d spells"), SpellCount));
			Button.ButtonTint = FLinearColor(0.2f, 0.8f, 0.3f, 1.0f); // Green
			break;
		case EPieMenuSpellSchool::Restoration:
			Button.ButtonID = TEXT("School_Restoration");
			Button.DisplayName = FText::FromString(TEXT("Restoration"));
			Button.Description = FText::FromString(FString::Printf(TEXT("%d spells"), SpellCount));
			Button.ButtonTint = FLinearColor(0.9f, 0.9f, 0.5f, 1.0f); // Yellow
			break;
		}

		Button.bEnabled = SpellCount > 0;
		return Button;
	}

	/** Create a data-referenced button (spell, ability, item, ring) */
	static FPieMenuButtonData MakeDataButton(const FString &InID, const FText &InName,
											 EPieMenuCategory InCategory, UObject *InData,
											 int32 InIndex = -1, UTexture2D *InIcon = nullptr)
	{
		FPieMenuButtonData Button;
		Button.ButtonID = InID;
		Button.DisplayName = InName;
		Button.Category = InCategory;
		Button.DataReference = InData;
		Button.DataIndex = InIndex;
		Button.Icon = InIcon;
		return Button;
	}

	/** Create a disabled button with reason */
	static FPieMenuButtonData MakeDisabledButton(const FString &InID, const FText &InName,
												 const FText &InReason, EPieMenuCategory InCategory = EPieMenuCategory::None)
	{
		FPieMenuButtonData Button;
		Button.ButtonID = InID;
		Button.DisplayName = InName;
		Button.Description = InReason;
		Button.Category = InCategory;
		Button.bEnabled = false;
		Button.ButtonTint = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // Greyed out
		return Button;
	}

	/** Create back button */
	static FPieMenuButtonData MakeBackButton()
	{
		FPieMenuButtonData Button;
		Button.ButtonID = TEXT("Back");
		Button.DisplayName = FText::FromString(TEXT("Back"));
		Button.Description = FText::FromString(TEXT("Return to previous menu"));
		Button.Category = EPieMenuCategory::Back;
		Button.bEnabled = true;
		return Button;
	}
};

/**
 * Delegate fired when a pie menu button is selected
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPieMenuButtonSelected, const FPieMenuButtonData &, ButtonData);

/**
 * Delegate fired when menu state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPieMenuStateChanged, EPieMenuState, OldState, EPieMenuState, NewState);
