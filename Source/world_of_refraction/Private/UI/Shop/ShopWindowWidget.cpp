// ShopWindowWidget.cpp

#include "UI/Shop/ShopWindowWidget.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyWallet.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h" // scaling-grade letters for the detail panel
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/CrystalDescription.h" // GetItemEffectText for the crystal Effect line
#include "Equipment/Crystals/CrystalTypeHelpers.h" // GetElement for the gem and ring attached-crystal element lines
#include "Equipment/Crystals/EvolutionItemData.h"  // single-owned cart-cap casts
#include "Equipment/Crystals/ItemIdentity.h"
#include "Equipment/Rings/RingData.h"
#include "Equipment/Weapons/WeaponData.h"
#include "GameFramework/Pawn.h"
#include "Inventory/ItemTier.h" // TierHelpers
#include "Shop/MerchantShopSubsystem.h"
#include "Skills/Definitions/AbilityData.h"
#include "Skills/Definitions/SpellData.h"
#include "Skills/Effects/EffectDefinition.h"
#include "UI/Shop/ShopRowWidget.h" // shared per-entry display ladder (NameFor/TierFor/DescriptionFor)
#include "Skills/Definitions/SupportSkillRules.h"
#include "Trial/TrialRunSubsystem.h"
#include "Character/CharacterData.h"
#include "Character/CharacterDataComponent.h"
#include "Party/Party.h"
#include "Party/PartySessionSubsystem.h"
#include "Pool/PoolSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"
#include "WoRGameNames.h"

namespace ShopWindowConstants
{
    constexpr float TOAST_DURATION_SECONDS = 1.5f;
    const FLinearColor INSUFFICIENT_RED(1.0f, 0.25f, 0.25f);
}

namespace
{
    /** B-333: same rule as Purchase — Prisms always, each essence only when the cost needs it. */
    bool CanAffordCost(const UCurrencyComponent &Currency, const FPurchaseCost &Cost)
    {
        for (const ECurrencyType Balance : PurchaseCostBalances::CHARGED)
        {
            const int32 Amount = Cost.Get(Balance);
            if ((Balance == ECurrencyType::Prisms || Amount > 0) && !Currency.CanAfford(Balance, Amount))
            {
                return false;
            }
        }
        return true;
    }

    /** Cart-line identity for RemoveFromCart: same Asset, or same CrystalId. */
    bool SameShopLine(const FMerchantStockEntry &A, const FMerchantStockEntry &B)
    {
        if (A.IsAssetEntry() != B.IsAssetEntry())
        {
            return false;
        }
        return A.IsAssetEntry() ? A.Asset == B.Asset : A.Crystal == B.Crystal;
    }

