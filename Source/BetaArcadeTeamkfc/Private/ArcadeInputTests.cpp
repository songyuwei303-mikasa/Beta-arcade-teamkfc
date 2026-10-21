#include "ArcadeInputSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "ArcadeGamepadMath.h"
#include "ArcadeBoardCharacter.h"
#include "ArcadeBoardGameMode.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/Package.h"
#include "Components/BoxComponent.h"
#include "ArcadeMinigame.h"
#include "ArcadeMinigameTestGameMode.h"
#include "Camera/CameraActor.h"
#include "ArcadeMinigameInstanceHost.h"
#include "Engine/LevelStreamingDynamic.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FArcadeInputTestAccess
{
    static FArcadeInputState Read(UArcadeInputSubsystem* S, EArcadeInputSource Source) { return S->ReadSource(Source); }
    static void Pad(UArcadeInputSubsystem* S, int32 Index, bool Connected, bool Confirm = false)
    { S->Pads[Index].bConnected = Connected; S->Pads[Index].bConfirm = Confirm; }
    static void Process(UArcadeInputSubsystem* S, int32 Seat) { S->ProcessSeat(Seat, true); }
    static void Cancel(UArcadeInputSubsystem* S, int32 Seat) { S->CancelSeat(Seat); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeKeyboardTest, "Arcade.Input.KeyboardMappings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeKeyboardTest::RunTest(const FString&)
{
    auto* S = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    const auto K1 = EArcadeInputSource::Keyboard1, K2 = EArcadeInputSource::Keyboard2;
    TestFalse(TEXT("Mouse does not enter player inputs"), S->HandleKey(EKeys::LeftMouseButton, true));
    S->HandleKey(EKeys::W, true); S->HandleKey(EKeys::D, true);
    const auto Diagonal = FArcadeInputTestAccess::Read(S, K1).Direction;
    TestTrue(TEXT("Diagonal is normalized"), FMath::IsNearlyEqual(Diagonal.Size(), 1.0));
    TestTrue(TEXT("Up and right positive"), Diagonal.X > 0 && Diagonal.Y > 0);
    TestTrue(TEXT("Keyboard 2 is independent"), FArcadeInputTestAccess::Read(S, K2).Direction.IsNearlyZero());
    S->HandleKey(EKeys::A, true);
    TestEqual(TEXT("Opposite horizontal keys cancel"), FArcadeInputTestAccess::Read(S, K1).Direction.X, 0.0);
    S->HandleKey(EKeys::Enter, true); S->HandleKey(EKeys::NumPadZero, true);
    S->HandleKey(EKeys::Enter, false);
    TestTrue(TEXT("Second confirm key holds aggregate"), FArcadeInputTestAccess::Read(S, K2).bConfirm);
    S->HandleKey(EKeys::NumPadZero, false);
    TestFalse(TEXT("Confirm releases after both keys"), FArcadeInputTestAccess::Read(S, K2).bConfirm);
    S->HandleKey(EKeys::Escape, true); S->HandleKey(EKeys::Delete, true);
    TestTrue(TEXT("Escape exits K1"), FArcadeInputTestAccess::Read(S, K1).bExit);
    TestTrue(TEXT("Delete exits K2"), FArcadeInputTestAccess::Read(S, K2).bExit);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeSeatTest, "Arcade.Input.SeatIsolationAndReconnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeSeatTest::RunTest(const FString&)
{
    auto* S = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    FArcadeInputTestAccess::Pad(S, 1, true);
    const auto Sources = S->GetAvailableSources();
    TestEqual(TEXT("Connected pad precedes keyboards"), Sources[0], EArcadeInputSource::Gamepad2);
    TestEqual(TEXT("Keyboard 1 before keyboard 2"), Sources[1], EArcadeInputSource::Keyboard1);
    TestTrue(TEXT("Assign pad"), S->AssignSeat(0, Sources[0]));
    TestFalse(TEXT("Cannot share physical pad"), S->AssignSeat(1, Sources[0]));
    TestFalse(TEXT("Reject invalid seat"), S->AssignSeat(2, Sources[1]));
    TestFalse(TEXT("Reject nonreceiver object"), S->SetInputReceiver(0, S));
    FArcadeInputTestAccess::Process(S, 0); // neutral unlock
    FArcadeInputTestAccess::Pad(S, 1, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestTrue(TEXT("Confirm delivered"), S->GetSeatState(0).bConfirm);
    FArcadeInputTestAccess::Pad(S, 1, false);
    FArcadeInputTestAccess::Process(S, 0);
    TestFalse(TEXT("Disconnect cancels held confirm"), S->GetSeatState(0).bConfirm);
    TestEqual(TEXT("Disconnect reserves source"), S->GetSeatState(0).Source, EArcadeInputSource::Gamepad2);
    FArcadeInputTestAccess::Pad(S, 1, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestFalse(TEXT("Reconnect while held cannot auto confirm"), S->GetSeatState(0).bConfirm);
    FArcadeInputTestAccess::Pad(S, 1, true, false);
    FArcadeInputTestAccess::Process(S, 0);
    FArcadeInputTestAccess::Pad(S, 1, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestTrue(TEXT("Fresh press after neutral works"), S->GetSeatState(0).bConfirm);
    TestFalse(TEXT("Other seat unchanged"), S->GetSeatState(1).bConfirm);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeDPadTest, "Arcade.Input.DPadAndStick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeDPadTest::RunTest(const FString&)
{
    using namespace ArcadeGamepad;
    TestTrue(TEXT("Centered POV"), POV(0xffffffff).IsNearlyZero());
    TestTrue(TEXT("16-bit neutral POV"), POV(0xffff).IsNearlyZero());
    TestEqual(TEXT("POV up"), POV(0), FVector2D(0, 1));
    TestEqual(TEXT("POV right"), POV(9000), FVector2D(1, 0));
    TestEqual(TEXT("POV down"), POV(18000), FVector2D(0, -1));
    TestEqual(TEXT("POV left"), POV(27000), FVector2D(-1, 0));
    TestEqual(TEXT("D-pad wins over opposite stick"), Direction(FVector2D(-1, 0), POV(9000), 0.2f), FVector2D(1, 0));
    TestTrue(TEXT("Diagonal limited to unit length"), FMath::IsNearlyEqual(Direction(FVector2D(1, 0), POV(4500), 0.2f).Size(), 1.0));
    TestEqual(TEXT("Released D-pad returns stick"), Direction(FVector2D(0, 1), POV(0xffffffff), 0.2f), FVector2D(0, 1));
    TestTrue(TEXT("Stick drift ignored"), Direction(FVector2D(0.05, 0), FVector2D::ZeroVector, 0.2f).IsNearlyZero());
    auto* S = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    FArcadeInputTestAccess::Pad(S, 4, true);
    TestEqual(TEXT("Native PS before keyboard"), S->GetAvailableSources()[0], EArcadeInputSource::PlayStation1);
    TestTrue(TEXT("Native PS can own seat"), S->AssignSeat(0, EArcadeInputSource::PlayStation1));
    FArcadeInputTestAccess::Process(S, 0);
    FArcadeInputTestAccess::Pad(S, 4, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestTrue(TEXT("PS confirm delivered through same interface"), S->GetSeatState(0).bConfirm);
    FArcadeInputTestAccess::Pad(S, 4, false);
    FArcadeInputTestAccess::Process(S, 0);
    TestFalse(TEXT("PS disconnect cancels confirm"), S->GetSeatState(0).bConfirm);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeBoardRoutingTest, "Arcade.Input.BoardReceiverLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeBoardRoutingTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Floor = World->SpawnActor<AActor>();
    auto* FloorBox = NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(FloorBox);
    FloorBox->SetBoxExtent(FVector(2000, 2000, 50));
    FloorBox->SetCollisionProfileName(TEXT("BlockAll"));
    FloorBox->RegisterComponent();
    Floor->SetActorLocation(FVector(0, 0, -138));
    auto* P1 = World->SpawnActor<AArcadeBoardCharacter>();
    auto* P2 = World->SpawnActor<AArcadeBoardCharacter>(FVector(0, 300, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("P1 spawned"), P1) || !TestNotNull(TEXT("P2 spawned"), P2))
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        return false;
    }
    P1->InitializeSeat(0, 0);
    P2->InitializeSeat(1, 0);
    auto* S = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    S->AssignSeat(0, EArcadeInputSource::Keyboard1);
    S->AssignSeat(1, EArcadeInputSource::Keyboard2);
    TestTrue(TEXT("Bind P1 interface"), S->SetInputReceiver(0, P1));
    TestTrue(TEXT("Bind P2 interface"), S->SetInputReceiver(1, P2));
    FArcadeInputTestAccess::Process(S, 0);
    FArcadeInputTestAccess::Process(S, 1);
    S->HandleKey(EKeys::W, true);
    S->HandleKey(EKeys::Right, true);
    FArcadeInputTestAccess::Process(S, 0);
    FArcadeInputTestAccess::Process(S, 1);
    TestEqual(TEXT("P1 received up"), P1->GetMoveDirection(), FVector2D(0, 1));
    TestEqual(TEXT("P2 received right"), P2->GetMoveDirection(), FVector2D(1, 0));
    TestNull(TEXT("P1 does not need a controller"), P1->GetController());
    P1->Tick(1.f / 60.f);
    TestEqual(TEXT("Cached up feeds world X without possession"), P1->GetPendingMovementInputVector(), FVector(1, 0, 0));
    for (int32 Frame = 0; Frame < 60; ++Frame)
    {
        ++GFrameCounter; // Synchronous automation must advance the engine frame for tick scheduling.
        World->Tick(LEVELTICK_All, 1.f / 60.f);
    }
    AddInfo(FString::Printf(TEXT("Movement: P1=%s P2=%s Mode=%d Velocity=%s"), *P1->GetActorLocation().ToString(), *P2->GetActorLocation().ToString(), int32(P1->GetCharacterMovement()->MovementMode), *P1->GetVelocity().ToString()));
    TestTrue(TEXT("Unpossessed P1 actually walks forward"), P1->GetActorLocation().X > 100.f);
    TestTrue(TEXT("Unpossessed P2 actually walks right"), P2->GetActorLocation().Y > 400.f);
    TestTrue(TEXT("Floor collision supports P1"), FMath::Abs(P1->GetActorLocation().Z) < 10.f);
    P1->GetCharacterMovement()->Velocity = FVector(400, 0, 0);
    S->SetInputReceiver(0, nullptr); // Same handoff path as a menu.
    TestTrue(TEXT("Handoff clears cached movement"), P1->GetMoveDirection().IsNearlyZero());
    TestTrue(TEXT("Handoff clears pending movement"), P1->GetPendingMovementInputVector().IsNearlyZero());
    TestTrue(TEXT("Handoff stops velocity immediately"), P1->GetVelocity().IsNearlyZero());
    TestEqual(TEXT("Other seat keeps moving"), P2->GetMoveDirection(), FVector2D(1, 0));
    S->SetInputReceiver(0, P1);
    FArcadeInputTestAccess::Process(S, 0);
    TestTrue(TEXT("Held input cannot leak on return"), P1->GetMoveDirection().IsNearlyZero());
    S->HandleKey(EKeys::W, false);
    FArcadeInputTestAccess::Process(S, 0);
    S->HandleKey(EKeys::W, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestEqual(TEXT("Fresh input after neutral resumes"), P1->GetMoveDirection(), FVector2D(0, 1));
    FArcadeInputTestAccess::Cancel(S, 0);
    TestTrue(TEXT("Focus/disconnect cancellation stops character"), P1->GetMoveDirection().IsNearlyZero());
    IArcadeInputReceiver::Execute_ArcadeDirection(P1, 1, FVector2D(1, 1));
    TestTrue(TEXT("Wrong seat rejected"), P1->GetMoveDirection().IsNearlyZero());
    S->SetInputReceiver(0, nullptr);
    S->SetInputReceiver(1, nullptr);
    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

struct FArcadeMinigameTestAccess
{
    static UArcadeMinigameSession* Prepare(UArcadeMinigameSubsystem* Manager, int32 Seat, AArcadeMinigameEntry* Entry, UArcadeInputSubsystem* Input)
    { return Manager->PrepareWithInput(Seat, Entry, Input); }
    static void StartPrepared(AArcadeMinigameTestGameMode* Mode, UArcadeMinigameSession* Run)
    {
        Mode->TestSession = Run;
        Mode->bEnablePresentation = false;
        Mode->TryStartPreparedSession();
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeMinigameTestHostTest, "Arcade.Minigame.StandaloneHost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeMinigameTestHostTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Host = World->SpawnActor<AArcadeMinigameTestGameMode>();
    AArcadeMinigameEntry* FoundEntry = nullptr;
    ACameraActor* FoundCamera = nullptr;
    TestFalse(TEXT("Empty test map rejected"), Host->FindTestActors(FoundEntry, FoundCamera));
    auto* Entry = World->SpawnActor<AArcadeMinigameEntry>();
    auto* Camera = World->SpawnActor<ACameraActor>();
    Camera->Tags.Add(TEXT("ArcadeMinigameCamera"));
    TestTrue(TEXT("Unique entry/camera found"), Host->FindTestActors(FoundEntry, FoundCamera));
    TestTrue(TEXT("Exact entry returned"), FoundEntry == Entry);
    auto* Duplicate = World->SpawnActor<AArcadeMinigameEntry>();
    TestFalse(TEXT("Ambiguous entries rejected"), Host->FindTestActors(FoundEntry, FoundCamera));
    Duplicate->Destroy();
    TestNull(TEXT("No default player pawn"), Host->DefaultPawnClass.Get());
    TestTrue(TEXT("Controller does not inherit demo"), Host->PlayerControllerClass == AArcadeBoardController::StaticClass());
    auto* Input = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    auto* Run = FArcadeMinigameTestAccess::Prepare(World->GetSubsystem<UArcadeMinigameSubsystem>(), 0, Entry, Input);
    if (TestNotNull(TEXT("Existing session API used"), Run))
    {
        FArcadeMinigameTestAccess::StartPrepared(Host, Run);
        TestEqual(TEXT("Host starts ready session"), Run->State, EArcadeMinigameState::Running);
        TestTrue(TEXT("Session retains input"), Input->GetInputReceiver(0) == Run);
        IArcadeInputReceiver::Execute_ArcadeButton(Run, 0, EArcadeInputAction::Exit, true);
        TestEqual(TEXT("Exit does not end gameplay"), Run->State, EArcadeMinigameState::Running);
        TestTrue(TEXT("Normal finish still works"), Run->Finish(1.25));
        FArcadeMinigameTestAccess::StartPrepared(Host, Run);
        TestEqual(TEXT("Host does not restart ended run"), Run->State, EArcadeMinigameState::Ended);
    }
    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeMinigameSessionTest, "Arcade.Minigame.SessionLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeMinigameSessionTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Manager = World->GetSubsystem<UArcadeMinigameSubsystem>();
    auto* Input = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    auto* Board0 = World->SpawnActor<AArcadeBoardCharacter>();
    auto* Board1 = World->SpawnActor<AArcadeBoardCharacter>(FVector(0, 300, 0), FRotator::ZeroRotator);
    auto* Entry0 = World->SpawnActor<AArcadeMinigameEntry>();
    auto* Entry1 = World->SpawnActor<AArcadeMinigameEntry>();
    Board0->InitializeSeat(0, 0);
    Board1->InitializeSeat(1, 0);
    Input->AssignSeat(0, EArcadeInputSource::Keyboard1);
    Input->AssignSeat(1, EArcadeInputSource::Keyboard2);
    Input->SetInputReceiver(0, Board0);
    Input->SetInputReceiver(1, Board1);
    FArcadeInputTestAccess::Process(Input, 0);
    FArcadeInputTestAccess::Process(Input, 1);
    Input->HandleKey(EKeys::W, true);
    Input->HandleKey(EKeys::Right, true);
    FArcadeInputTestAccess::Process(Input, 0);
    FArcadeInputTestAccess::Process(Input, 1);

    auto Prepare = [&](int32 Seat, AArcadeMinigameEntry* Entry)
    { return FArcadeMinigameTestAccess::Prepare(Manager, Seat, Entry, Input); };
    TestNull(TEXT("Invalid seat rejected"), Prepare(2, Entry0));
    auto* Run0 = Prepare(0, Entry0);
    if (!TestNotNull(TEXT("Create session"), Run0)) return false;
    TestEqual(TEXT("Default preparation marks ready"), Run0->State, EArcadeMinigameState::Ready);
    TestTrue(TEXT("Board cancelled during preparation"), Board0->GetMoveDirection().IsNearlyZero());
    TestEqual(TEXT("Other board remains independent"), Board1->GetMoveDirection(), FVector2D(1, 0));
    TestNull(TEXT("Seat cannot reserve a second session"), Prepare(0, Entry1));
    TestNull(TEXT("Entry cannot be shared concurrently"), Prepare(1, Entry0));
    TestFalse(TEXT("Cannot finish before Start"), Run0->Finish(100));
    TestTrue(TEXT("Ready starts"), Run0->Start());
    TestFalse(TEXT("Cannot start twice"), Run0->Start());
    FArcadeInputTestAccess::Process(Input, 0);
    TestTrue(TEXT("Entry press/held movement cannot leak"), Run0->Direction.IsNearlyZero());
    Input->HandleKey(EKeys::W, false);
    FArcadeInputTestAccess::Process(Input, 0);
    Input->HandleKey(EKeys::W, true);
    Input->HandleKey(EKeys::SpaceBar, true);
    FArcadeInputTestAccess::Process(Input, 0);
    TestEqual(TEXT("Run receives direction"), Run0->Direction, FVector2D(0, 1));
    TestTrue(TEXT("Run receives confirm"), Run0->bConfirmHeld);
    FArcadeInputTestAccess::Cancel(Input, 0);
    TestTrue(TEXT("Focus/disconnect clears cached direction"), Run0->Direction.IsNearlyZero());
    TestFalse(TEXT("Focus/disconnect clears charge"), Run0->bConfirmHeld);
    TestEqual(TEXT("Input interruption is not game cancellation"), Run0->State, EArcadeMinigameState::Running);
    auto* Run1 = Prepare(1, Entry1);
    TestTrue(TEXT("Second independent run starts"), Run1 && Run1->Start());
    TestTrue(TEXT("Distinct run identity"), Run1 && Run1->SessionId != Run0->SessionId);
    auto* Temporary = World->SpawnActor<AActor>();
    TestTrue(TEXT("Register owned runtime actor"), Run0->RegisterSpawnedActor(Temporary));
    TestFalse(TEXT("Other run cannot own same actor"), Run1->RegisterSpawnedActor(Temporary));
    TestFalse(TEXT("Never register board for destruction"), Run0->RegisterSpawnedActor(Board0));
    TestTrue(TEXT("Complete run"), Run0->Finish(42));
    TestEqual(TEXT("Result score"), Run0->Result.Score, 42.0);
    TestEqual(TEXT("Result seat"), Run0->Result.Seat, 0);
    TestEqual(TEXT("Result completed"), Run0->Result.Outcome, EArcadeMinigameOutcome::Completed);
    TestTrue(TEXT("Runtime actor cleaned"), !IsValid(Temporary) || Temporary->IsActorBeingDestroyed());
    TestTrue(TEXT("Restore original board receiver"), Input->GetInputReceiver(0) == Board0);
    TestEqual(TEXT("Other run remains running"), Run1->State, EArcadeMinigameState::Running);
    TestFalse(TEXT("Duplicate submission rejected"), Run0->Finish(999));
    FArcadeInputTestAccess::Process(Input, 0);
    TestTrue(TEXT("Held return input is gated"), Board0->GetMoveDirection().IsNearlyZero());
    Input->HandleKey(EKeys::W, false);
    Input->HandleKey(EKeys::SpaceBar, false);
    FArcadeInputTestAccess::Process(Input, 0);
    Input->HandleKey(EKeys::W, true);
    FArcadeInputTestAccess::Process(Input, 0);
    TestEqual(TEXT("Fresh board movement resumes"), Board0->GetMoveDirection(), FVector2D(0, 1));
    auto* NewRun0 = Prepare(0, Entry0);
    TestTrue(TEXT("Entry can be reused"), NewRun0 && NewRun0->Start());
    TestFalse(TEXT("Old async callback cannot cancel reused entry"), Run0->Cancel());
    TestTrue(TEXT("New run still current"), Manager->GetSession(0) == NewRun0);
    // Another receiver now owns the seat (e.g. a menu). Ending must not steal it.
    Input->SetInputReceiver(0, Board0);
    TestTrue(TEXT("Cancel after another owner took input"), NewRun0->Cancel());
    TestTrue(TEXT("Newer receiver retained"), Input->GetInputReceiver(0) == Board0);
    Entry1->Destroy();
    TestEqual(TEXT("Destroyed entry cancels run"), Run1->State, EArcadeMinigameState::Ended);
    TestEqual(TEXT("Destroyed reason recorded"), Run1->Result.Reason, EArcadeMinigameEndReason::EntryDestroyed);
    TestTrue(TEXT("Destroyed entry returns input"), Input->GetInputReceiver(1) == Board1);

    auto* Preparing = Prepare(0, Entry0);
    TestTrue(TEXT("Cancel before running"), Preparing && Preparing->Cancel());
    TestEqual(TEXT("No preparation time counted"), Preparing->Result.ElapsedSeconds, 0.0);
    auto* ShutdownRun = Prepare(0, Entry0);
    World->EndPlay(EEndPlayReason::Quit);
    TestEqual(TEXT("World teardown ends run"), ShutdownRun->State, EArcadeMinigameState::Ended);
    TestNull(TEXT("World teardown does not restore old actor"), Input->GetInputReceiver(0));
    TestNull(TEXT("Shutdown refuses new sessions"), Prepare(0, Entry0));
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

struct FArcadeInstanceTestAccess
{
    static bool Start(AArcadeMinigameInstanceHost* Host, UArcadeInputSubsystem* Input)
    { Host->bEnablePresentation = false; return Host->StartWithInput(Input); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeInstanceTest, "Arcade.Minigame.DynamicInstances", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeInstanceTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Input = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    auto* Board0 = World->SpawnActor<AArcadeBoardCharacter>();
    auto* Board1 = World->SpawnActor<AArcadeBoardCharacter>(FVector(0, 300, 0), FRotator::ZeroRotator);
    Input->SetInputReceiver(0, Board0);
    Input->SetInputReceiver(1, Board1);
    auto* Host0 = World->SpawnActor<AArcadeMinigameInstanceHost>();
    auto* Host1 = World->SpawnActor<AArcadeMinigameInstanceHost>();
    Host1->Seat = 1;
    const FSoftObjectPath Map(TEXT("/Game/Jiashu/MiniGames/TestMiniGame_WalkToGoal/L_WalkToGoalMiniGame.L_WalkToGoalMiniGame"));
    Host0->MinigameMap = TSoftObjectPtr<UWorld>(Map);
    Host1->MinigameMap = TSoftObjectPtr<UWorld>(Map);
    Host1->InstanceLocation = FVector(100000, 20000, 0);
    Host0->Seat = 2;
    TestFalse(TEXT("Invalid seat refused before loading"), FArcadeInstanceTestAccess::Start(Host0, Input));
    TestTrue(TEXT("Invalid request leaves board input alone"), Input->GetInputReceiver(0) == Board0);
    Host0->Seat = 0;
    TestTrue(TEXT("First asynchronous load accepted"), FArcadeInstanceTestAccess::Start(Host0, Input));
    TestTrue(TEXT("Loading suspends only its seat"), Input->GetInputReceiver(0) == Host0 && Input->GetInputReceiver(1) == Board1);
    TestFalse(TEXT("Duplicate request rejected"), FArcadeInstanceTestAccess::Start(Host0, Input));
    auto* Rival = World->SpawnActor<AArcadeMinigameInstanceHost>();
    Rival->MinigameMap = TSoftObjectPtr<UWorld>(Map);
    TestFalse(TEXT("Another host cannot reserve loading seat"), FArcadeInstanceTestAccess::Start(Rival, Input));
    TestTrue(TEXT("Same map loads for second seat"), FArcadeInstanceTestAccess::Start(Host1, Input));
    World->FlushLevelStreaming();
    ULevel* Level0 = Host0->StreamingLevel ? Host0->StreamingLevel->GetLoadedLevel() : nullptr;
    ULevel* Level1 = Host1->StreamingLevel ? Host1->StreamingLevel->GetLoadedLevel() : nullptr;
    if (TestNotNull(TEXT("First real map loaded"), Level0) && TestNotNull(TEXT("Second real map loaded"), Level1))
    {
        TestTrue(TEXT("Distinct level objects"), Level0 != Level1);
        for (AActor* Actor0 : Level0->Actors)
            if (auto* Entry0 = Cast<AArcadeMinigameEntry>(Actor0))
                for (AActor* Actor1 : Level1->Actors)
                    if (auto* Entry1 = Cast<AArcadeMinigameEntry>(Actor1))
                        TestTrue(TEXT("Map placement applies independent translation"),
                            (Entry1->GetActorLocation() - Entry0->GetActorLocation()).Equals(FVector(0, 20000, 0), 0.1));
        // Verify the user's serialized Entry actor references are remapped into each copy.
        // Replace gameplay entries in memory afterwards so host tests do not execute user rules/UI.
        int32 LocalReferences = 0;
        for (ULevel* Level : { Level0, Level1 })
        {
            const auto Actors = Level->Actors;
            int32 EntryCount = 0;
            for (AActor* Actor : Actors)
                if (auto* Entry = Cast<AArcadeMinigameEntry>(Actor))
                {
                    ++EntryCount;
                    for (TFieldIterator<FObjectProperty> It(Entry->GetClass()); It; ++It)
                        if (auto* Reference = Cast<AActor>(It->GetObjectPropertyValue_InContainer(Entry)))
                        {
                            TestTrue(TEXT("Entry reference belongs to this instance"), Reference->GetLevel() == Level);
                            ++LocalReferences;
                        }
                    Entry->Destroy();
                }
            TestEqual(TEXT("Sample contains one entry"), EntryCount, 1);
            FActorSpawnParameters Params;
            Params.OverrideLevel = Level;
            World->SpawnActor<AArcadeMinigameEntry>(AArcadeMinigameEntry::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        }
        TestTrue(TEXT("Sample local actor references inspected"), LocalReferences >= 2);
        Host0->Tick(0);
        Host1->Tick(0);
        TestEqual(TEXT("First host running"), Host0->State, EArcadeInstanceState::Running);
        TestEqual(TEXT("Second host running"), Host1->State, EArcadeInstanceState::Running);
        if (Host0->ActiveRun && Host1->ActiveRun)
        {
            auto* OldRun = Host0->ActiveRun.Get();
            TestTrue(TEXT("Submit first result"), OldRun->Finish(3.5));
            Host0->Tick(0);
            TestTrue(TEXT("Board input restored"), Input->GetInputReceiver(0) == Board0);
            TestEqual(TEXT("Other instance continues"), Host1->ActiveRun->State, EArcadeMinigameState::Running);
            World->FlushLevelStreaming();
            Host0->Tick(0);
            TestEqual(TEXT("Unloaded host reusable"), Host0->State, EArcadeInstanceState::Idle);
            TestTrue(TEXT("Other level stays loaded"), Host1->StreamingLevel->GetLoadedLevel() == Level1);
            TestFalse(TEXT("Late result rejected"), OldRun->Finish(100));
            TestTrue(TEXT("Restart accepted after unload"), FArcadeInstanceTestAccess::Start(Host0, Input));
            // Destroy during asynchronous loading; no late callback should start a run.
            Host0->Destroy();
            TestTrue(TEXT("Destroy while loading returns input"), Input->GetInputReceiver(0) == Board0);
            Input->SetInputReceiver(1, Board0); // A later menu-like receiver owns the seat.
            Host1->ActiveRun->Finish(7);
            Host1->Tick(0);
            TestTrue(TEXT("Later receiver preserved"), Input->GetInputReceiver(1) == Board0);
            World->FlushLevelStreaming();
            Host1->Tick(0);
            TestEqual(TEXT("Second instance unloaded"), Host1->State, EArcadeInstanceState::Idle);
        }
    }
    World->EndPlay(EEndPlayReason::Quit);
    World->FlushLevelStreaming();
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeMapAuditTest, "Arcade.Board.MapAudit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeMapAuditTest::RunTest(const FString&)
{
    for (const TCHAR* Path : {TEXT("/Game/Jiashu/Levels/L_Main"), TEXT("/Game/Jiashu/Levels/L_TestingLevel")})
    {
        UPackage* Package = LoadPackage(nullptr, Path, LOAD_None);
        UWorld* World = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
        if (!TestNotNull(Path, World)) continue;
        AddInfo(FString::Printf(TEXT("%s GameModeOverride=%s"), Path, *GetPathNameSafe(World->GetWorldSettings()->DefaultGameMode)));
        for (const auto& Actor : World->PersistentLevel->Actors)
            if (Actor) AddInfo(FString::Printf(TEXT("  %s (%s)"), *Actor->GetName(), *Actor->GetClass()->GetName()));
    }
    return true;
}
#endif
