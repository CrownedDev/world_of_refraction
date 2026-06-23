// CurrencyComponentDebug.cpp

#include "Currency/CurrencyComponentDebug.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyTypes.h"

namespace
{
    // "<Name>:<Amount>" for every NON-zero value across an enum's real entries (skipping
    // the implicit _MAX), joined ", ". GetAuthoredNameStringByValue yields the short, build-
    // config-independent enumerator name ("Fire", "Weapon"); ReadBalance maps each value to
    // the wallet's current amount via the component's typed getter.
    template <typename TEnum, typename TReadBalance>
    FString JoinNonZeroEntries(TReadBalance &&ReadBalance)
    {
        TArray<FString> Parts;
        if (const UEnum *EnumPtr = StaticEnum<TEnum>())
        {
            for (int32 i = 0; i < EnumPtr->NumEnums() - 1; ++i) // -1 skips the implicit _MAX
            {
                const int64 Value = EnumPtr->GetValueByIndex(i);
                const int32 Balance = ReadBalance(static_cast<TEnum>(Value));
                if (Balance != 0)
                {
                    Parts.Add(FString::Printf(TEXT("%s:%d"),
                                              *EnumPtr->GetAuthoredNameStringByValue(Value), Balance));
                }
            }
        }
        return FString::Join(Parts, TEXT(", "));
    }
}

FString UCurrencyComponentDebug::GetWalletString(const UCurrencyComponent *Wallet)
{
    if (!Wallet)
    {
        return TEXT("Wallet: <null>");
    }

    // Scalar currencies — ALWAYS shown, including zero balances.
    FString Out = FString::Printf(TEXT("Prismas: %d | Prisms: %d | Diamond: %d"),
                                  Wallet->GetBalance(ECurrencyType::Prismas),
                                  Wallet->GetBalance(ECurrencyType::Prisms),
                                  Wallet->GetBalance(ECurrencyType::Diamond));

    // Dust (14 keys) + Essence (2 pools) — only NON-zero entries.
    const FString DustStr = JoinNonZeroEntries<EDustType>(
        [Wallet](EDustType Type) { return Wallet->GetDust(Type); });
    const FString EssenceStr = JoinNonZeroEntries<EEssencePool>(
        [Wallet](EEssencePool Pool) { return Wallet->GetEssence(Pool); });

    Out += FString::Printf(TEXT(" | Dust[%s] | Essence[%s]"), *DustStr, *EssenceStr);
    return Out;
}
