#include "ArcadeBoardRound.h"
#include "ArcadeBoardCell.h"
#include "ArcadeBoardCharacter.h"
#include "ArcadeBoardGameMode.h"
#include "ArcadeMinigameInstanceHost.h"
#include "ArcadeInputSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

AArcadeBoardRound::AArcadeBoardRound()
{
    PrimaryActorTick.bCanEverTick = true;
    PendingCells.SetNum(2);
}
AArcadeMinigameInstanceHost* AArcadeBoardRound::Host(int32 Seat) const
{ return Seat == 0 ? Seat0Host.Get() : Seat == 1 ? Seat1Host.Get() : nullptr; }
AArcadeBoardCell* AArcadeBoardRound::GetChallengedCell(int32 Seat) const
{ return PendingCells.IsValidIndex(Seat) ? PendingCells[Seat].Get() : nullptr; }
void AArcadeBoardRound::BeginPlay()
{
    Super::BeginPlay();
    for (TActorIterator<AArcadeBoardRound> It(GetWorld()); It; ++It)
        if (*It != this) { LastError = TEXT("Place exactly one BoardRound manager."); return; }
    bDiscoverCells = Cells.IsEmpty();
    if (bDiscoverCells)
        for (TActorIterator<AArcadeBoardCell> It(GetWorld()); It; ++It)
            if (It->GetLevel() == GetLevel()) Cells.Add(*It);
    Cells.RemoveAll([](const TObjectPtr<AArcadeBoardCell>& Cell) { return !IsValid(Cell); });
    Cells.Sort([](const AArcadeBoardCell& A, const AArcadeBoardCell& B) { return A.GetPathName() < B.GetPathName(); });
    for (int32 Seat = 0; Seat < 2; ++Seat)
    {
        AArcadeMinigameInstanceHost* Selected = Host(Seat);
        for (TActorIterator<AArcadeMinigameInstanceHost> It(GetWorld()); It; ++It)
            if (It->GetLevel() == GetLevel() && It->Seat == Seat)
            {
                if (Selected && Selected != *It) { LastError = TEXT("Duplicate hosts for a seat."); return; }
                Selected = *It;
            }
        if (!Selected)
        {
            Selected = GetWorld()->SpawnActor<AArcadeMinigameInstanceHost>();
            if (!Selected) { LastError = TEXT("Host creation failed."); return; }
            Selected->Seat = Seat;
            Selected->InstanceLocation = FVector(100000, Seat * 20000, 0);
            bOwnHost[Seat] = true;
        }
        if (Selected->GetWorld() != GetWorld() || Selected->Seat != Seat)
        { LastError = TEXT("Host must belong to this world and the configured seat."); return; }
        Selected->bStartAutomatically = false;
        if (Seat == 0) Seat0Host = Selected; else Seat1Host = Selected;
        Selected->OnInstanceEnded.AddDynamic(this, &AArcadeBoardRound::HandleEnded);
    }
    Seat0Host->OnLoadFailed.AddDynamic(this, &AArcadeBoardRound::HandleError0);
    Seat1Host->OnLoadFailed.AddDynamic(this, &AArcadeBoardRound::HandleError1);
    GetWorld()->GetSubsystem<UArcadeMinigameSubsystem>()->OnSessionEnded.AddDynamic(this, &AArcadeBoardRound::HandleEnded);
    bConfigured = true;
    if (auto* Board = Cast<AArcadeBoardGameMode>(GetWorld()->GetAuthGameMode())) Board->SetBoardInputEnabled(false);
}
bool AArcadeBoardRound::Reject(const FString& Message)
{
    LastError = Message;
    return false;
}
bool AArcadeBoardRound::CanStartNewRound() const
{
    if (!bConfigured || GetWorld()->bIsTearingDown ||
        (FlowState != EArcadeFlowState::MainMenu && FlowState != EArcadeFlowState::Results)) return false;
    for (int32 Seat = 0; Seat < 2; ++Seat)
        if (!IsValid(Host(Seat)) || Host(Seat)->State != EArcadeInstanceState::Idle || PendingCells[Seat]) return false;
    return true;
}
void AArcadeBoardRound::SetFlow(EArcadeFlowState State)
{
    FlowState = State;
    bRoundReady = State == EArcadeFlowState::Playing;
    OnFlowStateChanged.Broadcast(State);
}
AArcadeBoardCell* AArcadeBoardRound::GetCellAt(int32 Row, int32 Column) const
{
    const int32 Index = Row * 3 + Column;
    return Row >= 0 && Row < 3 && Column >= 0 && Column < 3 && RoundGrid.IsValidIndex(Index) ? RoundGrid[Index].Get() : nullptr;
}
bool AArcadeBoardRound::StartNewRound(int32 Seed)
{
    if (bMutating || !CanStartNewRound()) return Reject(TEXT("Start requires MainMenu/Results and two idle hosts. Wait for cleanup; use ReturnToMainMenu to abandon a match."));
    TGuardValue<bool> Guard(bMutating, true);
    auto* Board = Cast<AArcadeBoardGameMode>(GetWorld()->GetAuthGameMode());
    if (Board && !Board->AreBoardCharactersReady()) return Reject(TEXT("Board characters are not ready. Start from UI after world startup."));
    TArray<TObjectPtr<AArcadeBoardCell>> Candidates = Cells;
    if (bDiscoverCells)
    {
        Candidates.Reset();
        for (TActorIterator<AArcadeBoardCell> It(GetWorld()); It; ++It)
            if (It->GetLevel() == GetLevel()) Candidates.Add(*It);
    }
    if (Candidates.Num() != 9) return Reject(FString::Printf(TEXT("Expected exactly 9 cells, found %d. Check the explicit Cells list or extra test actors."), Candidates.Num()));
    TArray<TObjectPtr<AArcadeBoardCell>> Grid;
    Grid.Init(nullptr, 9);
    TArray<UArcadeMinigamePool*> Pools;
    for (AArcadeBoardCell* Cell : Candidates)
    {
        if (!IsValid(Cell) || Cell->GetWorld() != GetWorld()) return Reject(TEXT("Cells contains an invalid actor/world reference."));
        if (Cell->Row < 0 || Cell->Row > 2 || Cell->Column < 0 || Cell->Column > 2)
            return Reject(FString::Printf(TEXT("%s: set Row and Column to 0..2."), *Cell->GetName()));
        const int32 Index = Cell->Row * 3 + Cell->Column;
        if (Grid[Index]) return Reject(FString::Printf(TEXT("Duplicate coordinate (%d,%d): %s and %s."), Cell->Row, Cell->Column, *Grid[Index]->GetName(), *Cell->GetName()));
        if (!IsValid(Cell->Pool) || Cell->Pool->GetUniqueGames().IsEmpty())
            return Reject(FString::Printf(TEXT("%s: Pool is missing or has no valid game maps."), *Cell->GetName()));
        Grid[Index] = Cell;
    }
    // Coordinate order, never actor name or placement order, defines reproducible allocation.
    for (AArcadeBoardCell* Cell : Grid) Pools.AddUnique(Cell->Pool);
    TArray<int32> PreviousOwners;
    for (AArcadeBoardCell* Cell : Grid) PreviousOwners.Add(Cell->OwnerSeat);
    RoundSeed = Seed == -1 ? InitialSeed : Seed;
    if (RoundSeed == -1) RoundSeed = FMath::Rand();
    FRandomStream Random(RoundSeed);
    for (auto* Pool : Pools)
    {
        TArray<AArcadeBoardCell*> Group;
        for (AArcadeBoardCell* Cell : Grid) if (Cell->Pool == Pool) Group.Add(Cell);
        const auto Games = Pool->GetUniqueGames();
        const auto Indices = UArcadeMinigamePool::Allocate(Games.Num(), Group.Num(), Random);
        for (int32 I = 0; I < Group.Num(); ++I) Group[I]->Assign(this, Games[Indices[I]]);
    }
    Cells = Grid;
    RoundGrid = Grid;
    WinnerSeat = INDEX_NONE;
    WinningCells.Reset();
    LastError.Reset();
    if (Board) { Board->ResetBoardCharacters(); Board->SetBoardInputEnabled(true); }
    // Publish all native state before any presentation callbacks.
    FlowState = EArcadeFlowState::Playing;
    bRoundReady = true;
    for (int32 I = 0; I < Cells.Num(); ++I)
        if (IsValid(Cells[I]))
        {
            if (PreviousOwners[I] != INDEX_NONE) Cells[I]->OnOwnerChanged(PreviousOwners[I], INDEX_NONE);
            Cells[I]->OnRoundAssigned();
        }
    OnFlowStateChanged.Broadcast(FlowState);
    OnRoundStarted(RoundSeed);
    return true;
}
bool AArcadeBoardRound::ReturnToMainMenu()
{
    if (!bConfigured || bMutating || FlowState == EArcadeFlowState::Ending) return false;
    if (FlowState == EArcadeFlowState::MainMenu) return true;
    TGuardValue<bool> Guard(bMutating, true);
    BeginFinish(EArcadeFlowState::MainMenu);
    return true;
}
void AArcadeBoardRound::BeginFinish(EArcadeFlowState Destination)
{
    CleanupDestination = Destination;
    bCleanupRequested = true;
    PendingCells.Init(nullptr, 2); // invalidate ALL result destinations before cancelling anything
    FlowState = EArcadeFlowState::Ending;
    bRoundReady = false;
    if (auto* Board = Cast<AArcadeBoardGameMode>(GetWorld()->GetAuthGameMode())) Board->SetBoardInputEnabled(false);
    OnFlowStateChanged.Broadcast(FlowState);
}
bool AArcadeBoardRound::EvaluateVictory()
{
    if (FlowState != EArcadeFlowState::Playing || RoundGrid.Num() != 9) return false;
    static constexpr int32 Lines[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
    for (const auto& Line : Lines)
    {
        auto* A = RoundGrid[Line[0]].Get(); auto* B = RoundGrid[Line[1]].Get(); auto* C = RoundGrid[Line[2]].Get();
        if (!IsValid(A) || !IsValid(B) || !IsValid(C)) continue;
        const int32 Seat = A->OwnerSeat;
        if (Seat >= 0 && Seat < 2 && B->OwnerSeat == Seat && C->OwnerSeat == Seat)
        {
            WinnerSeat = Seat;
            WinningCells = {A, B, C};
            BeginFinish(EArcadeFlowState::Results);
            OnMatchFinished.Broadcast(WinnerSeat);
            return true;
        }
    }
    return false;
}
bool AArcadeBoardRound::TryStartChallenge(AArcadeBoardCell* Cell, AArcadeBoardCharacter* Character)
{
    if (!bRoundReady || bMutating || !IsValid(Cell) || !IsValid(Character) || !Cells.Contains(Cell) ||
        Cell->Round.Get() != this || !Cell->ContainsCharacter(Character)) return false;
    const int32 Seat = Character->GetSeat();
    auto* Instance = Host(Seat);
    auto* Board = Cast<AArcadeBoardGameMode>(GetWorld()->GetAuthGameMode());
    auto* GI = GetGameInstance();
    auto* Input = GI ? GI->GetSubsystem<UArcadeInputSubsystem>() : nullptr;
    if (!IsValid(Instance) || !Board || Board->GetSeatCharacter(Seat) != Character || !Input ||
        Input->GetInputReceiver(Seat) != Character || Instance->State != EArcadeInstanceState::Idle || PendingCells[Seat]) return false;
    TGuardValue<bool> Guard(bMutating, true);
    PendingCells[Seat] = Cell;
    Instance->MinigameMap = Cell->RoundMap;
    if (!Instance->StartMinigame()) { PendingCells[Seat] = nullptr; return false; }
    Cell->OnChallengeStarted(Seat);
    return true;
}
void AArcadeBoardRound::HandleEnded(FArcadeMinigameResult Result)
{
    if (FlowState != EArcadeFlowState::Playing || bMutating) return;
    TGuardValue<bool> Guard(bMutating, true);
    auto* Instance = Host(Result.Seat);
    if (!IsValid(Instance) || !Instance->ActiveRun || Instance->ActiveRun->SessionId != Result.SessionId) return;
    auto* Cell = GetChallengedCell(Result.Seat);
    PendingCells[Result.Seat] = nullptr; // clear before Blueprint callbacks; duplicate notifications are harmless
    if (IsValid(Cell)) { Cell->Resolve(Result); EvaluateVictory(); }
}
void AArcadeBoardRound::HandleError0(FString Message) { HandleError(0, Message); }
void AArcadeBoardRound::HandleError1(FString Message) { HandleError(1, Message); }
void AArcadeBoardRound::HandleError(int32 Seat, const FString& Message)
{
    auto* Cell = GetChallengedCell(Seat);
    PendingCells[Seat] = nullptr;
    LastError = Message;
    if (IsValid(Cell)) Cell->OnChallengeError(Seat, Message);
}
void AArcadeBoardRound::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (FlowState == EArcadeFlowState::Ending)
    {
        TGuardValue<bool> Guard(bMutating, true);
        // Deferred one tick: never tear down a session inside its own result callback.
        if (bCleanupRequested)
        {
            bCleanupRequested = false;
            for (int32 Seat = 0; Seat < 2; ++Seat) if (IsValid(Host(Seat))) Host(Seat)->StopForMatchEnd();
        }
        for (int32 Seat = 0; Seat < 2; ++Seat)
            if (IsValid(Host(Seat)) && Host(Seat)->State != EArcadeInstanceState::Idle) return;
        SetFlow(CleanupDestination);
        return;
    }
    for (int32 Seat = 0; Seat < 2; ++Seat)
        if (PendingCells[Seat] && (!IsValid(Host(Seat)) || Host(Seat)->State == EArcadeInstanceState::Idle))
            HandleError(Seat, TEXT("Challenge host was removed or reset."));
}
void AArcadeBoardRound::EndPlay(const EEndPlayReason::Type Reason)
{
    bRoundReady = false;
    if (auto* Sessions = GetWorld()->GetSubsystem<UArcadeMinigameSubsystem>()) Sessions->OnSessionEnded.RemoveAll(this);
    for (int32 Seat = 0; Seat < 2; ++Seat)
        if (auto* Instance = Host(Seat); IsValid(Instance))
        {
            Instance->OnInstanceEnded.RemoveAll(this);
            Instance->OnLoadFailed.RemoveAll(this);
            if (bOwnHost[Seat] || PendingCells[Seat]) Instance->Destroy();
        }
    PendingCells.Init(nullptr, 2);
    Super::EndPlay(Reason);
}




