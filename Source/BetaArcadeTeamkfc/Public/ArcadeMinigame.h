#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcadeInputReceiver.h"
#include "ArcadeMinigame.generated.h"

class UArcadeInputSubsystem;
class UArcadeMinigameSubsystem;
class AArcadeMinigameEntry;
class UUserWidget;
class UCameraComponent;

UENUM(BlueprintType)
enum class EArcadeMinigameState : uint8 { Preparing, Ready, Running, Ending, Ended };

UENUM(BlueprintType)
enum class EArcadeMinigameOutcome : uint8 { Completed, Failed, Cancelled };

UENUM(BlueprintType)
enum class EArcadeMinigameEndReason : uint8 { Submitted, Requested, EntryDestroyed, WorldEnded };

USTRUCT(BlueprintType)
struct FArcadeMinigameResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") FGuid SessionId;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") int32 Seat = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") EArcadeMinigameOutcome Outcome = EArcadeMinigameOutcome::Cancelled;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") EArcadeMinigameEndReason Reason = EArcadeMinigameEndReason::Requested;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") double Score = 0;
    /** World game seconds since Start; preparation time is excluded. No automatic focus pause. */
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") double ElapsedSeconds = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcadeMinigameEnded, FArcadeMinigameResult, Result);

/**
 * One immutable run identity. Keep THIS object in delayed callbacks; never look up the seat's
 * newest session to submit an old result. Ended session methods return false.
 */
UCLASS(BlueprintType)
class BETAARCADETEAMKFC_API UArcadeMinigameSession : public UObject, public IArcadeInputReceiver
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Arcade|Minigame") bool MarkReady();
    UFUNCTION(BlueprintCallable, Category="Arcade|Minigame") bool Start();
    UFUNCTION(BlueprintCallable, Category="Arcade|Minigame") bool Finish(double Score, bool bSucceeded = true);
    UFUNCTION(BlueprintCallable, Category="Arcade|Minigame") bool Cancel();
    /** Register only actors created for this run. They must be in this entry's level. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Minigame") bool RegisterSpawnedActor(AActor* Actor);
    UFUNCTION(BlueprintPure, Category="Arcade|Minigame") double GetElapsedSeconds() const;

    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") FGuid SessionId;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") int32 Seat = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") EArcadeMinigameState State = EArcadeMinigameState::Preparing;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") FVector2D Direction = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") bool bConfirmHeld = false;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Minigame") FArcadeMinigameResult Result;

    virtual void ArcadeDirection_Implementation(int32 InSeat, FVector2D Value) override;
    virtual void ArcadeButton_Implementation(int32 InSeat, EArcadeInputAction Action, bool bPressed) override;
    virtual void ArcadeInputCancelled_Implementation(int32 InSeat) override;
private:
    friend class UArcadeMinigameSubsystem;
    friend class AArcadeMinigameEntry;
    TWeakObjectPtr<UArcadeMinigameSubsystem> Manager;
    TWeakObjectPtr<AArcadeMinigameEntry> Entry;
    TWeakObjectPtr<UArcadeInputSubsystem> Input;
    TWeakObjectPtr<UObject> ReturnReceiver;
    TArray<TWeakObjectPtr<AActor>> SpawnedActors;
    double StartSeconds = 0;
    bool bStarted = false;
    bool bChangingRoute = false;
    bool IsCurrent() const;
    bool End(EArcadeMinigameOutcome Outcome, EArcadeMinigameEndReason Reason, double Score);
};

/** Place one blueprint-derived entry per minigame instance. BeginPlay must not start the game. */
UCLASS(Blueprintable)
class BETAARCADETEAMKFC_API AArcadeMinigameEntry : public AActor
{
    GENERATED_BODY()
public:
    AArcadeMinigameEntry();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    UFUNCTION(BlueprintPure, Category="Arcade|Minigame") UArcadeMinigameSession* GetSession() const { return Session; }
    /** The presentation host creates this widget. Gameplay must not AddToViewport itself. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Arcade|Minigame|UI") TSubclassOf<UUserWidget> MinigameWidgetClass;
    /** Available by OnStarted when a widget class is configured, not during OnPrepare. */
    UFUNCTION(BlueprintPure, Category="Arcade|Minigame|UI") UUserWidget* GetMinigameWidget() const { return PresentationWidget; }
    void SetPresentationWidget(UUserWidget* Widget);

    /** Optional explicit actor in THIS level; may be a CameraActor or a character with a camera. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Arcade|Minigame|Camera") TObjectPtr<AActor> CameraSourceActor;
    /** When set, selects a component by its component tag on CameraSourceActor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arcade|Minigame|Camera") FName CameraComponentTag;
    /** Override to return a specific camera component. Hosts validate that it belongs to this instance. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Arcade|Minigame|Camera") UCameraComponent* GetMinigameCamera() const;

    /** Default calls Session.MarkReady. Override for async preparation and mark ready explicitly. */
    UFUNCTION(BlueprintNativeEvent, Category="Arcade|Minigame") void OnPrepare(UArcadeMinigameSession* Run);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Minigame") void OnStarted(UArcadeMinigameSession* Run);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Minigame") void OnDirection(UArcadeMinigameSession* Run, FVector2D Value);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Minigame") void OnButton(UArcadeMinigameSession* Run, EArcadeInputAction Action, bool bPressed);
    /** Input interruption only: clear movement/charge; does NOT end or pause the session. */
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Minigame") void OnInputCancelled(UArcadeMinigameSession* Run);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Minigame") void OnStopped(UArcadeMinigameSession* Run, FArcadeMinigameResult Result);
    /** Clear timers/delegates/audio here. Registered run actors are destroyed after this event. */
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Minigame") void OnCleanup(UArcadeMinigameSession* Run);
private:
    friend class UArcadeMinigameSession;
    friend class UArcadeMinigameSubsystem;
    UPROPERTY(Transient) TObjectPtr<UArcadeMinigameSession> Session;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> PresentationWidget;
    bool bEndingPlay = false;
};

/** Two independent logical-seat sessions, scoped to this world; no loading or camera changes. */
UCLASS()
class BETAARCADETEAMKFC_API UArcadeMinigameSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Reserves seat/entry and suspends its old receiver immediately. Then invokes OnPrepare. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Minigame")
    UArcadeMinigameSession* PrepareSession(int32 Seat, AArcadeMinigameEntry* Entry);
    UFUNCTION(BlueprintPure, Category="Arcade|Minigame") UArcadeMinigameSession* GetSession(int32 Seat) const;
    UPROPERTY(BlueprintAssignable, Category="Arcade|Minigame") FArcadeMinigameEnded OnSessionEnded;

    virtual void OnWorldEndPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    friend class UArcadeMinigameSession;
    friend class AArcadeMinigameInstanceHost;
    friend struct FArcadeMinigameTestAccess;
    UPROPERTY(Transient) TArray<TObjectPtr<UArcadeMinigameSession>> Sessions;
    bool bShuttingDown = false;
    /** A loading host can pass the receiver it suspended before loading (including null). */
    UArcadeMinigameSession* PrepareWithInput(int32 Seat, AArcadeMinigameEntry* Entry, UArcadeInputSubsystem* Input,
        TOptional<TWeakObjectPtr<UObject>> OriginalReceiver = {});
    void Shutdown();
};
