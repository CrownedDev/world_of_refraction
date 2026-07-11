// ShopWindowWidget.cpp

#include "UI/Shop/ShopWindowWidget.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h" // scaling-grade letters for the detail panel
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/CrystalDescription.h" // GetItemEffectText for the crystal Effect line
#include "Equipment/Crystals/CrystalTypeHelpers.h" // GetElement for ring attached-crystal line
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

namespace ShopWindowConstants
{
    constexpr float TOAST_DURATION_SECONDS = 1.5f;
    const FLinearColor INSUFFICIENT_RED(1.0f, 0.25f, 0.25f);
}

namespace
{
    FString EssenceName(EEssenceType Type)
    {
        return StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Type));
    }

    bool CanAffordCost(const UCurrencyComponent &Currency, const FPurchaseCost &Cost)
    {
        if (!Currency.CanAfford(ECurrencyType::Prisms, Cost.Prisms))
        {
            return false;
        }
        if (Cost.SkillEssence > 0 && !Currency.CanAfford(ECurrencyType::SkillEssence, Cost.SkillEssence))
        {
            return false;
        }
        for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
        {
            if (!Currency.CanAfford(ECurrencyType::EssenceTyped, Pair.Value, static_cast<uint8>(Pair.Key)))
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
            // Element from the enum display's parenthetical ("Garnet (Fire - Damage)" →
            // Fire); stones have no parenthetical → no Element line. Effect is the full
            // mechanical sentence (3j — numbers on stone effect text accepted per Crown).
            const FString Display = UEnum::GetDisplayValueAsText(E.Crystal.Type).ToString();
            int32 Open = INDEX_NONE, Close = INDEX_NONE;
            if (Display.FindChar(TEXT('('), Open) && Display.FindLastChar(TEXT(')'), Close) && Close > Open)
            {
                const FString Inner = Display.Mid(Open + 1, Close - Open - 1);
                FString ElementPart, EffectPart;
                if (Inner.Split(TEXT(" - "), &ElementPart, &EffectPart))
                {
                    Lines.Add(FString::Printf(TEXT("Element: %s"), *ElementPart));
                }
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
                                          *UEnum::GetDisplayValueAsText(CrystalTypeHelpers::GetElement(R->AttachedItem.CrystalType)).ToString()));
            }
        }
        else if (const UEvolutionItemData *Ev = Cast<UEvolutionItemData>(E.Asset))
        {
            // 3f #7: element IS shown for evolutions — Crown reversed the 3e
            // deliberately-bare call. Still no effect reveal.
            Lines.Add(FString::Printf(TEXT("Element: %s"),
                                      *UEnum::GetDisplayValueAsText(Ev->GetAssociatedElement()).ToString()));
        }
        else if (const USkillDataBase *Skill = Cast<USkillDataBase>(E.Asset)) // spell + ability
        {
            if (const USpellData *S = Cast<USpellData>(Skill))
            {
                Lines.Add(FString::Printf(TEXT("School: %s"), *UEnum::GetDisplayValueAsText(S->School).ToString()));
                Lines.Add(FString::Printf(TEXT("Element: %s"), *UEnum::GetDisplayValueAsText(S->Element).ToString()));
            }
            Lines.Add(FString::Printf(TEXT("Damage: %d"), Skill->BaseDamage));
            Lines.Add(FString::Printf(TEXT("Hits: %d"), Skill->HitCount));
            Lines.Add(FString::Printf(TEXT("Energy: %d"), Skill->BaseEnergyCost));
            if (Skill->StatusBuildup > 0)
            {
                Lines.Add(FString::Printf(TEXT("Status buildup: %d"), Skill->StatusBuildup));
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
    PurchaserCurrency = PurchaserPawn ? PurchaserPawn->FindComponentByClass<UCurrencyComponent>() : nullptr;

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

    if (Economy->Purchase(Pawn, Cart))
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
        const FString TypeDisplay = UEnum::GetDisplayValueAsText(ActiveMerchant->MerchantType).ToString();
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
        for (const FMerchantStockEntry &E : ActiveMerchant->Stock)
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
        FString CostLine;
        UEconomyService *Economy = GetEconomyService();
        if (Obj && Economy)
        {
            // Same builder Purchase charges with — one-entry cart, Count included.
            const FPurchaseCost Cost = Economy->PreviewCartCost({Obj->Entry});
            CostLine = FString::Printf(TEXT("Cost: %d P"), Cost.Prisms);
            if (Cost.SkillEssence > 0)
            {
                CostLine += FString::Printf(TEXT(" | Skill %d"), Cost.SkillEssence);
            }
            for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
            {
                CostLine += FString::Printf(TEXT(" | %s %d"),
                                            *StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key)),
                                            Pair.Value);
            }
        }
        DetailCost->SetText(FText::FromString(CostLine));
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

    const UEnum *EssenceEnum = StaticEnum<EEssenceType>();
    const int32 EssenceCount = EssenceEnum->NumEnums() - 1; // trailing autogenerated _MAX
    const int32 RowCount = 1 /*header*/ + 2 /*Prisms, Skill*/ + EssenceCount;

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
    TotalsRows[1].Label->SetText(NSLOCTEXT("Shop", "TotalsPrisms", "Prisms"));
    TotalsRows[2].Label->SetText(NSLOCTEXT("Shop", "TotalsSkill", "Skill Essence"));
    for (int32 e = 0; e < EssenceCount; ++e)
    {
        TotalsRows[3 + e].Label->SetText(FText::FromString(
            FString::Printf(TEXT("%s Essence"), *EssenceEnum->GetAuthoredNameStringByValue(e))));
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
        const int32 EssenceCount = StaticEnum<EEssenceType>()->NumEnums() - 1;
        bool bAnyVisible = false;

        for (int32 i = 0; i < 2 + EssenceCount; ++i)
        {
            const FTotalsRow &Row = TotalsRows[1 + i];
            int32 CostAmount = 0;
            int32 WalletAmount = 0;
            if (i == 0)
            {
                CostAmount = Cost.Prisms;
                WalletAmount = Currency ? Currency->GetBalance(ECurrencyType::Prisms) : 0;
            }
            else if (i == 1)
            {
                CostAmount = Cost.SkillEssence;
                WalletAmount = Currency ? Currency->GetBalance(ECurrencyType::SkillEssence) : 0;
            }
            else
            {
                const EEssenceType Essence = static_cast<EEssenceType>(i - 2);
                CostAmount = Cost.Typed.FindRef(Essence);
                WalletAmount = Currency ? Currency->GetBalance(ECurrencyType::EssenceTyped, static_cast<uint8>(Essence)) : 0;
            }

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

FString UShopWindowWidget::GetShopString() const
{
    FString Result = FString::Printf(TEXT("ShopWindow — merchant: %s, purchaser: %s"),
                                     ActiveMerchant ? *ActiveMerchant->GetName() : TEXT("<none>"),
                                     Purchaser.IsValid() ? *Purchaser->GetName() : TEXT("<none>"));

    UEconomyService *Economy = GetEconomyService();
    const FPurchaseCost Cost = Economy ? Economy->PreviewCartCost(Cart) : FPurchaseCost();
    Result += FString::Printf(TEXT("\n  Cart (%d lines): %d Prisms, %d Skill"), Cart.Num(), Cost.Prisms, Cost.SkillEssence);
    for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
    {
        Result += FString::Printf(TEXT(", %d %s"), Pair.Value, *EssenceName(Pair.Key));
    }

    const UCurrencyComponent *Currency = PurchaserCurrency.Get();
    Result += FString::Printf(TEXT("\n  Affordable: %s"),
                              Currency && CanAffordCost(*Currency, Cost) ? TEXT("yes") : TEXT("no"));
    return Result;
}
