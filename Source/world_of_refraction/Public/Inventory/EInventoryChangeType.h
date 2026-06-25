// EInventoryChangeType.h
// Hint carried by UInventoryComponent::OnInventoryChanged — what KIND of
// mutation fired. v1 consumers (UI) re-read the whole inventory on any change,
// so the enum is an optimisation hint, not a payload. Lives in its own header
// so sibling components (Crystal/Evolution) can include it without pulling in
// the full InventoryComponent.h.

#pragma once

#include "CoreMinimal.h"
#include "EInventoryChangeType.generated.h"

UENUM(BlueprintType)
enum class EInventoryChangeType : uint8
{
    /** Something was granted into the inventory (learn / add / draw-in). */
    Added,
    /** Something was consumed or removed from the inventory. */
    Removed,
    /** A crystal / evolution was socketed onto an owned weapon or ring. */
    Equipped,
    /** A bulk (re)load replaced the whole inventory (seed / pool draw). */
    Loaded
};
