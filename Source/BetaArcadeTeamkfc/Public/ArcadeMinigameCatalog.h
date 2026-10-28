#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArcadeMinigame.h"
#include "ArcadeMinigameCatalog.generated.h"

UENUM(BlueprintType)
enum class EArcadeRecordMetric : uint8 { Score, ElapsedSeconds };
UENUM(BlueprintType)
enum class EArcadeRecordDirection : uint8 { HigherWins, LowerWins };
UENUM(BlueprintType)
enum class EArcadeChallengeTag : uint8 { HandSpeed, Calculation, PreciseTiming, Memory, Observation, Planning, Movement, Relaxed };

/** Shared authoring data only. A cell snapshots its rules when a round starts. */
UCLASS(BlueprintType)
class BETAARCADETEAMKFC_API UArcadeMinigameDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade") FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade") TSoftObjectPtr<UWorld> Map;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade") EArcadeRecordMetric Metric = EArcadeRecordMetric::Score;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade") EArcadeRecordDirection Direction = EArcadeRecordDirection::HigherWins;
    /** Score metric only. Equal to the minimum qualifies. Set above zero to exclude zero scores. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade") double MinimumScore = 0;
    /** All metrics: successful completion after this time cannot claim a cell. Zero disables. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade", meta=(ClampMin="0")) double MaxCompletionSeconds = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade") TArray<EArcadeChallengeTag> ChallengeTags;
};

UCLASS(BlueprintType)
class BETAARCADETEAMKFC_API UArcadeMinigamePool : public UDataAsset
{
    GENERATED_BODY()
public:
    /** Edit this array in the Data Asset editor; all cells referencing this asset share one allocation group. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arcade") TArray<TObjectPtr<UArcadeMinigameDefinition>> Games;
    /** Null entries and duplicate map references are removed. No weights / fatigue scheduling yet. */
    UFUNCTION(BlueprintPure, Category="Arcade") TArray<UArcadeMinigameDefinition*> GetUniqueGames() const;
    static TArray<int32> Allocate(int32 GameCount, int32 CellCount, FRandomStream& Random);
};

