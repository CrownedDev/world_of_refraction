// ECondSubject.h
// Which actor(s) a skill condition evaluates against. Self/SelfTeam are the OWNER's side;
// Target/TargetTeam are the owner's TARGET's side. Team scopes resolve to all members of
// that side and the condition fires if ANY member meets it.

#pragma once

#include "CoreMinimal.h"
#include "ECondSubject.generated.h"

UENUM(BlueprintType)
enum class ECondSubject : uint8
{
    Self UMETA(DisplayName = "Self"),
    SelfTeam UMETA(DisplayName = "Self Team (any ally)"),
    Target UMETA(DisplayName = "Target"),
    TargetTeam UMETA(DisplayName = "Target Team (any enemy)")
};
