// CommandMenuTypes.h
// Data structures for Kingdom Hearts-style command menu
// World of Refraction - Combat UI

#pragma once

#include "CoreMinimal.h"
#include "SpellSchool.h" // Use existing ESpellSchool enum
#include "CommandMenuTypes.generated.h"

// Forward declarations
class USpellData;
class UAbilityData;
class UItemData;
class URingData;
class UTexture2D;

/**
 * Menu state for the command menu state machine
 */
UENUM(BlueprintType)
enum class ECommandMenuState : uint8
{
	Closed			UMETA(DisplayName = "Closed"),
	Main			UMETA(DisplayName = "Main Menu"),
	Schools			UMETA(DisplayName = "Spell Schools"),
	SpellGrid		UMETA(DisplayName = "Spell Grid"),
	AbilityGrid		UMETA(DisplayName = "Ability Grid"),
	ItemGrid		UMETA(DisplayName = "Item Grid"),
	RingGrid		UMETA(DisplayName = "Ring Grid")
};

/**
 * Command types available in the main menu
 * Varies by character class (Generic/Caster/Resonator)
 */
UENUM(BlueprintType)
enum class ECommandType : uint8
{
	None			UMETA(DisplayName = "None"),
	Attack			UMETA(DisplayName = "Attack"),
	Refractions		UMETA(DisplayName = "Refractions"),	// Caster spell access
	Resonate		UMETA(DisplayName = "Resonate"),		// Resonator spell access
	Abilities		UMETA(DisplayName = "Abilities"),		// Generic ability access
	ChangeRing		UMETA(DisplayName = "Change Ring"),		// Resonator ring switching
	Items			UMETA(DisplayName = "Items")
};

/**
 * Grid content type for tracking what's displayed
 */
UENUM(BlueprintType)
enum class EGridType : uint8
{
	None			UMETA(DisplayName = "None"),
	Spells			UMETA(DisplayName = "Spells"),
	Abilities		UMETA(DisplayName = "Abilities"),
	Items			UMETA(DisplayName = "Items"),
	Rings			UMETA(DisplayName = "Rings")
};

/**
 * Data for populating a command button
 */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FCommandButtonData
{
	GENERATED_BODY()

	/** Display text for the button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText DisplayName;

	/** Optional icon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	UTexture2D* Icon = nullptr;

	/** Is this button interactable? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bIsEnabled = true;

	/** Is this button currently selected/highlighted? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bIsSelected = false;

	/** For rings: is this the currently equipped ring? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bIsCurrentlyEquipped = false;

	/** Command type (for main menu buttons) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	ECommandType CommandType = ECommandType::None;

	/** Spell school (for school list buttons) - defaults to Destruction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	ESpellSchool SpellSchool = ESpellSchool::Destruction;
	
	/** Is this button for a spell school? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bIsSpellSchoolButton = false;

	/** Reference to the underlying data asset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	UObject* DataReference = nullptr;

	// Convenience constructors
	FCommandButtonData() = default;

	FCommandButtonData(const FText& InName, ECommandType InType)
		: DisplayName(InName)
		, CommandType(InType)
	{
	}

	FCommandButtonData(const FText& InName, ESpellSchool InSchool)
		: DisplayName(InName)
		, SpellSchool(InSchool)
		, bIsSpellSchoolButton(true)
	{
	}

	FCommandButtonData(const FText& InName, UObject* InData, UTexture2D* InIcon = nullptr)
		: DisplayName(InName)
		, Icon(InIcon)
		, DataReference(InData)
	{
	}
};

/**
 * Delegate fired when a command button is clicked
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommandButtonClicked, const FCommandButtonData&, ButtonData);

/**
 * Delegate fired when selection changes in a list or grid
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSelectionChanged, int32, OldIndex, int32, NewIndex);

/**
 * Delegate fired when menu state changes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMenuStateChanged, ECommandMenuState, OldState, ECommandMenuState, NewState);

/**
 * Delegate fired when a command is selected and should be executed
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommandSelected, ECommandType, Command);

/**
 * Delegate fired when a spell is selected
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpellSelected, USpellData*, Spell);

/**
 * Delegate fired when an ability is selected
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilitySelected, UAbilityData*, Ability);

/**
 * Delegate fired when an item is selected
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemSelected, UItemData*, Item);

/**
 * Delegate fired when a ring is selected (Resonator ring switch)
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRingSelected, URingData*, Ring);

/**
 * Delegate fired when menu opens/closes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCommandMenuOpened);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCommandMenuClosed);

/**
 * Helper functions for command menu
 */
namespace CommandMenuHelpers
{
	/** Get display name for a spell school */
	inline FString GetSchoolDisplayName(ESpellSchool School)
	{
		switch (School)
		{
		case ESpellSchool::Destruction:	return TEXT("Destruction");
		case ESpellSchool::Conjuration:	return TEXT("Conjuration");
		case ESpellSchool::Enhancement:	return TEXT("Enhancement");
		case ESpellSchool::Restoration:	return TEXT("Restoration");
		default: return TEXT("Unknown");
		}
	}

	/** Get display name for a command type */
	inline FString GetCommandDisplayName(ECommandType Command)
	{
		switch (Command)
		{
		case ECommandType::Attack:		return TEXT("Attack");
		case ECommandType::Refractions:	return TEXT("Refractions");
		case ECommandType::Resonate:	return TEXT("Resonate");
		case ECommandType::Abilities:	return TEXT("Abilities");
		case ECommandType::ChangeRing:	return TEXT("Change Ring");
		case ECommandType::Items:		return TEXT("Items");
		default: return TEXT("Unknown");
		}
	}

	/** Get all spell schools as array */
	inline TArray<ESpellSchool> GetAllSpellSchools()
	{
		return {
			ESpellSchool::Destruction,
			ESpellSchool::Conjuration,
			ESpellSchool::Enhancement,
			ESpellSchool::Restoration
		};
	}
}
