// GearSockets.h
// B-351 (D-141): every weapon and adapter has two sockets. The ELEMENT socket holds a gem (refined crystal) or an
// evolution; the STONE socket holds an augment stone. A fusion fits neither (fusions retire, B-352). The element
// socket is the gear entry's AttachedItem; the stone socket is its StoneSocket.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/EAttachedItemKind.h"

enum class EGearSocket : uint8
{
	None,
	Element,
	Stone,
};

namespace GearSockets
{
	/** The socket an attachment of this kind goes in; None for an empty kind or a fusion (D-141). */
	inline EGearSocket SocketFor(EAttachedItemKind Kind)
	{
		switch (Kind)
		{
		case EAttachedItemKind::Crystal:
		case EAttachedItemKind::Evolution:
			return EGearSocket::Element;
		case EAttachedItemKind::AugmentStone:
			return EGearSocket::Stone;
		default:
			return EGearSocket::None;
		}
	}

	/** D-231 (B-532): a prebuilt piece's own crystal or evolution stays in unless the prebuilt was made removable.
	 *  Taking off either socket clears both (CL-28b), so a fixed piece's stone stays too. The one rule the socket
	 *  removals and the loadout screen read. TGear: a weapon or ring entry, or an FGearInstance. */
	template <typename TGear>
	bool IsSocketFixed(const TGear &Gear)
	{
		return Gear.bPrebuilt && !Gear.bCrystalRemovable && !Gear.AttachedItem.IsEmpty();
	}

	inline const TCHAR *GetSocketName(EGearSocket Socket)
	{
		switch (Socket)
		{
		case EGearSocket::Element:
			return TEXT("element");
		case EGearSocket::Stone:
			return TEXT("stone");
		default:
			return TEXT("none");
		}
	}
}
