#include "ArcadeMinigameTestGameMode.h"
#include "ArcadeMinigame.h"
#include "ArcadeBoardGameMode.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
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
    int32 EntryCount = 0, CameraCount = 0;
    for (TActorIterator<AArcadeMinigameEntry> It(GetWorld()); It; ++It)
        if (It->GetLevel() == GetWorld()->PersistentLevel) { OutEntry = *It; ++EntryCount; }
    for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
        if (It->GetLevel() == GetWorld()->PersistentLevel && It->ActorHasTag(TEXT("ArcadeMinigameCamera")))
        { OutCamera = *It; ++CameraCount; }
    return EntryCount == 1 && CameraCount == 1;
}

void AArcadeMinigameTestGameMode::StartPlay()
{
    Super::StartPlay(); // All placed actors must finish BeginPlay before OnPrepare.
    if (TestSession) return;
    AArcadeMinigameEntry* Entry = nullptr;
    ACameraActor* Camera = nullptr;
    if (!FindTestActors(Entry, Camera))
    {
        UE_LOG(LogArcadeMinigameTest, Error, TEXT("Test map needs exactly one MinigameEntry and one CameraActor tagged ArcadeMinigameCamera in its persistent level."));
        return;
    }
    auto* GI = GetGameInstance();
    auto* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
    auto* Sessions = GetWorld()->GetSubsystem<UArcadeMinigameSubsystem>();
    if (!PC || !Sessions) return;
    PC->SetViewTarget(Camera); // Test host only. Streamed minigames never run this GameMode.
    TestEntry = Entry;
    TestSession = Sessions->PrepareSession(TestSeat, Entry);
    if (!TestSession)
    {
        UE_LOG(LogArcadeMinigameTest, Error, TEXT("Could not prepare test session. Check TestSeat and entry availability."));
        return;
    }
    if (TestSession->State == EArcadeMinigameState::Ended) return;
    if (Entry->MinigameWidgetClass)
    {
        TestWidget = CreateWidget<UUserWidget>(PC, Entry->MinigameWidgetClass);
        if (!TestWidget)
        {
            TestSession->Cancel();
            UE_LOG(LogArcadeMinigameTest, Error, TEXT("Could not create test UI."));
            return;
        }
        Entry->SetPresentationWidget(TestWidget);
        TestWidget->AddToViewport();
    }
    PreparedAt = GetWorld()->GetTimeSeconds();
    SetActorTickEnabled(true);
    TryStartPreparedSession();
}

void AArcadeMinigameTestGameMode::TryStartPreparedSession()
{
    if (!TestSession) { SetActorTickEnabled(false); return; }
    if (TestSession->State == EArcadeMinigameState::Ready)
    {
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
    if (TestSession->State != EArcadeMinigameState::Preparing) SetActorTickEnabled(false);
}

void AArcadeMinigameTestGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TryStartPreparedSession();
}

void AArcadeMinigameTestGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (TestSession) TestSession->Cancel();
    if (TestWidget) TestWidget->RemoveFromParent();
    if (IsValid(TestEntry) && TestEntry->GetMinigameWidget() == TestWidget) TestEntry->SetPresentationWidget(nullptr);
    Super::EndPlay(EndPlayReason);
}
