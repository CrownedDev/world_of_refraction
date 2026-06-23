// CurrencyComponent.cpp

#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyComponentDebug.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

// ============================================================
// FCurrencyEntry / FCurrencyArray callbacks (need full UCurrencyComponent)
// ============================================================

void FCurrencyEntry::PostReplicatedAdd(const FCurrencyArray &InArray)
{
    InArray.NotifyEntryChanged(Key, Amount);
}

void FCurrencyEntry::PostReplicatedChange(const FCurrencyArray &InArray)
{
    InArray.NotifyEntryChanged(Key, Amount);
}

void FCurrencyArray::NotifyEntryChanged(uint8 Key, int32 Amount) const
{
    if (OwnerComponent)
    {
        OwnerComponent->NotifyChanged(WalletType, Key, Amount);
    }
}

// ============================================================
// UCurrencyComponent
// ============================================================

UCurrencyComponent::UCurrencyComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // Genuinely network-ready (the CharacterDataComponent reference omits this).
    SetIsReplicatedByDefault(true);

    // Stamp wallet wiring once — runs on server AND clients, so client-side
    // FastArray callbacks can route changes back to this component.
    EssenceWallet.OwnerComponent = this;
    EssenceWallet.WalletType = ECurrencyType::EssenceTyped;
}

void UCurrencyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty> &OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UCurrencyComponent, Gold);
    DOREPLIFETIME(UCurrencyComponent, Prisms);
    DOREPLIFETIME(UCurrencyComponent, Diamond);
    DOREPLIFETIME(UCurrencyComponent, GearEssence);
    DOREPLIFETIME(UCurrencyComponent, EssenceWallet);
}

// ==================== UNIFIED API ====================

bool UCurrencyComponent::Add(ECurrencyType Currency, int32 Amount, uint8 SubKey)
{
    if (!HasServerAuthority() || Amount <= 0)
    {
        return false;
    }

    int32 NewBalance = 0;
    switch (Currency)
    {
    case ECurrencyType::Gold:
        Gold += Amount;
        NewBalance = Gold;
        break;
    case ECurrencyType::Prisms:
        Prisms += Amount;
        NewBalance = Prisms;
        break;
    case ECurrencyType::Diamond:
        Diamond += Amount;
        NewBalance = Diamond;
        break;
    case ECurrencyType::EssenceTyped:
    {
        FCurrencyEntry &Entry = FindOrAddEntry(EssenceWallet, SubKey);
        Entry.Amount += Amount;
        EssenceWallet.MarkItemDirty(Entry);
        NewBalance = Entry.Amount;
        break;
    }
    case ECurrencyType::GearEssence:
        GearEssence += Amount;
        NewBalance = GearEssence;
        break;
    }

    // Server-side broadcast (OnRep_* / FastArray callbacks fire on clients only).
    NotifyChanged(Currency, SubKey, NewBalance);
    return true;
}

bool UCurrencyComponent::Spend(ECurrencyType Currency, int32 Amount, uint8 SubKey)
{
    if (!HasServerAuthority() || Amount <= 0)
    {
        return false;
    }

    // All-or-nothing: no partial debit.
    if (!CanAfford(Currency, Amount, SubKey))
    {
        return false;
    }

    int32 NewBalance = 0;
    switch (Currency)
    {
    case ECurrencyType::Gold:
        Gold -= Amount;
        NewBalance = Gold;
        break;
    case ECurrencyType::Prisms:
        Prisms -= Amount;
        NewBalance = Prisms;
        break;
    case ECurrencyType::Diamond:
        Diamond -= Amount;
        NewBalance = Diamond;
        break;
    case ECurrencyType::EssenceTyped:
    {
        FCurrencyEntry &Entry = FindOrAddEntry(EssenceWallet, SubKey);
        Entry.Amount -= Amount; // CanAfford guaranteed >= Amount
        EssenceWallet.MarkItemDirty(Entry);
        NewBalance = Entry.Amount;
        break;
    }
    case ECurrencyType::GearEssence:
        GearEssence -= Amount; // CanAfford guaranteed >= Amount
        NewBalance = GearEssence;
        break;
    }

    NotifyChanged(Currency, SubKey, NewBalance);
    return true;
}

bool UCurrencyComponent::CanAfford(ECurrencyType Currency, int32 Amount, uint8 SubKey) const
{
    return Amount > 0 && GetBalance(Currency, SubKey) >= Amount;
}

int32 UCurrencyComponent::GetBalance(ECurrencyType Currency, uint8 SubKey) const
{
    switch (Currency)
    {
    case ECurrencyType::Gold:
        return Gold;
    case ECurrencyType::Prisms:
        return Prisms;
    case ECurrencyType::Diamond:
        return Diamond;
    case ECurrencyType::EssenceTyped:
        return GetWalletAmount(EssenceWallet, SubKey);
    case ECurrencyType::GearEssence:
        return GearEssence;
    }
    return 0;
}

// ==================== EVENTS ====================

void UCurrencyComponent::NotifyChanged(ECurrencyType Currency, uint8 SubKey, int32 NewBalance)
{
    OnCurrencyChanged.Broadcast(Currency, SubKey, NewBalance);
}

// ==================== DEBUG ====================

void UCurrencyComponent::PrintWallet()
{
    const FString Dump = UCurrencyComponentDebug::GetWalletString(this);
    UE_LOG(LogTemp, Log, TEXT("[CurrencyComponent] %s"), *Dump);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Cyan, Dump);
    }
}

// ==================== REP CALLBACKS ====================

void UCurrencyComponent::OnRep_Gold()
{
    NotifyChanged(ECurrencyType::Gold, 0, Gold);
}

void UCurrencyComponent::OnRep_Prisms()
{
    NotifyChanged(ECurrencyType::Prisms, 0, Prisms);
}

void UCurrencyComponent::OnRep_Diamond()
{
    NotifyChanged(ECurrencyType::Diamond, 0, Diamond);
}

void UCurrencyComponent::OnRep_GearEssence()
{
    NotifyChanged(ECurrencyType::GearEssence, 0, GearEssence);
}

// ==================== INTERNAL ====================

bool UCurrencyComponent::HasServerAuthority() const
{
    // Standalone (PIE, no networking) always has authority.
    if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
    {
        return true;
    }
    return GetOwner() && GetOwner()->HasAuthority();
}

FCurrencyEntry &UCurrencyComponent::FindOrAddEntry(FCurrencyArray &Wallet, uint8 Key)
{
    for (FCurrencyEntry &Entry : Wallet.Items)
    {
        if (Entry.Key == Key)
        {
            return Entry;
        }
    }

    FCurrencyEntry NewEntry;
    NewEntry.Key = Key;
    NewEntry.Amount = 0;
    const int32 Index = Wallet.Items.Add(NewEntry);
    Wallet.MarkArrayDirty();
    return Wallet.Items[Index];
}

int32 UCurrencyComponent::GetWalletAmount(const FCurrencyArray &Wallet, uint8 Key) const
{
    for (const FCurrencyEntry &Entry : Wallet.Items)
    {
        if (Entry.Key == Key)
        {
            return Entry.Amount;
        }
    }
    return 0;
}
