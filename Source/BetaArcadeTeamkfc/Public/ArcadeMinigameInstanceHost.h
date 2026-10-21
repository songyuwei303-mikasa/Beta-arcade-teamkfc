#pragma once

#include "CoreMinimal.h"
#include "ArcadeMinigame.h"
#include "ArcadeMinigamePresentation.h"
#include "ArcadeMinigameInstanceHost.generated.h"

class ULevelStreamingDynamic;
class UArcadeInputSubsystem;

UENUM(BlueprintType)
enum class EArcadeInstanceState : uint8 { Idle, Loading, Preparing, Running, Closing, Unloading };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcadeInstanceError, FString, Message);

/** One independently loaded minigame and its presentation; never changes the board camera. */
UCLASS(Blueprintable)
class BETAARCADETEAMKFC_API AArcadeMinigameInstanceHost : public AActor, public IArcadeInputReceiver
{
    GENERATED_BODY()
public:
    AArcadeMinigameInstanceHost();
    /** Native flow teardown, including Loading/Closing. Not a player Exit action. */
    void StopForMatchEnd();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Instance", meta=(ClampMin="0", ClampMax="1")) int32 Seat = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Instance") TSoftObjectPtr<UWorld> MinigameMap;
    /** Translation only; local actor references are remapped by level instancing. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Instance") FVector InstanceLocation = FVector(100000, 0, 0);
    /** Integration testing only. Starts on first Tick, after Board GameMode has installed input. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Instance") bool bStartAutomatically = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Instance", meta=(ClampMin="1")) float TimeoutSeconds = 30;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Presentation") FArcadePresentationSettings PresentationSettings;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Presentation") TObjectPtr<AArcadeMinigamePresentation> Presentation;

    /** True means accepted, not finished loading. Observe State / OnLoadFailed / OnInstanceEnded. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Instance") bool StartMinigame();
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Instance") EArcadeInstanceState State = EArcadeInstanceState::Idle;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Instance") TObjectPtr<UArcadeMinigameSession> ActiveRun;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Instance") TObjectPtr<AArcadeMinigameEntry> ActiveEntry;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Instance") TObjectPtr<ULevelStreamingDynamic> StreamingLevel;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Instance") FString LastError;
    UPROPERTY(BlueprintAssignable, Category="Arcade|Instance") FArcadeInstanceError OnLoadFailed;
    /** Input restored and unload requested; wait for Idle before starting again. */
    UPROPERTY(BlueprintAssignable, Category="Arcade|Instance") FArcadeMinigameEnded OnInstanceEnded;

    // The loading gate consumes gameplay input without creating a second LocalPlayer.
    virtual void ArcadeDirection_Implementation(int32 InSeat, FVector2D Value) override {}
    virtual void ArcadeButton_Implementation(int32 InSeat, EArcadeInputAction Action, bool bPressed) override {}
    virtual void ArcadeInputCancelled_Implementation(int32 InSeat) override {}
private:
    friend struct FArcadeInstanceTestAccess;
    bool bEnablePresentation = true; // Disabled only by native lifecycle automation.
    TWeakObjectPtr<UArcadeInputSubsystem> Input;
    TWeakObjectPtr<UObject> PreviousReceiver;
    int32 ActiveSeat = INDEX_NONE;
    double PhaseStarted = 0;
    bool bFirstTick = true;
    bool bEndingPlay = false;
    bool StartWithInput(UArcadeInputSubsystem* Router);
    void PrepareLoadedLevel();
    void Fail(const FString& Message);
    void BeginUnload();
    void RestoreInput();
};

