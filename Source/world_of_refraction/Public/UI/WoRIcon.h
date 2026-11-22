// WoRIcon.h
// B-487: what an icon stands for, without a texture. A screen's model fills one for each thing it shows (a weapon's
// type, the element of a crystal, an empty socket, a stone, an effect's kind) and its widget asks the icon table for
// the texture (WoRIcons::Find, UI/WoRIconTable.h). Where the table has none, the text the icon replaces stays.
// First used by the v2 loadout screen (B-485, as FLoadoutV2Icon), then the inventory and vendor screens.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EWeaponType.h"
#include "Skills/Definitions/ESpellElement.h"
#include "Skills/Effects/ESkillEffectType.h"

enum class EWoRIconKind : uint8
{
	None,
	WeaponType,
	/** A crystal or an evolution in a socket, or an element: the element's badge, in its silver frame. */
	Element,
	EmptySocket,
	/** An augment stone. */
	Stone,
	/** An effect: by its type, in its element's colour where the table has that variant. */
	Effect,
	/** B-530 (D-229): an adapter: the socket frame, its crystal's element badge in the centre (Element None: empty). */
	Adapter,
	/** B-584 (D-256): a crystal or an evolution in no socket: the gem alone (the silver frame means slotted). */
	LooseGem
};

struct FWoRIcon
{
	EWoRIconKind Kind = EWoRIconKind::None;
	EWeaponType WeaponType = EWeaponType::Sword;
	ESpellElement Element = ESpellElement::None;
	ESkillEffectType EffectType = ESkillEffectType::None;
	/** The icon in words (its tooltip): "Sword", "Garnet", "Empty socket". */
	FText Name;
};
