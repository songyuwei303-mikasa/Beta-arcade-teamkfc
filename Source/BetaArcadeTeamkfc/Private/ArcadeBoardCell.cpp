#include "ArcadeBoardCell.h"
#include "ArcadeBoardCharacter.h"
#include "ArcadeBoardRound.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
AArcadeBoardCell::AArcadeBoardCell()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    InteractionVolume->SetupAttachment(GetRootComponent());
    InteractionVolume->SetBoxExtent(FVector(140, 140, 120));
    InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    InteractionVolume->SetGenerateOverlapEvents(true);
}
bool AArcadeBoardCell::ContainsCharacter(AArcadeBoardCharacter* Character) const
{
    return IsValid(Character) && Character->GetWorld() == GetWorld() &&
        InteractionVolume->IsOverlappingComponent(Character->GetCapsuleComponent());
}
bool AArcadeBoardCell::TryInteract(AArcadeBoardCharacter* Character)
{
    return Round.IsValid() && Round->TryStartChallenge(this, Character);
}
void AArcadeBoardCell::Assign(AArcadeBoardRound* Manager, UArcadeMinigameDefinition* Game)
{
    Round = Manager;
    AssignedGame = Game;
    RoundMap = Game->Map;
    Metric = Game->Metric;
    Direction = Game->Direction;
    MinimumScore = Game->MinimumScore;
    MaxCompletionSeconds = Game->MaxCompletionSeconds;
    OwnerSeat = INDEX_NONE;
    bHasRecord = false;
    BestValue = 0;
}
void AArcadeBoardCell::Resolve(const FArcadeMinigameResult& Result)
{
    const double Value = Metric == EArcadeRecordMetric::Score ? Result.Score : Result.ElapsedSeconds;
    const bool bQualified = FMath::IsFinite(MinimumScore) && FMath::IsFinite(MaxCompletionSeconds) && MaxCompletionSeconds >= 0 &&
        (Metric != EArcadeRecordMetric::Score || Result.Score >= MinimumScore) &&
        FMath::IsFinite(Result.ElapsedSeconds) && Result.ElapsedSeconds >= 0 &&
        (MaxCompletionSeconds == 0 || Result.ElapsedSeconds <= MaxCompletionSeconds);
    const bool bBetter = bQualified && Result.Seat >= 0 && Result.Seat < 2 && Result.Outcome == EArcadeMinigameOutcome::Completed &&
        Result.Reason == EArcadeMinigameEndReason::Submitted && FMath::IsFinite(Value) &&
        (!bHasRecord || (Direction == EArcadeRecordDirection::HigherWins ? Value > BestValue : Value < BestValue));
    const int32 Previous = OwnerSeat;
    if (bBetter) { BestValue = Value; bHasRecord = true; OwnerSeat = Result.Seat; }
    if (Previous != OwnerSeat) OnOwnerChanged(Previous, OwnerSeat);
    OnChallengeResolved(Result, bBetter);
}

