// ShopWindowWidget.cpp

#include "UI/Shop/ShopWindowWidget.h"

#include "Components/Button.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/EconomyService.h"
#include "Currency/EconomyYield.h" // scaling-grade letters for the detail panel
#include "Engine/GameInstance.h"
#include "Equipment/Crystals/EvolutionItemData.h" // single-owned cart-cap casts
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
}

namespace
{
    FString EssenceName(EEssenceType Type)
    {
        return StaticEnum<EEssenceType>()->GetAuthoredNameStringByValue(static_cast<int64>(Type));
    }

    /** "Skill 30 | Fire 25 | Reality 12" over the currencies the cart needs — "—" if none. */
    FString BuildEssenceLine(const FPurchaseCost &Cost,
                             TFunctionRef<int32(ECurrencyType, uint8)> AmountOf)
    {
        TArray<FString> Parts;
        if (Cost.SkillEssence > 0)
        {
            Parts.Add(FString::Printf(TEXT("Skill %d"), AmountOf(ECurrencyType::SkillEssence, 0)));
        }
        for (const TPair<EEssenceType, int32> &Pair : Cost.Typed)
        {
            Parts.Add(FString::Printf(TEXT("%s %d"), *EssenceName(Pair.Key),
                                      AmountOf(ECurrencyType::EssenceTyped, static_cast<uint8>(Pair.Key))));
        }
        return Parts.Num() > 0 ? FString::Join(Parts, TEXT(" | ")) : TEXT("—");
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
            // Effect NAME from the enum display's parenthetical: "Garnet (Fire - Damage)"
            // → "Fire - Damage". Stones without a parenthetical use the display name.
            const FString Display = UEnum::GetDisplayValueAsText(E.Crystal.Type).ToString();
            int32 Open = INDEX_NONE, Close = INDEX_NONE;
            FString EffectName = Display;
            if (Display.FindChar(TEXT('('), Open) && Display.FindLastChar(TEXT(')'), Close) && Close > Open)
            {
                EffectName = Display.Mid(Open + 1, Close - Open - 1);
            }
            Lines.Add(FString::Printf(TEXT("Effect: %s"), *EffectName));
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
                Lines.Add(FString::Printf(TEXT("Crystal: %s (%s)"),
                                          *ItemIdentity::GetTypeName(R->AttachedItem.CrystalType),
                                          *TierHelpers::GetTierName(R->AttachedItem.CrystalTier)));
            }
        }
        else if (const USkillDataBase *Skill = Cast<USkillDataBase>(E.Asset)) // spell + ability
        {
            if (const USpellData *S = Cast<USpellData>(Skill))
            {
                Lines.Add(FString::Printf(TEXT("School: %s | Element: %s"),
                                          *UEnum::GetDisplayValueAsText(S->School).ToString(),
                                          *UEnum::GetDisplayValueAsText(S->Element).ToString()));
            }
            Lines.Add(FString::Printf(TEXT("Damage: %d × %d %s"), Skill->BaseDamage, Skill->HitCount,
                                      Skill->HitCount == 1 ? TEXT("hit") : TEXT("hits")));
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
    RefreshCostAndWallet();
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
    RefreshCostAndWallet();
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
    RefreshCostAndWallet();
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
        RefreshCostAndWallet();
        ShowToast(NSLOCTEXT("Shop", "Purchased", "Purchased!"));
    }
    else
    {
        // Shouldn't fire — Confirm disables while unaffordable; capacity/duplicate edge
        // cases (e.g. inventory filled since the last refresh) land here.
        ShowToast(NSLOCTEXT("Shop", "PurchaseFailed", "Purchase failed"));
        RefreshCostAndWallet();
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
        MerchantType->SetText(FText::FromString(TypeFlavour));
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
        DetailTier->SetText(Obj ? UShopRowWidget::TierFor(Obj->Entry) : FText::GetEmpty());
    }
    if (DetailDescription)
    {
        DetailDescription->SetText(Obj ? UShopRowWidget::DescriptionFor(Obj->Entry) : FText::GetEmpty());
    }
    if (DetailStats)
    {
        DetailStats->SetText(Obj ? FText::FromString(BuildDetailStats(Obj->Entry)) : FText::GetEmpty());
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

void UShopWindowWidget::RefreshCostAndWallet()
{
    UEconomyService *Economy = GetEconomyService();
    const FPurchaseCost Cost = Economy ? Economy->PreviewCartCost(Cart) : FPurchaseCost();

    if (CartTotalPrisms)
    {
        CartTotalPrisms->SetText(FText::AsNumber(Cost.Prisms));
    }
    if (CartTotalEssence)
    {
        // Cost side: amounts are the cart's own totals.
        CartTotalEssence->SetText(FText::FromString(BuildEssenceLine(
            Cost, [&Cost](ECurrencyType Type, uint8 SubKey)
            { return Type == ECurrencyType::SkillEssence ? Cost.SkillEssence
                                                         : Cost.Typed[static_cast<EEssenceType>(SubKey)]; })));
    }

    UCurrencyComponent *Currency = PurchaserCurrency.Get();
    if (WalletPrisms)
    {
        WalletPrisms->SetText(Currency ? FText::AsNumber(Currency->GetBalance(ECurrencyType::Prisms))
                                       : NSLOCTEXT("Shop", "NoWallet", "—"));
    }
    if (WalletEssence)
    {
        // Wallet side mirrors the SAME currencies (cart-relevant), showing balances.
        WalletEssence->SetText(Currency
                                   ? FText::FromString(BuildEssenceLine(
                                         Cost, [Currency](ECurrencyType Type, uint8 SubKey)
                                         { return Currency->GetBalance(Type, SubKey); }))
                                   : NSLOCTEXT("Shop", "NoWallet", "—"));
    }

    if (ConfirmButton)
    {
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
    RefreshCostAndWallet();
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
