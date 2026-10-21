#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcadeMinigame.h"
#include "ArcadeBoardRound.generated.h"
class AArcadeBoardCell;
class AArcadeBoardCharacter;
class AArcadeMinigameInstanceHost;
UENUM(BlueprintType)
enum class EArcadeFlowState : uint8 { MainMenu, Playing, Ending, Results };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcadeFlowChanged, EArcadeFlowState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcadeMatchFinished, int32, WinnerSeat);

/** Map-scoped application/round flow. Loading the world never starts a match. */
UCLASS(Blueprintable)
class BETAARCADETEAMKFC_API AArcadeBoardRound : public AActor
{
    GENERATED_BODY()
public:
    AArcadeBoardRound();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Empty: discover cells in this manager's level when starting each match. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Arcade|Board") TArray<TObjectPtr<AArcadeBoardCell>> Cells;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Arcade|Board") TObjectPtr<AArcadeMinigameInstanceHost> Seat0Host;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Arcade|Board") TObjectPtr<AArcadeMinigameInstanceHost> Seat1Host;
    /** Legacy serialized setting. Deliberately ignored: use StartNewRound from UI. */
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use StartNewRound explicitly.")) bool bInitializeOnBeginPlay = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Board") int32 InitialSeed = -1;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Board") int32 RoundSeed = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Board") bool bRoundReady = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Board") FString LastError;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Flow") EArcadeFlowState FlowState = EArcadeFlowState::MainMenu;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Flow") int32 WinnerSeat = INDEX_NONE;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Flow") TArray<TObjectPtr<AArcadeBoardCell>> WinningCells;
    UPROPERTY(BlueprintAssignable, Category="Arcade|Flow") FArcadeFlowChanged OnFlowStateChanged;
    /** Winner is frozen; cleanup may still be underway. Wait for Results to enable rematch. */
    UPROPERTY(BlueprintAssignable, Category="Arcade|Flow") FArcadeMatchFinished OnMatchFinished;
    /** Only from MainMenu/Results, after cleanup. -1 uses InitialSeed (-1 there means random). */
    UFUNCTION(BlueprintCallable, Category="Arcade|Flow") bool StartNewRound(int32 Seed = -1);
    /** Explicit system/menu action, never bound to minigame Exit. Asynchronous cleanup to MainMenu. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Flow") bool ReturnToMainMenu();
    UFUNCTION(BlueprintPure, Category="Arcade|Flow") bool CanStartNewRound() const;
    UFUNCTION(BlueprintPure, Category="Arcade|Board") AArcadeBoardCell* GetCellAt(int32 Row, int32 Column) const;
    UFUNCTION(BlueprintCallable, Category="Arcade|Board") bool TryStartChallenge(AArcadeBoardCell* Cell, AArcadeBoardCharacter* Character);
    UFUNCTION(BlueprintPure, Category="Arcade|Board") AArcadeBoardCell* GetChallengedCell(int32 Seat) const;
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Board") void OnRoundStarted(int32 Seed);
private:
    friend struct FArcadeFlowTestAccess;
    UPROPERTY(Transient) TArray<TObjectPtr<AArcadeBoardCell>> PendingCells;
    /** Immutable board topology for the active round. Row * 3 + Column. */
    UPROPERTY(Transient) TArray<TObjectPtr<AArcadeBoardCell>> RoundGrid;
    bool bConfigured = false;
    bool bMutating = false;
    bool bDiscoverCells = false;
    bool bCleanupRequested = false;
    bool bOwnHost[2] = {false, false};
    EArcadeFlowState CleanupDestination = EArcadeFlowState::Results;
    AArcadeMinigameInstanceHost* Host(int32 Seat) const;
    bool Reject(const FString& Message);
    void SetFlow(EArcadeFlowState State);
    void BeginFinish(EArcadeFlowState Destination);
    bool EvaluateVictory();
    UFUNCTION() void HandleEnded(FArcadeMinigameResult Result);
    UFUNCTION() void HandleError0(FString Message);
    UFUNCTION() void HandleError1(FString Message);
    void HandleError(int32 Seat, const FString& Message);
};
