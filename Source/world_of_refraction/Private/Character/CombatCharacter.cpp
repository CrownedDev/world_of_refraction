// CombatCharacter.cpp

#include "Character/CombatCharacter.h"

#include "Character/CharacterDataComponent.h"

ACombatCharacter::ACombatCharacter()
{
    CharacterDataComponent = CreateDefaultSubobject<UCharacterDataComponent>(TEXT("CharacterDataComponent"));
}
