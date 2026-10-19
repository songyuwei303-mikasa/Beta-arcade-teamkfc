#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ArcadeInputReceiver.generated.h"

UENUM(BlueprintType)
enum class EArcadeInputAction : uint8 { Confirm, Exit };

/** Implement on an Actor, component or widget; no device-specific nodes are needed. */
UINTERFACE(BlueprintType)
class BETAARCADETEAMKFC_API UArcadeInputReceiver : public UInterface
{
    GENERATED_BODY()
};
class BETAARCADETEAMKFC_API IArcadeInputReceiver
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Arcade|Input")
    void ArcadeDirection(int32 Seat, FVector2D Direction);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Arcade|Input")
    void ArcadeButton(int32 Seat, EArcadeInputAction Action, bool bPressed);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Arcade|Input")
    void ArcadeInputCancelled(int32 Seat);
};
