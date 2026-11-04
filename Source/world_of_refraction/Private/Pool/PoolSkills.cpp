// PoolSkills.cpp
// B-316b (D-111, Q-316, Q-316b): each character's learned skills in the store, UPoolSubsystem's skills half, kept out
// of PoolSubsystem.cpp. A member banks its asset's skills on its first draw and loads them after; a give moves one
// instance between two characters.

#include "Pool/PoolSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/InventoryConstants.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"

namespace
{
    /** Moves the instance of Skill from From's list to To's (one copy). False when From lacks it, To has it, or To
     *  is full. TInstance is FSpellInstance or FAbilityInstance; GetSkill reads its skill pointer. */
    template <class TInstance, class TSkill, class TGetSkill>
    bool MoveSkillInstance(TArray<TInstance> &From, TArray<TInstance> &To, const TSkill *Skill, int32 Capacity, TGetSkill GetSkill)
    {
        const int32 Index = From.IndexOfByPredicate([&](const TInstance &I) { return GetSkill(I) == Skill; });
        if (!Skill || Index == INDEX_NONE || To.Num() >= Capacity ||
            To.ContainsByPredicate([&](const TInstance &I) { return GetSkill(I) == Skill; }))
        {
            return false;
        }
        To.Add(From[Index]);
        From.RemoveAt(Index);
        return true;
    }

    /** wor.Pool.PrintSkills: every character's skills in the store. */
    FAutoConsoleCommandWithWorld GPoolPrintSkillsCmd(
        TEXT("wor.Pool.PrintSkills"),
        TEXT("DEBUG: each character's learned spells and abilities in the store (B-316b)."),
        FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld *World)
        {
            const UGameInstance *GI = World ? World->GetGameInstance() : nullptr;
            const UPoolSubsystem *Pool = GI ? GI->GetSubsystem<UPoolSubsystem>() : nullptr;
            if (!Pool)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Pool] wor.Pool.PrintSkills: no pool subsystem (no PIE world?)."));
                return;
            }
            UE_LOG(LogTemp, Log, TEXT("%s"), *Pool->GetCharacterSkillsString());
        }));
}

bool UPoolSubsystem::BankSkills(FName CharacterKey, const TArray<FSpellInstance> &Spells, const TArray<FAbilityInstance> &Abilities)
{
    if (CharacterKey.IsNone() || CharacterSkills.Contains(CharacterKey))
    {
        return false;
    }
    FPoolSkills &Skills = CharacterSkills.Add(CharacterKey);
    Skills.Spells = Spells;
    Skills.Abilities = Abilities;
    UE_LOG(LogTemp, Log, TEXT("[Pool] %s's skills banked: %d spell(s), %d ability(ies)"), *CharacterKey.ToString(),
           Spells.Num(), Abilities.Num());
    return true;
}

void UPoolSubsystem::SetSkills(FName CharacterKey, const TArray<FSpellInstance> &Spells, const TArray<FAbilityInstance> &Abilities)
{
    FPoolSkills *Skills = CharacterSkills.Find(CharacterKey);
    if (!Skills)
    {
        return;
    }
    Skills->Spells = Spells;
    Skills->Abilities = Abilities;
    OnPoolSkillsChanged.Broadcast(CharacterKey);
}

void UPoolSubsystem::RestoreCharacterSkills(const TMap<FName, FPoolSkills> &Skills)
{
    CharacterSkills = Skills;
    TArray<FName> Keys;
    CharacterSkills.GetKeys(Keys); // a listener may bank a new character: don't broadcast while iterating the map
    for (const FName Key : Keys)
    {
        OnPoolSkillsChanged.Broadcast(Key);
    }
}

bool UPoolSubsystem::KnowsSpell(FName CharacterKey, const USpellData *Spell) const
{
    const FPoolSkills *Skills = CharacterSkills.Find(CharacterKey);
    return Spell && Skills && Skills->Spells.ContainsByPredicate([Spell](const FSpellInstance &I) { return I.Spell == Spell; });
}

bool UPoolSubsystem::KnowsAbility(FName CharacterKey, const UAbilityData *Ability) const
{
    const FPoolSkills *Skills = CharacterSkills.Find(CharacterKey);
    return Ability && Skills && Skills->Abilities.ContainsByPredicate([Ability](const FAbilityInstance &I) { return I.Ability == Ability; });
}

