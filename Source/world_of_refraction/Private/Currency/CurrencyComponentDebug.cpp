// CurrencyComponentDebug.cpp

#include "Currency/CurrencyComponentDebug.h"
#include "Currency/CurrencyComponent.h"
#include "Currency/CurrencyTypes.h"

namespace
{
    // "<Name>:<Amount>" for every NON-zero value across an enum's real entries (skipping
    // the implicit _MAX), joined ", ". GetAuthoredNameStringByValue yields the short, build-
    // config-independent enumerator name ("Fire", "Mind"); ReadBalance maps each value to
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
    FString Out = FString::Printf(TEXT("Gold: %d | Prisms: %d | Diamond: %d | GearEssence: %d"),
                                  Wallet->GetBalance(ECurrencyType::Gold),
                                  Wallet->GetBalance(ECurrencyType::Prisms),
                                  Wallet->GetBalance(ECurrencyType::Diamond),
                                  Wallet->GetBalance(ECurrencyType::GearEssence));

    // Typed Essence (14 keys) — only NON-zero entries.
    const FString EssenceStr = JoinNonZeroEntries<EEssenceType>(
        [Wallet](EEssenceType Type) { return Wallet->GetEssenceType(Type); });

    Out += FString::Printf(TEXT(" | Essence[%s]"), *EssenceStr);
    return Out;
}
