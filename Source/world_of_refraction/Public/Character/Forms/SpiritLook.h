// SpiritLook.h
// B-558 (D-238, one-player stage 4b): how a body shows its spirit, for the player and for the hub's recruits. A worn
// spirit (a form) puts its mesh and materials on; Male / Female Nobody is the plain body. A recruit on its starting body
// (FPlayerPicks::Body) with no spirit shows its Blueprint's own look, which carries its element or class colour.
// The player's own spirit is in UCharacterFormSubsystem (AWoRPlayerState::ApplyFormLook); a recruit's is on its party
// member (FPartyMember::SpiritForm, SpiritBody). Debug: wor.Party.PrintSpirits.

#pragma once

#include "CoreMinimal.h"

class ACharacter;
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

	/** Mesh wears Form, else Body: OwnLook's mesh and materials when given (a recruit's own Blueprint look), else the
	 *  plain body. False, changing nothing, when there is no mesh to wear. */
	WORLD_OF_REFRACTION_API bool Dress(USkeletalMeshComponent &Mesh, const UCharacterFormData *Form, EPlayerBaseForm Body,
									   const USkeletalMeshComponent *OwnLook);

	/** A recruit member (Picks.bRecruit) wears its spirit on Pawn. False for any other member. */
	WORLD_OF_REFRACTION_API bool DressMember(ACharacter &Pawn, const FPartyMember &Member, const UCharacterFormSubsystem *Forms);

	/** Dresses every body standing for Member in World: its pawn (by character key) and its placed hub recruit. */
	WORLD_OF_REFRACTION_API void DressMemberBodies(UWorld *World, const FPartyMember &Member);

	/** Debug: "Fire Spirit: Male Nobody (own look)" or the worn spirit's name. */
	WORLD_OF_REFRACTION_API FString GetMemberSpiritString(const FPartyMember &Member);
}
