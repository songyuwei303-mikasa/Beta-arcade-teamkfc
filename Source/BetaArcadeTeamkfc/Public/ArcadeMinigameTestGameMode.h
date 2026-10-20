#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArcadeMinigameTestGameMode.generated.h"

class AArcadeMinigameEntry;
class UArcadeMinigameSession;
class UUserWidget;
class ACameraActor;

/** Independent-map harness only. Uses the same sessions as the board; no demo input panel. */
UCLASS()
class BETAARCADETEAMKFC_API AArcadeMinigameTestGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AArcadeMinigameTestGameMode();
    virtual void StartPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Arcade|Minigame|Test", meta=(ClampMin="0", ClampMax="1")) int32 TestSeat = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Arcade|Minigame|Test", meta=(ClampMin="1")) float PreparationTimeout = 30.f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arcade|Minigame|Test") TObjectPtr<UArcadeMinigameSession> TestSession;
    /** Requires one entry and one CameraActor tagged ArcadeMinigameCamera in the persistent level. */
    bool FindTestActors(AArcadeMinigameEntry*& OutEntry, ACameraActor*& OutCamera) const;
private:
    friend struct FArcadeMinigameTestAccess;
    UPROPERTY(Transient) TObjectPtr<AArcadeMinigameEntry> TestEntry;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> TestWidget;
    double PreparedAt = 0;
    void TryStartPreparedSession();
};
