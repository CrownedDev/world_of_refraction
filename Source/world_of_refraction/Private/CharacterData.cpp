// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterData.h"
#include "BaseAttackData.h"
#include "EvolutionData.h"

// Implementation is mostly in header (inline functions)
// Add any non-inline implementations here if needed

ESpellElement UCharacterData::GetEvolutionElement() const
{
    if (!bIsEvolved || !ActiveEvolution)
    {
        return ESpellElement::Generic;
    }
    return ActiveEvolution->Element;
}