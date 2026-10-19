#pragma once
#include "CoreMinimal.h"
#include "ArcadeInputSubsystem.h"
class FArcadeSonyInput
{
public:
    FArcadeSonyInput();
    ~FArcadeSonyInput();
    void Poll(FArcadeInputState* OutFourPads, float DeadZone);
private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
