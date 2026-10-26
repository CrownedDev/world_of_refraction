// WoRAuthority.h
// The one "may this machine change this actor's game state?" check (B-027). Every mutating
// component or service asks this instead of its own copy.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

namespace WoRAuthority
{
    /** True in standalone play (PIE with no networking) or on the server's copy of Actor; false
     *  for a null actor and on a client. */
    inline bool HasAuthority(const AActor *Actor)
    {
        return Actor && (Actor->GetNetMode() == NM_Standalone || Actor->HasAuthority());
    }
}