bool UPoolSubsystem::MoveSpell(FName FromKey, FName ToKey, const USpellData *Spell)
{
    FPoolSkills *From = CharacterSkills.Find(FromKey);
    FPoolSkills *To = CharacterSkills.Find(ToKey);
    if (!From || !To || FromKey == ToKey ||
        !MoveSkillInstance(From->Spells, To->Spells, Spell, InventoryConstants::MAX_LEARNED_SPELLS,
                           [](const FSpellInstance &I) { return I.Spell; }))
    {
        return false;
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Spell %s moved from %s to %s"), *GetNameSafe(Spell), *FromKey.ToString(), *ToKey.ToString());
    OnPoolSkillsChanged.Broadcast(FromKey);
    OnPoolSkillsChanged.Broadcast(ToKey);
    return true;
}

bool UPoolSubsystem::MoveAbility(FName FromKey, FName ToKey, const UAbilityData *Ability)
{
    FPoolSkills *From = CharacterSkills.Find(FromKey);
    FPoolSkills *To = CharacterSkills.Find(ToKey);
    if (!From || !To || FromKey == ToKey ||
        !MoveSkillInstance(From->Abilities, To->Abilities, Ability, InventoryConstants::MAX_LEARNED_ABILITIES,
                           [](const FAbilityInstance &I) { return I.Ability; }))
    {
        return false;
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] Ability %s moved from %s to %s"), *GetNameSafe(Ability), *FromKey.ToString(), *ToKey.ToString());
    OnPoolSkillsChanged.Broadcast(FromKey);
    OnPoolSkillsChanged.Broadcast(ToKey);
    return true;
}

int32 UPoolSubsystem::HandOverSkills(FName FromKey, FName ToKey)
{
    FPoolSkills Leaving;
    if (FromKey == ToKey || !CharacterSkills.Contains(ToKey) || !CharacterSkills.RemoveAndCopyValue(FromKey, Leaving))
    {
        return 0;
    }
    FPoolSkills &To = CharacterSkills[ToKey];
    int32 Moved = 0;
    int32 Dropped = 0;
    for (const FSpellInstance &Spell : Leaving.Spells)
    {
        const bool bFits = Spell.Spell && To.Spells.Num() < InventoryConstants::MAX_LEARNED_SPELLS &&
                           !To.Spells.ContainsByPredicate([&Spell](const FSpellInstance &I) { return I.Spell == Spell.Spell; });
        if (bFits)
        {
            To.Spells.Add(Spell);
            ++Moved;
        }
        else
        {
            ++Dropped;
        }
    }
    for (const FAbilityInstance &Ability : Leaving.Abilities)
    {
        const bool bFits = Ability.Ability && To.Abilities.Num() < InventoryConstants::MAX_LEARNED_ABILITIES &&
                           !To.Abilities.ContainsByPredicate([&Ability](const FAbilityInstance &I) { return I.Ability == Ability.Ability; });
        if (bFits)
        {
            To.Abilities.Add(Ability);
            ++Moved;
        }
        else
        {
            ++Dropped;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[Pool] %s's skills go to %s: %d moved, %d already known or no room (one copy kept, Q-361c)"),
           *FromKey.ToString(), *ToKey.ToString(), Moved, Dropped);
    OnPoolSkillsChanged.Broadcast(FromKey);
    OnPoolSkillsChanged.Broadcast(ToKey);
    return Moved;
}

FString UPoolSubsystem::GetCharacterSkillsString() const
{
    FString Out = FString::Printf(TEXT("=== Skills per character (%d) ==="), CharacterSkills.Num());
    for (const TPair<FName, FPoolSkills> &Pair : CharacterSkills)
    {
        TArray<FString> Names;
        for (const FSpellInstance &Spell : Pair.Value.Spells)
        {
            Names.Add(GetNameSafe(Spell.Spell));
        }
        for (const FAbilityInstance &Ability : Pair.Value.Abilities)
        {
            Names.Add(GetNameSafe(Ability.Ability));
        }
        Out += FString::Printf(TEXT("\n  %s: %d spell(s), %d ability(ies): %s"), *Pair.Key.ToString(), Pair.Value.Spells.Num(),
                               Pair.Value.Abilities.Num(), *FString::Join(Names, TEXT(", ")));
    }
    return Out;
}
