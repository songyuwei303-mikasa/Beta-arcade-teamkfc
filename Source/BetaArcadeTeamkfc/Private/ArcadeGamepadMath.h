#pragma once
#include "CoreMinimal.h"
namespace ArcadeGamepad
{
inline FVector2D Direction(FVector2D Stick, FVector2D DPad, float DeadZone)
{
    if (!DPad.IsNearlyZero()) return DPad.GetClampedToMaxSize(1.0);
    const double Magnitude = Stick.Size();
    const double DZ = FMath::Clamp(double(DeadZone), 0.0, 0.95);
    return Magnitude > DZ ? Stick / Magnitude * FMath::Clamp((Magnitude - DZ) / (1.0 - DZ), 0.0, 1.0) : FVector2D::ZeroVector;
}
inline FVector2D POV(uint32 Value)
{
    if ((Value & 0xffff) == 0xffff || Value >= 36000) return FVector2D::ZeroVector;
    const int32 Octant = ((Value + 2250) / 4500) % 8;
    static const FVector2D Directions[] = {{0,1}, {1,1}, {1,0}, {1,-1}, {0,-1}, {-1,-1}, {-1,0}, {-1,1}};
    return Directions[Octant];
}
}
