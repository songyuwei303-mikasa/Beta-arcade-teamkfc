#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcadeMinigameCatalog.h"
#include "ArcadeBoardCell.generated.h"
class UBoxComponent;
class AArcadeBoardCharacter;
class AArcadeBoardRound;

/** No mesh assumptions: attach any floor, arcade cabinet, VFX or widget in a derived Blueprint. */
UCLASS(Blueprintable)
class BETAARCADETEAMKFC_API AArcadeBoardCell : public AActor
{
    GENERATED_BODY()
public:
    AArcadeBoardCell();
    /** Explicit logical coordinates; independent of name, array order and world transform. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Cell", meta=(ClampMin="0", ClampMax="2")) int32 Row = -1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Cell", meta=(ClampMin="0", ClampMax="2")) int32 Column = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arcade|Cell") TObjectPtr<UBoxComponent> InteractionVolume;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade|Cell") TObjectPtr<UArcadeMinigamePool> Pool;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Cell") TObjectPtr<UArcadeMinigameDefinition> AssignedGame;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Cell") int32 OwnerSeat = INDEX_NONE;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Cell") bool bHasRecord = false;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Cell") double BestValue = 0;
    UFUNCTION(BlueprintPure, Category="Arcade|Cell") bool ContainsCharacter(AArcadeBoardCharacter* Character) const;
    UFUNCTION(BlueprintCallable, Category="Arcade|Cell") bool TryInteract(AArcadeBoardCharacter* Character);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Cell") void OnRoundAssigned();
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Cell") void OnChallengeStarted(int32 Seat);
    /** Fired for every settled challenge, after native record update; useful for scores / animation. */
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Cell") void OnChallengeResolved(FArcadeMinigameResult Result, bool bNewRecord);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Cell") void OnOwnerChanged(int32 PreviousSeat, int32 NewSeat);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|Cell") void OnChallengeError(int32 Seat, const FString& Message);
private:
    friend class AArcadeBoardRound;
    friend struct FArcadeBoardCellTestAccess;
    TWeakObjectPtr<AArcadeBoardRound> Round;
    TSoftObjectPtr<UWorld> RoundMap;
    EArcadeRecordMetric Metric = EArcadeRecordMetric::Score;
    EArcadeRecordDirection Direction = EArcadeRecordDirection::HigherWins;
    void Assign(AArcadeBoardRound* Manager, UArcadeMinigameDefinition* Game);
    void Resolve(const FArcadeMinigameResult& Result);
};