    /** Per-type stat lines for the detail panel (3e). Effects show NAMES only —
     *  never numbers or durations. Evolutions deliberately contribute nothing
     *  (Name/Tier/Description only — no reveal). */
    FString BuildDetailStats(const FMerchantStockEntry &E)
    {
        TArray<FString> Lines;

        if (E.IsCrystalEntry())
        {
            // Element line for elemental gems only (stones and Quartz have none, B-207). Effect is
            // the full mechanical sentence (3j — numbers on stone effect text accepted per Crown).
            const ESpellElement CrystalElement = CrystalTypeHelpers::GetElement(E.Crystal.Type);
            if (CrystalElement != ESpellElement::None)
            {
                Lines.Add(FString::Printf(TEXT("Element: %s"), *WoRGameNames::GetElementName(CrystalElement).ToString()));
            }
            Lines.Add(FString::Printf(TEXT("Effect: %s"), *CrystalDescription::GetItemEffectText(E.Crystal)));
        }
        else if (const UWeaponData *W = Cast<UWeaponData>(E.Asset))
        {
            if (W->WeaponAttack)
            {
                Lines.Add(FString::Printf(TEXT("Attack: %s"), *W->WeaponAttack->Name));
            }
            TArray<FString> AbilityNames;
            for (const UAbilityData *A : W->PresetAbilities)
            {
                if (A)
                {
                    AbilityNames.Add(A->Name);
                }
            }
            if (AbilityNames.Num() > 0)
            {
                Lines.Add(FString::Printf(TEXT("Abilities: %s"), *FString::Join(AbilityNames, TEXT(", "))));
            }
        }
        else if (const URingData *R = Cast<URingData>(E.Asset))
        {
            TArray<FString> SpellNames;
            for (const USpellData *S : R->DefaultSpells)
            {
                if (S)
                {
                    SpellNames.Add(S->Name);
                }
            }
            if (SpellNames.Num() > 0)
            {
                Lines.Add(FString::Printf(TEXT("Spells: %s"), *FString::Join(SpellNames, TEXT(", "))));
            }
            if (R->HasCrystal())
            {
                Lines.Add(FString::Printf(TEXT("Attached Crystal: %s (%s)"),
                                          *ItemIdentity::GetTypeName(R->AttachedItem.CrystalType),
                                          *TierHelpers::GetTierName(R->AttachedItem.CrystalTier)));
                Lines.Add(FString::Printf(TEXT("Attached Crystal Element: %s"),
                                          *WoRGameNames::GetElementName(CrystalTypeHelpers::GetElement(R->AttachedItem.CrystalType)).ToString()));
            }
        }
        else if (const UEvolutionItemData *Ev = Cast<UEvolutionItemData>(E.Asset))
        {
            // 3f #7: element IS shown for evolutions — Crown reversed the 3e
            // deliberately-bare call. Still no effect reveal.
            Lines.Add(FString::Printf(TEXT("Element: %s"),
                                      *WoRGameNames::GetElementName(Ev->GetAssociatedElement()).ToString()));
        }
        else if (const USkillDataBase *Skill = Cast<USkillDataBase>(E.Asset)) // spell + ability
        {
            if (const USpellData *S = Cast<USpellData>(Skill))
            {
                Lines.Add(FString::Printf(TEXT("School: %s"), *WoRGameNames::GetSpellSchoolName(S->GetSchool()).ToString()));
                Lines.Add(FString::Printf(TEXT("Element: %s"), *WoRGameNames::GetElementName(S->Element).ToString()));
            }
            Lines.Add(FString::Printf(TEXT("Damage: %d"), Skill->BaseDamage));
            Lines.Add(FString::Printf(TEXT("Hits: %d"), Skill->HitCount));
            Lines.Add(FString::Printf(TEXT("Energy: %d"), Skill->BaseEnergyCost));
            if (const int32 Buildup = SupportSkillRules::GetAuthoredStatusBuildup(Skill); Buildup > 0)
            {
                Lines.Add(FString::Printf(TEXT("Status buildup: %d"), Buildup));
            }
            TArray<FString> Grades;
            for (const FStatScaling &Sc : Skill->StatScaling)
            {
                if (Sc.Stat == ESubStat::None)
                {
                    continue;
                }
                Grades.Add(FString::Printf(TEXT("%s %s"),
                                           *StaticEnum<ESubStat>()->GetAuthoredNameStringByValue(static_cast<int64>(Sc.Stat)),
                                           *TierHelpers::GetTierName(EconomyYield::ScalingGradeToItemTier(Sc.Tier))));
            }
            if (Grades.Num() > 0)
            {
                Lines.Add(FString::Printf(TEXT("Scaling: %s"), *FString::Join(Grades, TEXT(", "))));
            }
            TArray<FString> EffectNames;
            for (const UEffectDefinition *Def : Skill->ReferencedEffects)
            {
                if (Def)
                {
                    EffectNames.Add(Def->DisplayName.IsEmpty() ? Def->GetName() : Def->DisplayName.ToString());
                }
            }
            if (EffectNames.Num() > 0)
            {
                Lines.Add(FString::Printf(TEXT("Effects: %s"), *FString::Join(EffectNames, TEXT(", "))));
            }
        }

        return FString::Join(Lines, TEXT("\n"));
    }
}

FText UShopWindowWidget::GetCostLineText(const FPurchaseCost &Cost)
{
    TArray<FText> Parts;
    for (const ECurrencyType Balance : PurchaseCostBalances::CHARGED)
    {
        const int32 Amount = Cost.Get(Balance);
        if (Balance == ECurrencyType::Prisms || Amount > 0)
        {
            Parts.Add(FText::Format(NSLOCTEXT("Shop", "CostPart", "{0} {1}"), WoRGameNames::GetCurrencyName(Balance),
                                    FText::AsNumber(Amount)));
        }
    }
    return FText::Join(NSLOCTEXT("Shop", "CostSeparator", " | "), Parts);
}

void UShopWindowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true); // FInputModeUIOnly focuses this widget

    if (ConfirmButton)
    {
        ConfirmButton->OnClicked.AddUniqueDynamic(this, &UShopWindowWidget::HandleConfirmClicked);
    }
    if (CloseButton)
    {
        CloseButton->OnClicked.AddUniqueDynamic(this, &UShopWindowWidget::HandleCloseClicked);
    }
    if (DetailAddButton)
    {
        DetailAddButton->OnClicked.AddUniqueDynamic(this, &UShopWindowWidget::HandleDetailAddClicked);
    }
    if (BuyForButton)
    {
        BuyForButton->OnClicked.AddUniqueDynamic(this, &UShopWindowWidget::HandleBuyForClicked);
    }
    if (ToastText)
    {
        ToastText->SetVisibility(ESlateVisibility::Collapsed);
    }

    // Self-resolve the data source: the subsystem stamps ActiveMerchant/ActivePawn BEFORE
    // AddToViewport, so Construct reads them without the subsystem knowing this type.
    if (UMerchantShopSubsystem *Shop = GetShopSubsystem())
    {
        if (!ActiveMerchant && Shop->GetActiveMerchant())
        {
            SetMerchant(Shop->GetActiveMerchant(), Shop->GetActivePawn());
        }
    }
}

void UShopWindowWidget::NativeDestruct()
{
    if (UCurrencyComponent *Currency = PurchaserCurrency.Get())
    {
        Currency->OnCurrencyChanged.RemoveDynamic(this, &UShopWindowWidget::HandleCurrencyChanged);
    }
    FTSTicker::GetCoreTicker().RemoveTicker(ToastTickerHandle);
    ToastTickerHandle.Reset();

    // B-075: the window removed some other way (not the Close button) still unpauses the game.
    if (UMerchantShopSubsystem *Shop = GetShopSubsystem(); Shop && Shop->IsShowing(this))
    {
        Shop->HandleWindowDestroyed();
    }

    Super::NativeDestruct();
}

void UShopWindowWidget::SetMerchant(UMerchantData *Merchant, APawn *PurchaserPawn)
{
    if (UCurrencyComponent *Old = PurchaserCurrency.Get())
    {
        Old->OnCurrencyChanged.RemoveDynamic(this, &UShopWindowWidget::HandleCurrencyChanged);
    }

    ActiveMerchant = Merchant;
    Purchaser = PurchaserPawn;
    BuyForKey = NAME_None;
    RefreshBuyFor();
    PurchaserCurrency = CurrencyWallet::FindFor(PurchaserPawn); // D-046: the player's one wallet

    if (UCurrencyComponent *Currency = PurchaserCurrency.Get())
    {
        Currency->OnCurrencyChanged.AddUniqueDynamic(this, &UShopWindowWidget::HandleCurrencyChanged);
    }
    else
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[ShopWindow] SetMerchant: %s has no CurrencyComponent — wallet blank, purchases will fail."),
               PurchaserPawn ? *PurchaserPawn->GetName() : TEXT("<null pawn>"));
    }

    Cart.Reset();
    RefreshHeader();
    RefreshStockList();
    RefreshCartList();
    RefreshTotals();
}

void UShopWindowWidget::AddToCart(const FMerchantStockEntry &Entry)
{
    if (!Entry.IsAssetEntry() && !Entry.IsCrystalEntry())
    {
        UE_LOG(LogTemp, Warning, TEXT("[ShopWindow] AddToCart: empty entry ignored."));
        return;
    }

    // Coalesce: ONE cart line per distinct item, Count accumulates per click.
    // Spells/abilities/evolutions cap at 1 — Purchase clamps their grant to a single
    // unit per line, so a higher displayed count would be a lie at the till.
    const bool bSingleOwned = Cast<USpellData>(Entry.Asset) != nullptr ||
                              Cast<UAbilityData>(Entry.Asset) != nullptr ||
                              Cast<UEvolutionItemData>(Entry.Asset) != nullptr;
    FMerchantStockEntry *Line = Cart.FindByPredicate([&Entry](const FMerchantStockEntry &L)
                                                     { return SameShopLine(L, Entry); });
    if (Line)
    {
        if (!bSingleOwned)
        {
            Line->Count += FMath::Max(1, Entry.Count);
        }
    }
    else
    {
        FMerchantStockEntry &NewLine = Cart.Add_GetRef(Entry);
        NewLine.Count = bSingleOwned ? 1 : FMath::Max(1, Entry.Count);
    }
    RefreshCartList();
    RefreshTotals();
}

