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

    // B-333 (D-125): the seven balances — ALWAYS shown, including zero balances; B-361b: and Mentor Tokens.
    TArray<FString> Parts;
    for (const ECurrencyType Currency : CurrencyBalances::ALL)
    {
        Parts.Add(FString::Printf(TEXT("%s: %d"),
                                  *StaticEnum<ECurrencyType>()->GetAuthoredNameStringByValue(static_cast<int64>(Currency)),
                                  Wallet->GetBalance(Currency)));
    }
    FString Out = FString::Join(Parts, TEXT(" | "));

    // OBSOLETE (B-333): leftover typed essence (14 keys) — only NON-zero entries, and only when any.
    const FString EssenceStr = JoinNonZeroEntries<EEssenceType>(
        [Wallet](EEssenceType Type) { return Wallet->GetEssenceType(Type); });
    if (!EssenceStr.IsEmpty())
    {
        Out += FString::Printf(TEXT(" | Obsolete typed essence[%s]"), *EssenceStr);
    }
    return Out;
}
