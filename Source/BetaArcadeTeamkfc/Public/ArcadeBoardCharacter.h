#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArcadeInputReceiver.h"
#include "ArcadeBoardCharacter.generated.h"

/** A logical seat's board character. No LocalPlayer or possession is required. */
UCLASS()
class BETAARCADETEAMKFC_API AArcadeBoardCharacter : public ACharacter, public IArcadeInputReceiver
{
    GENERATED_BODY()
public:
    AArcadeBoardCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void ArcadeDirection_Implementation(int32 Seat, FVector2D Direction) override;
    virtual void ArcadeButton_Implementation(int32 Seat, EArcadeInputAction Action, bool bPressed) override;
    virtual void ArcadeInputCancelled_Implementation(int32 Seat) override;

    // Called by the GameMode before binding input. Screen-up follows this yaw in world XY.
    void InitializeSeat(int32 InSeat, float InMovementYaw);

    UFUNCTION(BlueprintPure, Category="Arcade|Board") int32 GetSeat() const { return LogicalSeat; }
    UFUNCTION(BlueprintPure, Category="Arcade|Board") FVector2D GetMoveDirection() const { return MoveDirection; }

protected:
    /** Receives unconsumed board input. Confirm first attempts native cell interaction. */
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Board")
    void OnBoardButton(int32 Seat, EArcadeInputAction Action, bool bPressed);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Board") void OnBoardInputCancelled(int32 Seat);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arcade|Board") TObjectPtr<class UStaticMeshComponent> PlaceholderBody;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arcade|Board") TObjectPtr<class UTextRenderComponent> SeatLabel;

private:
    int32 LogicalSeat = INDEX_NONE;
    float MovementYaw = 0.f;
    FVector2D MoveDirection = FVector2D::ZeroVector;
    void StopBoardMovement();
};