void UShopWindowWidget::RemoveFromCart(const FMerchantStockEntry &Entry)
{
    const int32 Index = Cart.IndexOfByPredicate([&Entry](const FMerchantStockEntry &Line)
                                                { return SameShopLine(Line, Entry); });
    if (Index == INDEX_NONE)
    {
        return;
    }
    // Decrement one unit per click; the line leaves the cart at zero.
    if (--Cart[Index].Count <= 0)
    {
        Cart.RemoveAt(Index);
    }
    RefreshCartList();
    RefreshTotals();
}

void UShopWindowWidget::HandleConfirmClicked()
{
    if (Cart.Num() == 0)
    {
        return;
    }

    APawn *Pawn = Purchaser.Get();
    UEconomyService *Economy = GetEconomyService();
    if (!Pawn || !Economy)
    {
        ShowToast(NSLOCTEXT("Shop", "ShopUnavailable", "Shop unavailable"));
        return;
    }

    if (Economy->PurchaseFor(Pawn, BuyForKey, Cart))
    {
        Cart.Reset();
        RefreshCartList();
        RefreshTotals();
        ShowToast(NSLOCTEXT("Shop", "Purchased", "Purchased!"));
    }
    else
    {
        // Shouldn't fire — Confirm disables while unaffordable; capacity/duplicate edge
        // cases (e.g. inventory filled since the last refresh) land here.
        ShowToast(NSLOCTEXT("Shop", "PurchaseFailed", "Purchase failed"));
        RefreshTotals();
    }
}

void UShopWindowWidget::HandleCloseClicked()
{
    if (UMerchantShopSubsystem *Shop = GetShopSubsystem())
    {
        Shop->Close();
    }
}

void UShopWindowWidget::RefreshHeader()
{
    // 3e: no asset-name line. The header splits the merchant-type display name —
    // "Spell Shop (spells)" → name "Spell Shop", flavour "(spells)".
    FString HeaderName;
    FString TypeFlavour;
    if (ActiveMerchant)
    {
        const FString TypeDisplay = WoRGameNames::GetMerchantTypeName(ActiveMerchant->MerchantType).ToString();
        int32 ParenIdx = INDEX_NONE;
        if (TypeDisplay.FindChar(TEXT('('), ParenIdx))
        {
            HeaderName = TypeDisplay.Left(ParenIdx).TrimEnd();
            TypeFlavour = TypeDisplay.Mid(ParenIdx);
        }
        else
        {
            HeaderName = TypeDisplay;
        }
    }

    if (MerchantName)
    {
        MerchantName->SetText(ActiveMerchant ? FText::FromString(HeaderName)
                                             : NSLOCTEXT("Shop", "NoMerchant", "<no merchant>"));
    }
    if (MerchantType)
    {
        // 3g: flavour line dropped — the header is just the merchant name.
        MerchantType->SetText(FText::GetEmpty());
    }
}

void UShopWindowWidget::RefreshStockList()
{
    StockObjects.Reset();
    if (!StockList)
    {
        return;
    }

    TArray<UObject *> Items;
    if (ActiveMerchant)
    {
        // B-179: a trial vendor shows this run's rolled stock; at home the full list.
        const UTrialRunSubsystem *TrialRun = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTrialRunSubsystem>() : nullptr;
        const TArray<FMerchantStockEntry> Stock = TrialRun ? TrialRun->GetMerchantStock(ActiveMerchant) : ActiveMerchant->Stock;
        for (const FMerchantStockEntry &E : Stock)
        {
            UShopEntryObject *Obj = NewObject<UShopEntryObject>(this);
            Obj->Entry = E;
            Obj->ParentWindow = this; // rows route AddToCart through this back-ref
            StockObjects.Add(Obj);
            Items.Add(Obj);
        }
    }
    StockList->SetListItems(Items);

    // Default detail target on (re)population: the first stock entry, or clear.
    SetHoveredEntry(StockObjects.Num() > 0 ? StockObjects[0].Get() : nullptr);
}

void UShopWindowWidget::SetHoveredEntry(UShopEntryObject *Entry)
{
    HoveredEntry = Entry;
    RefreshDetail();
}

