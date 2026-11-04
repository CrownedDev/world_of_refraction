// LevelDropWidget.h
// PLACEHOLDER UI (B-223, D-102): place the pillar levels a won fight dropped. Pick a party member, then Mind,
// Body or Spirit; each press places one waiting level. Plain buttons built in C++, opened with the console
// command wor.UI.LevelDrop (Backlog rule 4). Replaced by the CommonUI screen (D-024). Rules: WoRLevelDrops
// (Trial/LevelDropPlacement.h).
// B-360c (D-150 point 2): the rows are party MEMBERS, so a later copy of a character places its own levels.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/EWorldPillar.h"
#include "Party/Party.h"
#include "LevelDropWidget.generated.h"

class UTextBlock;

UCLASS()
class WORLD_OF_REFRACTION_API ULevelDropWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Add a fresh one to PC's viewport (UI-only input, cursor on). Null when PC is null. */
	static ULevelDropWidget *Open(APlayerController *PC);

	/** Re-read the member's pillars and the waiting count, and rewrite every row. */
	void Refresh();

	/** Debug: the rows as shown. */
	FString GetLevelDropWidgetString() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	TArray<FPartyMember> GatherMembers() const;
	/** The member the screen is on; false when the party is empty. */
	bool GetSelectedMember(FPartyMember &OutMember) const;

	void Place(EWorldPillar Pillar);

	UFUNCTION()
	void HandleNextMemberClicked();

	UFUNCTION()
	void HandleMindClicked();

	UFUNCTION()
	void HandleBodyClicked();

	UFUNCTION()
	void HandleSpiritClicked();

	UFUNCTION()
	void HandleCloseClicked();

	int32 MemberIndex = 0;

	FText StatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> WaitingText;

	UPROPERTY()
	TObjectPtr<UTextBlock> MemberText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLine;
};
