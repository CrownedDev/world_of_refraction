// EMovementCategory.h
UENUM(BlueprintType)
enum class EMovementCategory : uint8
{
    Approach UMETA(DisplayName = "Approach"),    // Moving to attack target
    Return UMETA(DisplayName = "Return"),        // Returning to grid position
    Dodge UMETA(DisplayName = "Dodge"),          // Evasion movement
    Parry UMETA(DisplayName = "Parry"),          // Parry stance/movement
    Block UMETA(DisplayName = "Block"),          // Block stance
    Reposition UMETA(DisplayName = "Reposition") // Grid position change
};