void UShopWindowWidget::RefreshDetail()
{
    const UShopEntryObject *Obj = HoveredEntry.Get();

    if (DetailAddButton)
    {
        DetailAddButton->SetIsEnabled(Obj != nullptr);
    }
    if (DetailName)
    {
        DetailName->SetText(Obj ? UShopRowWidget::NameFor(Obj->Entry) : FText::GetEmpty());
    }
    if (DetailTier)
    {
        DetailTier->SetText(Obj ? FText::FromString(FString::Printf(TEXT("Tier: %s"),
                                                                    *UShopRowWidget::TierFor(Obj->Entry).ToString()))
                                : FText::GetEmpty());
    }
    if (DetailDescription)
    {
        DetailDescription->SetText(Obj ? FText::FromString(FString::Printf(TEXT("Description: %s"),
                                                                           *UShopRowWidget::DescriptionFor(Obj->Entry).ToString()))
                                       : FText::GetEmpty());
    }
    if (DetailStats)
    {
        DetailStats->SetText(Obj ? FText::FromString(BuildDetailStats(Obj->Entry)) : FText::GetEmpty());
    }
    if (DetailCost)
    {
        FText CostLine;
        UEconomyService *Economy = GetEconomyService();
        if (Obj && Economy)
        {
            // Same builder Purchase charges with — one-entry cart, Count included.
            const FPurchaseCost Cost = Economy->PreviewCartCost({Obj->Entry});
            CostLine = FText::Format(NSLOCTEXT("Shop", "DetailCost", "Cost: {0}"), GetCostLineText(Cost));
        }
        DetailCost->SetText(CostLine);
    }
}

void UShopWindowWidget::HandleDetailAddClicked()
{
    if (const UShopEntryObject *Obj = HoveredEntry.Get())
    {
        AddToCart(Obj->Entry);
    }
}

void UShopWindowWidget::RefreshCartList()
{
    CartObjects.Reset();
    if (!CartList)
    {
        return;
    }

    TArray<UObject *> Items;
    for (const FMerchantStockEntry &E : Cart)
    {
        UShopEntryObject *Obj = NewObject<UShopEntryObject>(this);
        Obj->Entry = E;
        Obj->ParentWindow = this; // rows route RemoveFromCart through this back-ref
        CartObjects.Add(Obj);
        Items.Add(Obj);
    }
    CartList->SetListItems(Items);
}

void UShopWindowWidget::EnsureTotalsPool()
{
    if (!CartTotalsPanel || TotalsRows.Num() > 0)
    {
        return;
    }

    // B-333: a header + one row per balance a purchase can charge (Prisms, Skill, Elemental, Pillar).
    constexpr int32 ChargedCount = static_cast<int32>(UE_ARRAY_COUNT(PurchaseCostBalances::CHARGED));
    const int32 RowCount = 1 /*header*/ + ChargedCount;

    for (int32 i = 0; i < RowCount; ++i)
    {
        FTotalsRow Row;
        Row.Box = NewObject<UHorizontalBox>(this);
        Row.Label = MakeTotalsCell(Row.Box, 1.0f);
        Row.CostCell = MakeTotalsCell(Row.Box, 0.35f);
        Row.WalletCell = MakeTotalsCell(Row.Box, 0.35f);
        Row.Box->SetVisibility(ESlateVisibility::Collapsed);
        CartTotalsPanel->AddChildToVerticalBox(Row.Box);
        TotalsRows.Add(Row);
    }

    // Static texts — header + currency labels never change after the build.
    TotalsRows[0].Label->SetText(NSLOCTEXT("Shop", "TotalsCurrency", "Currency"));
    TotalsRows[0].CostCell->SetText(NSLOCTEXT("Shop", "TotalsCost", "Cost"));
    TotalsRows[0].WalletCell->SetText(NSLOCTEXT("Shop", "TotalsWallet", "Wallet"));
    for (int32 i = 0; i < ChargedCount; ++i)
    {
        TotalsRows[1 + i].Label->SetText(WoRGameNames::GetCurrencyName(PurchaseCostBalances::CHARGED[i]));
    }
}

