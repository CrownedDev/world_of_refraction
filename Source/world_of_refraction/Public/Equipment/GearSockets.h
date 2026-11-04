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
