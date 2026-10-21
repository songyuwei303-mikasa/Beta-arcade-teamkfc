#include "ArcadeMinigameTestGameMode.h"
#include "ArcadeMinigame.h"
#include "ArcadeBoardGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcadeMinigameTest, Log, All);

AArcadeMinigameTestGameMode::AArcadeMinigameTestGameMode()
{
    PlayerControllerClass = AArcadeBoardController::StaticClass();
    DefaultPawnClass = nullptr;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

bool AArcadeMinigameTestGameMode::FindTestActors(AArcadeMinigameEntry*& OutEntry, ACameraActor*& OutCamera) const
{
    OutEntry = nullptr;
    OutCamera = nullptr;
    int32 EntryCount = 0;
    for (TActorIterator<AArcadeMinigameEntry> It(GetWorld()); It; ++It)
        if (It->GetLevel() == GetWorld()->PersistentLevel) { OutEntry = *It; ++EntryCount; }
    auto* Camera = EntryCount == 1 ? OutEntry->GetMinigameCamera() : nullptr;
    OutCamera = Camera ? Cast<ACameraActor>(Camera->GetOwner()) : nullptr;
    return Camera != nullptr;
}

void AArcadeMinigameTestGameMode::StartPlay()
{
    Super::StartPlay(); // All placed actors must finish BeginPlay before OnPrepare.
    if (TestSession) return;
    AArcadeMinigameEntry* Entry = nullptr;
    int32 EntryCount = 0;
    for (TActorIterator<AArcadeMinigameEntry> It(GetWorld()); It; ++It)
        if (It->GetLevel() == GetWorld()->PersistentLevel) { Entry = *It; ++EntryCount; }
    if (EntryCount != 1)
    {
        UE_LOG(LogArcadeMinigameTest, Error, TEXT("Test map needs exactly one MinigameEntry in its persistent level."));
        return;
    }
    auto* GI = GetGameInstance();
    auto* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
    auto* Sessions = GetWorld()->GetSubsystem<UArcadeMinigameSubsystem>();
    if (!PC || !Sessions) return;
    TestEntry = Entry;
    TestSession = Sessions->PrepareSession(TestSeat, Entry);
    if (!TestSession)
    {
        UE_LOG(LogArcadeMinigameTest, Error, TEXT("Could not prepare test session. Check TestSeat and entry availability."));
        return;
    }
    if (TestSession->State == EArcadeMinigameState::Ended) return;
    PreparedAt = GetWorld()->GetTimeSeconds();
    SetActorTickEnabled(true);
    TryStartPreparedSession();
}

void AArcadeMinigameTestGameMode::TryStartPreparedSession()
{
    if (!TestSession) { SetActorTickEnabled(false); return; }
    if (TestSession->State == EArcadeMinigameState::Ended)
    {
        if (IsValid(Presentation))
        {
            if (bKeepResultVisible) Presentation->Freeze();
            else
            {
                Presentation->BeginClose(TestSession->Result);
                if (!Presentation->IsClosed()) return;
                Presentation->Destroy();
                Presentation = nullptr;
            }
        }
        SetActorTickEnabled(false);
        return;
    }
    if (Presentation && (!IsValid(Presentation) || Presentation->HasFailed()))
    {
        TestSession->Cancel();
        UE_LOG(LogArcadeMinigameTest, Error, TEXT("Presentation lost its instance camera."));
        return;
    }
    if (TestSession->State == EArcadeMinigameState::Ready)
    {
        if (bEnablePresentation)
        {
            if (!Presentation)
            {
                Presentation = GetWorld()->SpawnActor<AArcadeMinigamePresentation>();
                if (!Presentation || !Presentation->Initialize(TestEntry, TestSeat, PresentationSettings, true))
                {
                    TestSession->Cancel();
                    if (IsValid(Presentation)) Presentation->Destroy();
                    Presentation = nullptr;
                    UE_LOG(LogArcadeMinigameTest, Error, TEXT("Presentation setup failed. Check camera, frame widget names and resolution aspect ratio."));
                    return;
                }
            }
            if (!Presentation->IsReady())
            {
                if (GetWorld()->GetTimeSeconds() - PreparedAt >= PreparationTimeout)
                {
                    TestSession->Cancel();
                    UE_LOG(LogArcadeMinigameTest, Error, TEXT("Opening timed out; frame must call FinishOpening."));
                }
                return;
            }
        }
        if (!TestSession->Start())
        {
            TestSession->Cancel();
            UE_LOG(LogArcadeMinigameTest, Error, TEXT("Test start failed: the session no longer owns input."));
        }
    }
    else if (TestSession->State == EArcadeMinigameState::Preparing &&
        GetWorld()->GetTimeSeconds() - PreparedAt >= PreparationTimeout)
    {
        TestSession->Cancel();
        UE_LOG(LogArcadeMinigameTest, Error, TEXT("Preparation timed out. OnPrepare must call Run.MarkReady after initialization."));
    }
}

void AArcadeMinigameTestGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TryStartPreparedSession();
}

void AArcadeMinigameTestGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (TestSession) TestSession->Cancel();
    if (IsValid(Presentation)) Presentation->Destroy();
    Presentation = nullptr;
    Super::EndPlay(EndPlayReason);
}
