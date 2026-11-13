// InteractPrompt.h
// B-443 (D-181, UIScreens #14): what the floating interact prompt says about the thing the player faces:
// a verb and what it is ("Talk" + "Blacksmith"; "Rest"; "Open" + "Chest"). Each interactable answers for itself;
// the verbs' words are one name table (WoRGameNames::GetInteractVerbName, D-062).

#pragma once

#include "CoreMinimal.h"

/** The prompt's verbs. None = no prompt (nothing to do here now, or a thing that must stay unannounced). */
enum class EInteractVerb : uint8
{
	None,
	Talk,
	Enter,
	Return,
	Invite,
	Dismiss,
	Open,
	Read,
	Challenge,
	PickUp,
	Rest
};

struct FInteractPrompt
{
	EInteractVerb Verb = EInteractVerb::None;
	/** What the verb acts on; may be empty ("Rest"). */
	FText What;

	FInteractPrompt() = default;
	FInteractPrompt(EInteractVerb InVerb, const FText &InWhat = FText::GetEmpty()) : Verb(InVerb), What(InWhat) {}

	bool IsEmpty() const { return Verb == EInteractVerb::None; }
};