UTextBlock *UShopWindowWidget::MakeTotalsCell(UHorizontalBox *Row, float FillFraction)
{
    UTextBlock *Cell = NewObject<UTextBlock>(this);
    UHorizontalBoxSlot *CellSlot = Row->AddChildToHorizontalBox(Cell);
    FSlateChildSize Size(ESlateSizeRule::Fill);
    Size.Value = FillFraction;
    CellSlot->SetSize(Size);
    CellSlot->SetPadding(FMargin(4.0f, 2.0f));
    return Cell;
}

void UShopWindowWidget::RefreshTotals()
{
    UEconomyService *Economy = GetEconomyService();
    const FPurchaseCost Cost = Economy ? Economy->PreviewCartCost(Cart) : FPurchaseCost();
    UCurrencyComponent *Currency = PurchaserCurrency.Get();

    if (CartTotalsPanel)
    {
        EnsureTotalsPool();
        constexpr int32 ChargedCount = static_cast<int32>(UE_ARRAY_COUNT(PurchaseCostBalances::CHARGED));
        bool bAnyVisible = false;

        for (int32 i = 0; i < ChargedCount; ++i)
        {
            const FTotalsRow &Row = TotalsRows[1 + i];
            const ECurrencyType Balance = PurchaseCostBalances::CHARGED[i];
            const int32 CostAmount = Cost.Get(Balance);
            const int32 WalletAmount = Currency ? Currency->GetBalance(Balance) : 0;

            // The Prisms row anchors the table whenever the cart has anything; other
            // currencies appear only when the cart actually needs them.
            const bool bVisible = Cart.Num() > 0 && (CostAmount > 0 || i == 0);
            Row.Box->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (!bVisible)
            {
                continue;
            }

            const bool bShort = WalletAmount < CostAmount;
            const FSlateColor CellColor(bShort ? ShopWindowConstants::INSUFFICIENT_RED : FLinearColor::White);
            Row.CostCell->SetText(FText::AsNumber(CostAmount));
            Row.WalletCell->SetText(FText::AsNumber(WalletAmount));
            Row.CostCell->SetColorAndOpacity(CellColor);
            Row.WalletCell->SetColorAndOpacity(CellColor);
            bAnyVisible = true;
        }
        TotalsRows[0].Box->SetVisibility(bAnyVisible ? ESlateVisibility::HitTestInvisible
                                                     : ESlateVisibility::Collapsed);
    }

    if (ConfirmButton)
    {
        // All-green requirement — identical math to the row coloring above.
        ConfirmButton->SetIsEnabled(Currency && Cart.Num() > 0 && CanAffordCost(*Currency, Cost));
    }
}

void UShopWindowWidget::ShowToast(const FText &Message)
{
    if (!ToastText)
    {
        return;
    }
    ToastText->SetText(Message);
    ToastText->SetVisibility(ESlateVisibility::HitTestInvisible);

    // Core ticker, NOT a world timer: the hub is paused under the shop, so FTimerManager
    // would never fire the auto-hide.
    FTSTicker::GetCoreTicker().RemoveTicker(ToastTickerHandle);
    ToastTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this, [this](float)
                                          { HideToast(); return false; }),
        ShopWindowConstants::TOAST_DURATION_SECONDS);
}

void UShopWindowWidget::HideToast()
{
    if (ToastText)
    {
        ToastText->SetVisibility(ESlateVisibility::Collapsed);
    }
    ToastTickerHandle.Reset();
}

UEconomyService *UShopWindowWidget::GetEconomyService() const
{
    UGameInstance *GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UEconomyService>() : nullptr;
}

UMerchantShopSubsystem *UShopWindowWidget::GetShopSubsystem() const
{
    UGameInstance *GI = GetGameInstance();
    return GI ? GI->GetSubsystem<UMerchantShopSubsystem>() : nullptr;
}

void UShopWindowWidget::HandleCurrencyChanged(ECurrencyType /*Currency*/, uint8 /*SubKey*/, int32 /*NewBalance*/)
{
    RefreshTotals();
}

namespace
{
    /** B-316: buying for someone else needs the purchaser and at least one other member. */
    constexpr int32 BUY_FOR_MIN_PARTY = 2;

    /** B-316: the local party's members, in party order. */
    TArray<const UCharacterData *> ShopPartyMembers(const UUserWidget &Widget)
    {
        TArray<const UCharacterData *> Members;
        const UGameInstance *GI = Widget.GetGameInstance();
        const UPartySessionSubsystem *PartySession = GI ? GI->GetSubsystem<UPartySessionSubsystem>() : nullptr;
        if (const UParty *Party = PartySession ? PartySession->GetLocalParty() : nullptr)
        {
            for (const FPartyMember &Member : Party->Members)
            {
                if (Member.CharacterData)
                {
                    Members.Add(Member.CharacterData);
                }
            }
        }
        return Members;
    }

