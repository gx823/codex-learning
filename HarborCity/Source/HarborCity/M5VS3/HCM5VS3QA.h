#pragma once
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
// Local opt-in QA only: no console, remote control or network listener.
inline bool HCM5VS3LocalQA()
{
    FString Slot,Mode;
    if(!FParse::Param(FCommandLine::Get(),TEXT("HCM5VS3QA")) ||
       !FParse::Value(FCommandLine::Get(),TEXT("M5Test="),Mode) || !Mode.StartsWith(TEXT("m5_")) ||
       !FParse::Value(FCommandLine::Get(),TEXT("HCM1SaveSlot="),Slot) ||
       !Slot.StartsWith(TEXT("HarborCity_M5_VS1_Test_VS3_")) || Slot.Len()>100) return false;
    for(TCHAR C:Slot) if(!FChar::IsAlnum(C)&&C!=TEXT('_')) return false;
    return true;
}
