// SpiritLook.h
// B-558 (D-238, one-player stage 4b): how a body shows its spirit, for the player and for the hub's recruits. A worn
// spirit (a form) puts its mesh and materials on; Male / Female Nobody is the plain body, tinted with the character's
// element or class colour (B-560: each material slot's Tint, ClassColors), the colour cleared while a spirit is worn.
// The player's own spirit is in UCharacterFormSubsystem (AWoRPlayerState::ApplyFormLook); a recruit's is on its party
// member (FPartyMember::SpiritForm, SpiritBody). Debug: wor.Party.PrintSpirits.

#pragma once

#include "CoreMinimal.h"

class ACharacter;
class UCharacterData;
class UCharacterFormData;
class UCharacterFormSubsystem;
class USkeletalMeshComponent;
class UWorld;
enum class EPlayerBaseForm : uint8;
struct FPartyMember;

namespace WoRSpiritLook
{
	/** The plain body's mesh path (Male Nobody: Manny, Female Nobody: Quinn). */
	WORLD_OF_REFRACTION_API const TCHAR *GetBodyMeshPath(EPlayerBaseForm Body);

	/** B-560: the Nobody bodies' material parameter for the body colour (MI_Manny_* / MI_Quinn_*). */
	WORLD_OF_REFRACTION_API extern const FName TINT_PARAMETER;

	/** B-560 (D-238): a body's colour: a Refractor its element's (Broken Darkness its own), a Generic or Resonator its
	 *  class's (ClassColors, the table the element debug colour reads). */
	WORLD_OF_REFRACTION_API FLinearColor GetBodyColour(const UCharacterData *Character);

	/** Mesh wears Form, else the plain Body, tinted with Tint when given. False, changing nothing, when there is no
	 *  mesh to wear. */
	WORLD_OF_REFRACTION_API bool Dress(USkeletalMeshComponent &Mesh, const UCharacterFormData *Form, EPlayerBaseForm Body,
									   const FLinearColor *Tint);

	/** A recruit member (Picks.bRecruit) wears its spirit on Pawn. False for any other member. */
	WORLD_OF_REFRACTION_API bool DressMember(ACharacter &Pawn, const FPartyMember &Member, const UCharacterFormSubsystem *Forms);

	/** Dresses every body standing for Member in World: its pawn (by character key) and its placed hub recruit. */
	WORLD_OF_REFRACTION_API void DressMemberBodies(UWorld *World, const FPartyMember &Member);

	/** Debug: "Fire Spirit: Male Nobody (its colour)" or the worn spirit's name. */
	WORLD_OF_REFRACTION_API FString GetMemberSpiritString(const FPartyMember &Member);
}