    FName ShopPurchaserKey(const APawn *Pawn)
    {
        const UCharacterDataComponent *CharComp = Pawn ? Pawn->FindComponentByClass<UCharacterDataComponent>() : nullptr;
        return UPoolSubsystem::GetCharacterKey(CharComp); // B-360b: the pawn's member key
    }
}

void UShopWindowWidget::HandleBuyForClicked()
{
    // The cycle: each party member in order, the purchaser's own key standing for "yourself" (none).
    const TArray<const UCharacterData *> Members = ShopPartyMembers(*this);
    const FName Self = ShopPurchaserKey(Purchaser.Get());
    TArray<FName> Keys;
    for (const UCharacterData *Member : Members)
    {
        Keys.Add(UPoolSubsystem::GetCharacterKey(Member));
    }
    if (Keys.Num() < BUY_FOR_MIN_PARTY)
    {
        BuyForKey = NAME_None;
    }
    else
    {
        const FName Current = BuyForKey.IsNone() ? Self : BuyForKey;
        const int32 Index = Keys.IndexOfByKey(Current);
        const FName Next = Keys[(Index == INDEX_NONE ? 0 : Index + 1) % Keys.Num()];
        BuyForKey = Next == Self ? NAME_None : Next;
    }
    RefreshBuyFor();
    ShowToast(GetBuyForLabel());
    UE_LOG(LogTemp, Log, TEXT("[ShopWindow] %s"), *GetBuyForLabel().ToString());
}

FText UShopWindowWidget::GetBuyForLabel() const
{
    if (!BuyForKey.IsNone())
    {
        for (const UCharacterData *Member : ShopPartyMembers(*this))
        {
            if (UPoolSubsystem::GetCharacterKey(Member) == BuyForKey)
            {
                // TODO: character names are FString assets; FText with B-127 (D-061).
                return FText::Format(NSLOCTEXT("Shop", "BuyingFor", "Buying for: {0}"), FText::FromString(Member->Name));
            }
        }
    }
    return FText::Format(NSLOCTEXT("Shop", "BuyingFor", "Buying for: {0}"), WoRGameNames::GetYourselfWord());
}

void UShopWindowWidget::RefreshBuyFor()
{
    if (BuyForText)
    {
        BuyForText->SetText(GetBuyForLabel());
    }
}

FString UShopWindowWidget::GetShopString() const
{
    FString Result = FString::Printf(TEXT("ShopWindow — merchant: %s, purchaser: %s, %s"),
                                     ActiveMerchant ? *ActiveMerchant->GetName() : TEXT("<none>"),
                                     Purchaser.IsValid() ? *Purchaser->GetName() : TEXT("<none>"), *GetBuyForLabel().ToString());

    UEconomyService *Economy = GetEconomyService();
    const FPurchaseCost Cost = Economy ? Economy->PreviewCartCost(Cart) : FPurchaseCost();
    Result += FString::Printf(TEXT("\n  Cart (%d lines): %s"), Cart.Num(), *GetCostLineText(Cost).ToString());

    const UCurrencyComponent *Currency = PurchaserCurrency.Get();
    Result += FString::Printf(TEXT("\n  Affordable: %s"),
                              Currency && CanAffordCost(*Currency, Cost) ? TEXT("yes") : TEXT("no"));
    return Result;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GShopBuyForCommand(
    TEXT("wor.Shop.BuyFor"),
    TEXT("PLACEHOLDER (B-316): with a shop open, cycle who the cart is bought for (yourself, then each party member, AI included)."),
    FConsoleCommandWithWorldDelegate::CreateLambda(
        [](UWorld *World)
        {
            for (TObjectIterator<UShopWindowWidget> It; It; ++It)
            {
                if (It->GetWorld() == World && It->IsInViewport())
                {
                    It->HandleBuyForClicked();
                    return;
                }
            }
            UE_LOG(LogTemp, Warning, TEXT("[ShopWindow] wor.Shop.BuyFor: no shop window open"));
        }));
#endif // !UE_BUILD_SHIPPING
