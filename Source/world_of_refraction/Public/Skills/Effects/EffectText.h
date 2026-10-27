// EffectText.h
// Item and skill descriptions generated from the effect data (D-031, B-045): the deckbuilder's
// card-text tree (wor-deckbuilder docs/reference/07-text-grammar.md, "The card-text tree").
// Level 0 is the trigger heading, each deeper level a condition that must hold, and the leaves
// are the effects. Pure: no game state is read, so the same data always gives the same text.
// Live lighting (which conditions hold now) is a separate pass over the built lines.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "Skills/Effects/FSkillEffect.h"
#include "EffectText.generated.h"

/** Where the effects come from; it names the heading of effects that have no trigger. */
UENUM(BlueprintType)
enum class EEffectTextSource : uint8
{
    /** Gear (weapon, ring, crystal, stone): untriggered effects are always on, "Passive". */
    Gear UMETA(DisplayName = "Gear"),
    /** A skill: untriggered effects happen when it is used, "Activate". */
    Skill UMETA(DisplayName = "Skill")
};

UENUM(BlueprintType)
enum class EEffectTextLineKind : uint8
{
    Heading UMETA(DisplayName = "Heading"),
    Condition UMETA(DisplayName = "Condition"),
    Effect UMETA(DisplayName = "Effect")
};

/** Live lighting of a condition line (D-031): the base item's pop-up shows none. */
UENUM(BlueprintType)
enum class EEffectTextLight : uint8
{
    Unlit UMETA(DisplayName = "Unlit"),
    Held UMETA(DisplayName = "Held"),
    NotHeld UMETA(DisplayName = "Not Held")
};

/** One printed line of the tree. */
USTRUCT(BlueprintType)
struct WORLD_OF_REFRACTION_API FEffectTextLine
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    EEffectTextLineKind Kind = EEffectTextLineKind::Effect;

    /** 0 for a heading; one more per condition level. */
    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    int32 Depth = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    FText Text;

    /** The effect (index into the list given to BuildTree) this line came from first. */
    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    int32 EffectIndex = INDEX_NONE;

    /** A condition line: the conditions of that effect it prints (one AND entry, or every OR entry). */
    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    TArray<int32> ConditionIndices;

    /** A condition line printing the OR entries: it holds when any of them does. */
    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    bool bAnyOf = false;

    /** A branch that lights up lights green (a buff) or red (a debuff) (D-031). */
    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    bool bDebuff = false;

    UPROPERTY(BlueprintReadOnly, Category = "Effect Text")
    EEffectTextLight Light = EEffectTextLight::Unlit;
};

namespace EffectText
{
    /** The tree, headings in the order first met; rows with the same words share a node. */
    WORLD_OF_REFRACTION_API TArray<FEffectTextLine> BuildTree(const TArray<FSkillEffect> &Effects, EEffectTextSource Source);

    /** Short form: one line per heading, "Trigger: effect, effect", with "*" after the trigger
     *  when anything under it has a condition ("On Hit*: Heal 3, gain 1 Energy"). */
    WORLD_OF_REFRACTION_API TArray<FText> BuildShortForm(const TArray<FSkillEffect> &Effects, EEffectTextSource Source);

    /** The tree as one text block: two spaces per level, effects as "• " bullets. */
    WORLD_OF_REFRACTION_API FText ToMultilineText(const TArray<FEffectTextLine> &Lines);

    /** One payload as its imperative body, lowercase-initial ("heal an Ally for 5"). */
    WORLD_OF_REFRACTION_API FText DescribePayload(const FSkillEffectPayload &Payload);

    /** The trigger heading of one effect ("On Hit", "When a Team Member takes damage"), or the
     *  source's untriggered heading ("Passive" / "Activate"). */
    WORLD_OF_REFRACTION_API FText DescribeHeading(const FSkillEffect &Effect, EEffectTextSource Source);

    /** Sets Light on every condition line: Held when its condition holds now (IsHeld), else NotHeld. */
    WORLD_OF_REFRACTION_API void LightTree(TArray<FEffectTextLine> &Lines, const TArray<FSkillEffect> &Effects,
                                           TFunctionRef<bool(const FSkillCondition &)> IsHeld);

    /** True for the triggers that name a moment (On Hit, At the start of your turn) rather
     *  than a state (HP below a threshold) or nothing (Always, None). */
    WORLD_OF_REFRACTION_API bool IsEventTrigger(ESkillTrigger Trigger);
}
