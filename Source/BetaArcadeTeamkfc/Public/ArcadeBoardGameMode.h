#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ArcadeBoardGameMode.generated.h"

class AArcadeBoardCharacter;
class ACameraActor;

/** Only shared camera and native UI; never creates the input demo widget. */
UCLASS()
class BETAARCADETEAMKFC_API AArcadeBoardController : public APlayerController
{
    GENERATED_BODY()
public:
    AArcadeBoardController();
protected:
    virtual void BeginPlay() override;
};

UCLASS()
class BETAARCADETEAMKFC_API AArcadeBoardGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AArcadeBoardGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void SetBoardInputEnabled(bool bEnabled);
    bool ResetBoardCharacters();
    bool AreBoardCharactersReady() const { return GetSeatCharacter(0) && GetSeatCharacter(1); }
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Seat is 0/1, not a PlayerController index. */
    UFUNCTION(BlueprintPure, Category="Arcade|Board") AArcadeBoardCharacter* GetSeatCharacter(int32 Seat) const;
    /** Explicit return from a UI receiver; never called in Tick. Waits for neutral via the subsystem. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Board") bool RestoreBoardInput(int32 Seat);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Arcade|Board") TSubclassOf<AArcadeBoardCharacter> BoardCharacterClass;
    /** Use one CameraActor with actor tag ArcadeBoardCamera, otherwise spawn this fixed camera. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Arcade|Board") FTransform DefaultCameraTransform;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Arcade|Board", meta=(ClampMin="5", ClampMax="170")) float CameraFOV = 60.f;

    /** Pan toward a single visible window to expose the board on the opposite side. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arcade|Board|Camera Pan") bool bEnableMinigameCameraPan = true;
    /** World centimetres along the camera screen-right axis. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arcade|Board|Camera Pan", meta=(ClampMin="0", Units="cm")) float MinigameCameraPanDistance = 550.f;
    /** Exponential response rate: higher is faster, zero snaps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arcade|Board|Camera Pan", meta=(ClampMin="0")) float MinigameCameraPanSpeed = 4.f;

private:
    friend struct FArcadeBoardCameraTestAccess;
    void UpdateBoardCamera(bool bLeftWindow, bool bRightWindow, float DeltaSeconds);
    UPROPERTY(Transient) TObjectPtr<ACameraActor> BoardCamera;
    FVector CameraCenter = FVector::ZeroVector;
    FVector CameraScreenRight = FVector::RightVector;
    float CameraPanOffset = 0.f;
    bool bBoardInputEnabled = true;
    TArray<FTransform> SpawnTransforms;
    UPROPERTY(Transient) TArray<TObjectPtr<AArcadeBoardCharacter>> SeatCharacters;
};
