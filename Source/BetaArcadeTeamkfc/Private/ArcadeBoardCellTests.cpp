#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ArcadeBoardCell.h"
#include "ArcadeBoardRound.h"
#include "ArcadeMinigameInstanceHost.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include <limits>
#include "UObject/StrongObjectPtr.h"
#include "ArcadeBoardCharacter.h"
#include "ArcadeMenuWidget.h"
#include "ArcadeBoardGameMode.h"
#include "ArcadeInputSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Tests/AutomationCommon.h"
struct FArcadeBoardCellTestAccess
{
    static void Assign(AArcadeBoardCell* Cell, UArcadeMinigameDefinition* Game) { Cell->Assign(nullptr, Game); }
    static void Resolve(AArcadeBoardCell* Cell, const FArcadeMinigameResult& Result) { Cell->Resolve(Result); }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadePoolTest, "Arcade.Board.PoolCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadePoolTest::RunTest(const FString&)
{
    for (int32 Seed = 0; Seed < 100; ++Seed)
        for (int32 Games = 1; Games <= 12; ++Games)
            for (int32 Cells = 1; Cells <= 9; ++Cells)
            {
                FRandomStream Random(Seed), Replay(Seed);
                const auto Picks = UArcadeMinigamePool::Allocate(Games, Cells, Random);
                TestEqual(TEXT("One assignment per cell"), Picks.Num(), Cells);
                TestTrue(TEXT("Seed is reproducible"), Picks == UArcadeMinigamePool::Allocate(Games, Cells, Replay));
                TSet<int32> Unique;
                for (int32 Pick : Picks) { TestTrue(TEXT("Valid game index"), Pick >= 0 && Pick < Games); Unique.Add(Pick); }
                TestEqual(TEXT("No repeats when sufficient; full coverage otherwise"), Unique.Num(), FMath::Min(Games, Cells));
            }
    auto* Pool = NewObject<UArcadeMinigamePool>();
    auto* A = NewObject<UArcadeMinigameDefinition>();
    auto* B = NewObject<UArcadeMinigameDefinition>();
    A->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/A.A")));
    B->Map = A->Map;
    Pool->Games = {nullptr, A, A, B};
    TestEqual(TEXT("Duplicate map assets cannot fake pool diversity"), Pool->GetUniqueGames().Num(), 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeCellRecordTest, "Arcade.Board.CellRecords", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeCellRecordTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* A = World->SpawnActor<AArcadeBoardCell>();
    auto* B = World->SpawnActor<AArcadeBoardCell>();
    auto* Game = NewObject<UArcadeMinigameDefinition>();
    FArcadeBoardCellTestAccess::Assign(A, Game);
    FArcadeBoardCellTestAccess::Assign(B, Game);
    FArcadeMinigameResult Result;
    Result.Seat = 0; Result.Outcome = EArcadeMinigameOutcome::Completed; Result.Reason = EArcadeMinigameEndReason::Submitted; Result.Score = -5;
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    TestEqual(TEXT("First negative score still owns cell"), A->OwnerSeat, 0);
    TestFalse(TEXT("Same game's other cell remains independent"), B->bHasRecord);
    Result.Seat = 1;
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    TestEqual(TEXT("Tie keeps owner"), A->OwnerSeat, 0);
    Result.Score = -4;
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    TestEqual(TEXT("Strict improvement captures"), A->OwnerSeat, 1);
    Result.Seat = 0; Result.Score = 999; Result.Outcome = EArcadeMinigameOutcome::Failed;
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    TestEqual(TEXT("Failure does not capture"), A->OwnerSeat, 1);
    Result.Outcome = EArcadeMinigameOutcome::Cancelled;
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    TestEqual(TEXT("Cancel preserves record"), A->BestValue, -4.0);
    Result.Outcome = EArcadeMinigameOutcome::Completed; Result.Score = std::numeric_limits<double>::infinity();
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    TestEqual(TEXT("Nonfinite result rejected"), A->BestValue, -4.0);
    Game->Metric = EArcadeRecordMetric::ElapsedSeconds; Game->Direction = EArcadeRecordDirection::LowerWins;
    FArcadeBoardCellTestAccess::Assign(A, Game);
    TestFalse(TEXT("New round clears record"), A->bHasRecord);
    Game->Direction = EArcadeRecordDirection::HigherWins; // editing asset cannot change active round
    Result.Score = 0; Result.ElapsedSeconds = 10;
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    Result.Seat = 1; Result.ElapsedSeconds = 8;
    FArcadeBoardCellTestAccess::Resolve(A, Result);
    TestEqual(TEXT("Elapsed metric and snapshotted lower-wins rule"), A->OwnerSeat, 1);
    TestEqual(TEXT("Elapsed stored"), A->BestValue, 8.0);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeRoundTest, "Arcade.Board.RoundLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeRoundTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Pool = NewObject<UArcadeMinigamePool>();
    auto* Game = NewObject<UArcadeMinigameDefinition>();
    Game->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Example.Example")));
    Pool->Games.Add(Game);
    for (int32 I = 0; I < 9; ++I) { auto* Cell = World->SpawnActor<AArcadeBoardCell>(); Cell->Pool = Pool; Cell->Row = I / 3; Cell->Column = I % 3; }
    auto* Round = World->SpawnActor<AArcadeBoardRound>();
    World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
    TestFalse(TEXT("World startup does not start match"), Round->bRoundReady);
    TestEqual(TEXT("World starts at menu"), Round->FlowState, EArcadeFlowState::MainMenu);
    TestNull(TEXT("World startup does not assign games"), Round->Cells[0]->AssignedGame.Get());
    TestTrue(TEXT("Hosts created without automatic play"), Round->Seat0Host && Round->Seat1Host && !Round->Seat0Host->bStartAutomatically);
    Round->Seat0Host->State = EArcadeInstanceState::Unloading;
    TestFalse(TEXT("New round cannot replace active allocation"), Round->StartNewRound(12));
    Round->Seat0Host->State = EArcadeInstanceState::Idle;
    TestTrue(TEXT("Idle board may start next round"), Round->StartNewRound(12));
    TestFalse(TEXT("Cannot silently reset a live match"), Round->StartNewRound(14));
    TestTrue(TEXT("Explicit return to menu accepted"), Round->ReturnToMainMenu());
    Round->Tick(0);
    TestEqual(TEXT("Cleanup reaches menu"), Round->FlowState, EArcadeFlowState::MainMenu);
    Round->Cells[0]->Pool = nullptr;
    TestFalse(TEXT("Invalid pool rejects entire new round"), Round->StartNewRound(13));
    TestEqual(TEXT("Failed request keeps round seed"), Round->RoundSeed, 12);
    TestTrue(TEXT("Failed request keeps current assignments"), Round->Cells[0]->AssignedGame == Game);
    TestFalse(TEXT("No arbitrary caller starts a challenge"), Round->TryStartChallenge(Round->Cells[1], nullptr));
    World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}
class FArcadeBoardRuntimeCommand : public IAutomationLatentCommand
{
public:
    explicit FArcadeBoardRuntimeCommand(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
        if (!World || FPlatformTime::Seconds() - Started > 45) { Test->AddError(TEXT("Board runtime timed out / no world")); return true; }
        AArcadeBoardRound* Round = nullptr;
        for (TActorIterator<AArcadeBoardRound> It(World); It; ++It) { Round = *It; break; }
        auto* Board = Cast<AArcadeBoardGameMode>(World->GetAuthGameMode());
        if (!Round || !Board) { Test->AddError(TEXT("Main map missing ready board")); return true; }
        auto* H0 = Round->Seat0Host.Get(); auto* H1 = Round->Seat1Host.Get();
        auto* Input = World->GetGameInstance()->GetSubsystem<UArcadeInputSubsystem>();
        auto* Cell = Round->Cells[4].Get();
        if (Phase == 0)
        {
            Test->TestEqual(TEXT("Main map begins at menu"), Round->FlowState, EArcadeFlowState::MainMenu);
            Test->TestNotNull(TEXT("Main menu routes P1 input to menu"), Cast<UArcadeMenuWidget>(Input->GetInputReceiver(0)));
            if (!Test->TestTrue(TEXT("UI explicitly starts match"), Round->StartNewRound(45))) { Test->AddError(Round->LastError); return true; }
            Cell = Round->GetCellAt(1,1);
            Test->TestEqual(TEXT("Nine cells in actual main map"), Round->Cells.Num(), 9);
            Test->TestTrue(TEXT("No automatic games"), H0->State == EArcadeInstanceState::Idle && H1->State == EArcadeInstanceState::Idle);
            for (int32 Seat = 0; Seat < 2; ++Seat)
            {
                auto* Character = Board->GetSeatCharacter(Seat);
                if (!Character) { Test->AddError(TEXT("Missing board character")); return true; }
                Character->SetActorLocation(Cell->GetActorLocation() + FVector(0, Seat ? 50 : -50, 90), false, nullptr, ETeleportType::TeleportPhysics);
                Character->GetCapsuleComponent()->UpdateOverlaps();
                Test->TestTrue(TEXT("Standing capsule overlaps selected cell"), Cell->ContainsCharacter(Character));
                IArcadeInputReceiver::Execute_ArcadeButton(Character, Seat, EArcadeInputAction::Confirm, true);
                Test->TestTrue(TEXT("Confirm reserves the selected cell"), Round->GetChallengedCell(Seat) == Cell);
            }
            Test->TestFalse(TEXT("Cannot reroll while players loading"), Round->StartNewRound(77));
            Phase = 1;
        }
        else if (Phase == 1 && H0->State == EArcadeInstanceState::Running && H1->State == EArcadeInstanceState::Running)
        {
            OldRun = H0->ActiveRun;
            Test->TestTrue(TEXT("Both real Blueprint games have presentations"), H0->Presentation && H1->Presentation);
            Test->TestTrue(TEXT("Real sample result accepted"), H0->ActiveRun->Finish(10));
            Test->TestEqual(TEXT("Result immediately owns originating cell"), Cell->OwnerSeat, 0);
            Test->TestTrue(TEXT("Seat zero returned while other game runs"), Input->GetInputReceiver(0) == Board->GetSeatCharacter(0) && H1->ActiveRun->State == EArcadeMinigameState::Running);
            FirstValue = Cell->BestValue;
            Test->TestTrue(TEXT("Second same-cell result accepted"), H1->ActiveRun->Finish(20));
            Test->TestEqual(TEXT("Cell compares two elapsed results"), Cell->BestValue, FMath::Min(FirstValue, H1->ActiveRun->Result.ElapsedSeconds));
            Test->TestFalse(TEXT("Different cell has no shared record"), Round->Cells[0]->bHasRecord);
            Phase = 2;
        }
        else if (Phase == 2 && H0->State == EArcadeInstanceState::Idle && H1->State == EArcadeInstanceState::Idle)
        {
            Test->TestTrue(TEXT("Both input routes restored"), Input->GetInputReceiver(0) == Board->GetSeatCharacter(0) && Input->GetInputReceiver(1) == Board->GetSeatCharacter(1));
            Test->TestTrue(TEXT("Menu request accepted"), Round->ReturnToMainMenu());
            Round->Tick(0);
            Test->TestTrue(TEXT("New round accepted after cleanup"), Round->StartNewRound(77));
            Test->TestFalse(TEXT("New round resets per-cell record"), Cell->bHasRecord);
            Test->TestFalse(TEXT("Late old result rejected"), OldRun.IsValid() && OldRun->Finish(999));
            auto* Character = Board->GetSeatCharacter(0);
            Character->SetActorLocation(FVector(1800,1800,150), false, nullptr, ETeleportType::TeleportPhysics);
            Character->GetCapsuleComponent()->UpdateOverlaps();
            Test->TestFalse(TEXT("Outside collision cannot start game"), Cell->TryInteract(Character));
            Character->SetActorLocation(Cell->GetActorLocation() + FVector(0,-50,90), false, nullptr, ETeleportType::TeleportPhysics);
            Character->GetCapsuleComponent()->UpdateOverlaps();
            IArcadeInputReceiver::Execute_ArcadeButton(Character, 0, EArcadeInputAction::Confirm, true);
            Test->TestTrue(TEXT("Player can enter another challenge"), Round->GetChallengedCell(0) == Cell);
            Phase = 3;
        }
        else if (Phase == 3 && H0->State == EArcadeInstanceState::Running)
        {
            Test->TestTrue(TEXT("Failed completion accepted by session"), H0->ActiveRun->Finish(500, false));
            Test->TestFalse(TEXT("Failure cannot claim unowned cell"), Cell->bHasRecord);
            Phase = 4;
        }
        else if (Phase == 4 && H0->State == EArcadeInstanceState::Idle)
        {
            Test->TestNull(TEXT("Failed run releases reservation"), Round->GetChallengedCell(0));
            FArcadeMinigameResult Prior;
            Prior.Seat = 0; Prior.Outcome = EArcadeMinigameOutcome::Completed; Prior.Reason = EArcadeMinigameEndReason::Submitted; Prior.ElapsedSeconds = 1;
            FArcadeBoardCellTestAccess::Resolve(Round->GetCellAt(0,0), Prior);
            FArcadeBoardCellTestAccess::Resolve(Round->GetCellAt(0,1), Prior);
            for (int32 Seat = 0; Seat < 2; ++Seat)
            {
                auto* Target = Seat == 0 ? Round->GetCellAt(0,2) : Round->GetCellAt(2,2);
                auto* Character = Board->GetSeatCharacter(Seat);
                Character->SetActorLocation(Target->GetActorLocation() + FVector(0,0,90), false, nullptr, ETeleportType::TeleportPhysics);
                Character->GetCapsuleComponent()->UpdateOverlaps();
                Test->TestTrue(TEXT("Start endgame challenges"), Target->TryInteract(Character));
            }
            Phase = 5;
        }
        else if (Phase == 5 && H0->State == EArcadeInstanceState::Running && H1->State == EArcadeInstanceState::Running)
        {
            OtherRun.Reset(H1->ActiveRun.Get());
            H0->ActiveRun->Finish(30);
            Test->TestEqual(TEXT("Three-in-row freezes winner"), Round->WinnerSeat, 0);
            Test->TestEqual(TEXT("Winner enters cleanup"), Round->FlowState, EArcadeFlowState::Ending);
            Test->TestFalse(TEXT("Cannot rematch before unload"), Round->StartNewRound());
            Test->TestEqual(TEXT("Three winning cells"), Round->WinningCells.Num(), 3);
            Phase = 6;
        }
        else if (Phase == 6 && Round->FlowState == EArcadeFlowState::Results)
        {
            Test->TestTrue(TEXT("Other game system-cancelled"), OtherRun.Get() && OtherRun->Result.Outcome == EArcadeMinigameOutcome::Cancelled);
            Test->TestFalse(TEXT("Cancellation cannot claim other cell"), Round->GetCellAt(2,2)->bHasRecord);
            Test->TestNotNull(TEXT("Results routes P1 input to menu"), Cast<UArcadeMenuWidget>(Input->GetInputReceiver(0)));
            Test->TestNotNull(TEXT("Results routes P2 input to menu"), Cast<UArcadeMenuWidget>(Input->GetInputReceiver(1)));
            Test->TestTrue(TEXT("Rematch from Results"), Round->StartNewRound(99));
            Test->TestEqual(TEXT("Rematch clears winner"), Round->WinnerSeat, INDEX_NONE);
            Test->TestFalse(TEXT("Rematch clears claims"), Round->GetCellAt(0,0)->bHasRecord);
            // Start then abandon while still loading: no late entry can revive the match.
            auto* Character = Board->GetSeatCharacter(0);
            auto* Target = Round->GetCellAt(1,1);
            Character->SetActorLocation(Target->GetActorLocation() + FVector(0,0,90), false, nullptr, ETeleportType::TeleportPhysics);
            Character->GetCapsuleComponent()->UpdateOverlaps();
            Test->TestTrue(TEXT("Load before menu return"), Target->TryInteract(Character));
            Test->TestTrue(TEXT("Return to menu during loading"), Round->ReturnToMainMenu());
            Phase = 7;
        }
        else if (Phase == 7 && Round->FlowState == EArcadeFlowState::MainMenu)
        {
            Test->TestNotNull(TEXT("Returning to menu restores menu input"), Cast<UArcadeMenuWidget>(Input->GetInputReceiver(0)));
            Test->TestEqual(TEXT("Loading host fully cleaned"), H0->State, EArcadeInstanceState::Idle);
            return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test;
    double Started;
    double FirstValue = 0;
    int32 Phase = 0;
    TWeakObjectPtr<UArcadeMinigameSession> OldRun;
    TStrongObjectPtr<UArcadeMinigameSession> OtherRun;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeBoardRuntimeTest, "Arcade.Board.CellRuntime", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FArcadeBoardRuntimeTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FArcadeBoardRuntimeCommand(this));
    return true;
}
struct FArcadeFlowTestAccess
{
    static bool Win(AArcadeBoardRound* Round) { return Round->EvaluateVictory(); }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeVictoryTest, "Arcade.Board.VictoryAndCoordinates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeVictoryTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* Pool = NewObject<UArcadeMinigamePool>();
    auto* Game = NewObject<UArcadeMinigameDefinition>();
    Game->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Example.Example")));
    Pool->Games.Add(Game);
    // Spawn reversed: matching coordinates must not depend on enumeration/name order.
    for (int32 I = 8; I >= 0; --I)
    { auto* C = World->SpawnActor<AArcadeBoardCell>(); C->Pool = Pool; C->Row = I / 3; C->Column = I % 3; }
    auto* Round = World->SpawnActor<AArcadeBoardRound>();
    World->InitializeActorsForPlay(FURL()); World->GetWorldSettings()->NotifyBeginPlay();
    static constexpr int32 Lines[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
    for (int32 Seat = 0; Seat < 2; ++Seat)
        for (const auto& Line : Lines)
        {
            TestTrue(TEXT("Explicit match start"), Round->StartNewRound(7));
            for (int32 Index : Line) Round->GetCellAt(Index / 3, Index % 3)->OwnerSeat = Seat;
            TestTrue(TEXT("Each horizontal/vertical/diagonal wins"), FArcadeFlowTestAccess::Win(Round));
            TestEqual(TEXT("Correct seat wins"), Round->WinnerSeat, Seat);
            TestFalse(TEXT("No duplicate win after state frozen"), FArcadeFlowTestAccess::Win(Round));
            Round->Tick(0);
            TestEqual(TEXT("Cleanup completes in Results"), Round->FlowState, EArcadeFlowState::Results);
        }
    TestTrue(TEXT("New match"), Round->StartNewRound());
    const int32 FullNoWin[9] = {0,1,0,0,1,1,1,0,0};
    for (int32 I = 0; I < 9; ++I) Round->GetCellAt(I/3,I%3)->OwnerSeat = FullNoWin[I];
    TestFalse(TEXT("Full board still permits stealing, no automatic draw"), FArcadeFlowTestAccess::Win(Round));
    Round->ReturnToMainMenu(); Round->Tick(0);
    Round->Cells[0]->Row = -1;
    TestFalse(TEXT("Unassigned coordinate rejected"), Round->StartNewRound());
    TestTrue(TEXT("Error identifies bad cell"), Round->LastError.Contains(Round->Cells[0]->GetName()));
    Round->Cells[0]->Row = 0; Round->Cells[0]->Column = 1;
    TestFalse(TEXT("Duplicate coordinate rejected"), Round->StartNewRound());
    Round->Cells[0]->Column = 0;
    TestTrue(TEXT("Corrected configuration can retry without reload"), Round->StartNewRound());
    World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}
#endif





