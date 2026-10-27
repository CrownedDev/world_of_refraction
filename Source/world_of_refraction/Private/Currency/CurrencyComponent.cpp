// CurrencyComponent.cpp

#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyComponentDebug.h"
#include "WoRAuthority.h"
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
    DOREPLIFETIME(UCurrencyComponent, SkillEssence);
    DOREPLIFETIME(UCurrencyComponent, EssenceWallet);
}

// ==================== UNIFIED API ====================

namespace CurrencyComponentInternal
{
    /** B-075: a balance stops at the int32 limit instead of wrapping negative. Not a design cap. */
    int32 SaturatingAdd(int32 Balance, int32 Amount)
    {
        return static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Balance) + Amount, TNumericLimits<int32>::Max()));
    }
}

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
        Gold = CurrencyComponentInternal::SaturatingAdd(Gold, Amount);
        NewBalance = Gold;
        break;
    case ECurrencyType::Prisms:
        Prisms = CurrencyComponentInternal::SaturatingAdd(Prisms, Amount);
        NewBalance = Prisms;
        break;
    case ECurrencyType::Diamond:
        Diamond = CurrencyComponentInternal::SaturatingAdd(Diamond, Amount);
        NewBalance = Diamond;
        break;
    case ECurrencyType::EssenceTyped:
    {
        FCurrencyEntry &Entry = FindOrAddEntry(EssenceWallet, SubKey);
        Entry.Amount = CurrencyComponentInternal::SaturatingAdd(Entry.Amount, Amount);
        EssenceWallet.MarkItemDirty(Entry);
        NewBalance = Entry.Amount;
        break;
    }
    case ECurrencyType::GearEssence:
        GearEssence = CurrencyComponentInternal::SaturatingAdd(GearEssence, Amount);
        NewBalance = GearEssence;
        break;
    case ECurrencyType::SkillEssence:
        SkillEssence = CurrencyComponentInternal::SaturatingAdd(SkillEssence, Amount);
        NewBalance = SkillEssence;
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
    case ECurrencyType::SkillEssence:
        SkillEssence -= Amount; // CanAfford guaranteed >= Amount
        NewBalance = SkillEssence;
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
    case ECurrencyType::SkillEssence:
        return SkillEssence;
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

void UCurrencyComponent::OnRep_SkillEssence()
{
    NotifyChanged(ECurrencyType::SkillEssence, 0, SkillEssence);
}

// ==================== INTERNAL ====================

bool UCurrencyComponent::HasServerAuthority() const
{
    return WoRAuthority::HasAuthority(GetOwner());
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

void UCurrencyComponent::CopyBalancesFrom(const UCurrencyComponent &Other)
{
    Gold = Other.Gold;
    Prisms = Other.Prisms;
    Diamond = Other.Diamond;
    GearEssence = Other.GearEssence;
    SkillEssence = Other.SkillEssence;
    EssenceWallet.Items = Other.EssenceWallet.Items;
    EssenceWallet.MarkArrayDirty();
    UE_LOG(LogTemp, Log, TEXT("[Currency] %s: balances carried over from %s (Gold %d, Prisms %d)"), *GetNameSafe(GetOwner()),
           *GetNameSafe(Other.GetOwner()), Gold, Prisms);
}
