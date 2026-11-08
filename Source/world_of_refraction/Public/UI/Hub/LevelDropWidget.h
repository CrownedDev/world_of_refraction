// LevelDropWidget.h
// PLACEHOLDER UI (B-223, D-102): place the pillar levels a won fight dropped. B-363 (D-156): at a rest spot, with a
// Purchase point button (run Prisms, Q-156's ladder); the rest menu opens it. Pick a party member, then Mind,
// Body or Spirit; each press places one waiting level. Plain buttons built in C++, opened with the console
// command wor.UI.LevelDrop (Backlog rule 4). Replaced by the CommonUI screen (D-024). Rules: WoRLevelDrops
// (Trial/LevelDropPlacement.h).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Character/EWorldPillar.h"
#include "LevelDropWidget.generated.h"

class UCharacterData;
struct FPartyMember;
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
	/** B-360c: the local party's members (each copy of a character is its own); pointers into the live party. */
	TArray<const FPartyMember *> GatherMembers() const;
	const FPartyMember *SelectedMember() const;

	void Place(EWorldPillar Pillar);

	UFUNCTION()
	void HandleNextMemberClicked();

	UFUNCTION()
	void HandleMindClicked();

	UFUNCTION()
	void HandleBodyClicked();

	UFUNCTION()
	void HandleSpiritClicked();

	/** B-363 (D-156): buy the member one point for run Prisms (at a rest spot). */
	UFUNCTION()
	void HandleBuyClicked();

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

	/** B-363: the Purchase point button's label (its price follows the member). */
	UPROPERTY()
	TObjectPtr<UTextBlock> BuyLabel;
};
