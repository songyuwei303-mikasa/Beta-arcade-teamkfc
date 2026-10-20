#pragma once
#include "CoreMinimal.h"
#include "ArcadeInputSubsystem.h"
class FArcadeSonyInput
{
public:
    FArcadeSonyInput();
    ~FArcadeSonyInput();
    void Poll(FArcadeInputState* OutFourPads, float DeadZone);
    /** Hotplug scan. Do not call every frame: EnumDevices stalls the game thread. */
    void RequestDeviceScan();
private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
