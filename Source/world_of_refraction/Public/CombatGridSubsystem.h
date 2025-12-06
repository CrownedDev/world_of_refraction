// CombatGridSubsystem.h
// Manages combat grid positions and row modifiers

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FCombatGridPosition.h"
#include "CombatGridSubsystem.generated.h"

/**
 * Manages combat grid positions for all combatants
 * Provides damage/defense modifiers based on row positioning
 * 
 * Grid Layout (per team):
 *   [0,0] [0,1] [0,2]  ← Back Row   (-5% dmg, +5% def)
 *   [1,0] [1,1] [1,2]  ← Middle Row (no modifiers)
 *   [2,0] [2,1] [2,2]  ← Front Row  (+5% dmg, -5% def)
 */
UCLASS()
class WORLD_OF_REFRACTION_API UCombatGridSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
    
public:
    // ==================== LIFECYCLE ====================
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    // ==================== POSITION MANAGEMENT ====================
    
    /**
     * Assign an actor to a grid position
     * @param Actor The combatant to position
     * @param Position The grid position to assign
     * @return True if assignment successful
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid")
    bool AssignPosition(AActor* Actor, const FCombatGridPosition& Position);
    
    /**
     * Remove an actor from the grid
     * @param Actor The combatant to remove
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid")
    void RemoveFromGrid(AActor* Actor);
    
    /**
     * Get an actor's current grid position
     * @param Actor The combatant to query
     * @param OutPosition The position if found
     * @return True if actor has a position
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid")
    bool GetActorPosition(AActor* Actor, FCombatGridPosition& OutPosition) const;
    
    /**
     * Check if a grid position is occupied
     * @param Position The position to check
     * @return True if position has an actor
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid")
    bool IsPositionOccupied(const FCombatGridPosition& Position) const;
    
    /**
     * Get the actor at a specific position
     * @param Position The position to query
     * @return The actor at that position, or nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid")
    AActor* GetActorAtPosition(const FCombatGridPosition& Position) const;
    
    /**
     * Clear all positions (call at combat end)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid")
    void ClearAllPositions();
    
    // ==================== MODIFIER GETTERS ====================
    
    /**
     * Get damage modifier for an actor based on their row position
     * @param Actor The attacking actor
     * @return Damage multiplier (0.95, 1.0, or 1.05)
     */
    UFUNCTION(BlueprintPure, Category = "Combat Grid|Modifiers")
    float GetDamageModifier(AActor* Actor) const;
    
    /**
     * Get defense modifier for an actor based on their row position
     * @param Actor The defending actor
     * @return Defense multiplier (0.95, 1.0, or 1.05)
     */
    UFUNCTION(BlueprintPure, Category = "Combat Grid|Modifiers")
    float GetDefenseModifier(AActor* Actor) const;
    
    /**
     * Get row for an actor
     * @param Actor The actor to query
     * @return The row (defaults to Middle if not found)
     */
    UFUNCTION(BlueprintPure, Category = "Combat Grid")
    ECombatRow GetActorRow(AActor* Actor) const;
    
    // ==================== TEAM QUERIES ====================
    
    /**
     * Get all actors on a team
     * @param TeamIndex The team to query (0 or 1)
     * @return Array of actors on that team
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid|Teams")
    TArray<AActor*> GetTeamActors(int32 TeamIndex) const;
    
    /**
     * Get all actors in a specific row
     * @param TeamIndex The team to query
     * @param Row The row to query
     * @return Array of actors in that row
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid|Teams")
    TArray<AActor*> GetActorsInRow(int32 TeamIndex, ECombatRow Row) const;
    
    /**
     * Get count of actors on a team
     * @param TeamIndex The team to query
     * @return Number of actors
     */
    UFUNCTION(BlueprintPure, Category = "Combat Grid|Teams")
    int32 GetTeamCount(int32 TeamIndex) const;
    
    // ==================== WORLD POSITIONING ====================
    
    /**
     * Calculate world position for a grid position
     * @param Position The grid position
     * @param ArenaCenter The center of the arena
     * @return World location for the position
     */
    UFUNCTION(BlueprintPure, Category = "Combat Grid|World")
    FVector CalculateWorldPosition(const FCombatGridPosition& Position, const FVector& ArenaCenter) const;
    
    /**
     * Place an actor at their assigned grid position in the world
     * @param Actor The actor to place
     * @param ArenaCenter The center of the arena
     * @return True if placement successful
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid|World")
    bool PlaceActorAtGridPosition(AActor* Actor, const FVector& ArenaCenter);
    
    /**
     * Place all assigned actors at their grid positions
     * @param ArenaCenter The center of the arena
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid|World")
    void PlaceAllActors(const FVector& ArenaCenter);
    
    // ==================== AUTO-ASSIGNMENT ====================
    
    /**
     * Automatically assign positions for a team
     * Places actors in default formation (one per column, starting from middle row)
     * @param TeamActors The actors to assign
     * @param TeamIndex Which team (0 or 1)
     * @param PreferredRow Default row for all actors
     */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid|Auto")
    void AutoAssignTeam(const TArray<AActor*>& TeamActors, int32 TeamIndex, ECombatRow PreferredRow = ECombatRow::Middle);
    
    // ==================== DEBUG ====================
    
    /** Log all current positions */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid|Debug", CallInEditor)
    void DebugLogAllPositions() const;
    
    /** Log modifiers for all actors */
    UFUNCTION(BlueprintCallable, Category = "Combat Grid|Debug", CallInEditor)
    void DebugLogModifiers() const;
    
private:
    /** Map of actors to their grid positions */
    UPROPERTY()
    TMap<AActor*, FCombatGridPosition> ActorPositions;
    
    /** Get unique key for position lookup */
    int32 GetPositionKey(const FCombatGridPosition& Position) const;
};
