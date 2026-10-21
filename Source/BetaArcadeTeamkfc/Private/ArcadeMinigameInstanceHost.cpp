#include "ArcadeMinigameInstanceHost.h"
#include "ArcadeInputSubsystem.h"
#include "ArcadeBoardGameMode.h"
#include "ArcadeBoardCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcadeInstance, Log, All);

AArcadeMinigameInstanceHost::AArcadeMinigameInstanceHost()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool AArcadeMinigameInstanceHost::StartMinigame()
{
    auto* GI = GetGameInstance();
    return StartWithInput(GI ? GI->GetSubsystem<UArcadeInputSubsystem>() : nullptr);
}

bool AArcadeMinigameInstanceHost::StartWithInput(UArcadeInputSubsystem* Router)
{
    if (bEndingPlay || !HasActorBegunPlay() || GetWorld()->bIsTearingDown || State != EArcadeInstanceState::Idle)
        return false;
    LastError.Reset();
    auto* Sessions = GetWorld()->GetSubsystem<UArcadeMinigameSubsystem>();
    if (Seat < 0 || Seat > 1 || MinigameMap.IsNull() || !Router || !Sessions ||
        Sessions->GetSession(Seat) || InstanceLocation.ContainsNaN())
    {
        LastError = TEXT("Invalid seat/map/input, or seat already has a session.");
        OnLoadFailed.Broadcast(LastError);
        return false;
    }
    for (TActorIterator<AArcadeMinigameInstanceHost> It(GetWorld()); It; ++It)
        if (*It != this && It->State != EArcadeInstanceState::Idle && It->ActiveSeat == Seat)
        {
            LastError = TEXT("Seat already has a loading/running/unloading instance.");
            OnLoadFailed.Broadcast(LastError);
            return false;
        }
    ActiveSeat = Seat;
    Input = Router;
    PreviousReceiver = Router->GetInputReceiver(ActiveSeat);
    State = EArcadeInstanceState::Loading;
    PhaseStarted = FPlatformTime::Seconds();
    Router->SetInputReceiver(ActiveSeat, this);
    if (bEndingPlay || GetWorld()->bIsTearingDown) return false;
    bool bAccepted = false;
    StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
        this, MinigameMap, InstanceLocation, FRotator::ZeroRotator, bAccepted);
    if (!bAccepted || !StreamingLevel)
    {
        Fail(TEXT("Level instance request failed. Check map asset and packaging settings."));
        return false;
    }
    UE_LOG(LogArcadeInstance, Log, TEXT("Seat %d loading %s at %s"), ActiveSeat,
        *MinigameMap.ToSoftObjectPath().ToString(), *InstanceLocation.ToString());
    return true;
}

void AArcadeMinigameInstanceHost::PrepareLoadedLevel()
{
    ULevel* Level = StreamingLevel->GetLoadedLevel();
    int32 Count = 0;
    for (AActor* Actor : Level->Actors)
        if (auto* Entry = Cast<AArcadeMinigameEntry>(Actor); IsValid(Entry))
        {
            ActiveEntry = Entry;
            ++Count;
        }
    if (Count != 1 || !ActiveEntry->HasActorBegunPlay())
    {
        Fail(TEXT("Loaded instance needs exactly one begun-play ArcadeMinigameEntry."));
        return;
    }
    if (!Input.IsValid() || Input->GetInputReceiver(ActiveSeat) != this)
    {
        Fail(TEXT("Input ownership changed while loading; the new receiver was preserved."));
        return;
    }
    State = EArcadeInstanceState::Preparing;
    PhaseStarted = FPlatformTime::Seconds();
    ActiveRun = GetWorld()->GetSubsystem<UArcadeMinigameSubsystem>()->PrepareWithInput(
        ActiveSeat, ActiveEntry, Input.Get(), PreviousReceiver);
    if (bEndingPlay) { if (ActiveRun) ActiveRun->Cancel(); RestoreInput(); return; }
    if (!ActiveRun) { Fail(TEXT("Could not prepare loaded entry.")); return; }
    if (ActiveRun->State == EArcadeMinigameState::Ended) { BeginUnload(); return; }
    UE_LOG(LogArcadeInstance, Log, TEXT("Seat %d prepared entry %s"), ActiveSeat, *ActiveEntry->GetPathName());
}

void AArcadeMinigameInstanceHost::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFirstTick)
    {
        bFirstTick = false;
        if (bStartAutomatically && State == EArcadeInstanceState::Idle) StartMinigame();
    }
    if (bEndingPlay) return;
    if (State == EArcadeInstanceState::Closing)
    {
        if (!IsValid(Presentation) || Presentation->IsClosed()) BeginUnload();
        return;
    }
    if (State == EArcadeInstanceState::Unloading)
    {
        if (!StreamingLevel || !GetWorld()->GetStreamingLevels().Contains(StreamingLevel))
        {
            StreamingLevel = nullptr;
            ActiveEntry = nullptr;
            ActiveRun = nullptr;
            State = EArcadeInstanceState::Idle;
            UE_LOG(LogArcadeInstance, Log, TEXT("Seat %d instance unloaded; ready for reuse."), ActiveSeat);
            ActiveSeat = INDEX_NONE;
        }
        return;
    }
    if (State == EArcadeInstanceState::Loading)
    {
        if (StreamingLevel && StreamingLevel->IsLevelVisible() && StreamingLevel->GetLoadedLevel())
            PrepareLoadedLevel();
        else if (FPlatformTime::Seconds() - PhaseStarted > FMath::Max(1.f, TimeoutSeconds))
            Fail(TEXT("Level loading timed out."));
    }
    if (State == EArcadeInstanceState::Preparing || State == EArcadeInstanceState::Running)
    {
        if (!ActiveRun) { Fail(TEXT("Missing session.")); return; }
        if (ActiveRun->State == EArcadeMinigameState::Ended)
        {
            if (IsValid(Presentation))
            {
                State = EArcadeInstanceState::Closing;
                Presentation->BeginClose(ActiveRun->Result);
                if (Presentation->IsClosed()) BeginUnload();
            }
            else BeginUnload();
            return;
        }
        if (Presentation && (!IsValid(Presentation) || Presentation->HasFailed()))
        { Fail(TEXT("Presentation lost its instance camera.")); return; }
        if (State == EArcadeInstanceState::Preparing)
        {
            if (ActiveRun->State == EArcadeMinigameState::Ready)
            {
                if (bEnablePresentation)
                {
                    if (!Presentation)
                    {
                        Presentation = GetWorld()->SpawnActor<AArcadeMinigamePresentation>();
                        if (!Presentation || !Presentation->Initialize(ActiveEntry, ActiveSeat, PresentationSettings))
                        { Fail(TEXT("Presentation setup failed. Check instance camera, frame widget names and resolution aspect ratio.")); return; }
                    }
                    if (!Presentation->IsReady())
                    {
                        if (FPlatformTime::Seconds() - PhaseStarted > FMath::Max(1.f, TimeoutSeconds))
                            Fail(TEXT("Opening timed out; frame must call FinishOpening."));
                        return;
                    }
                }
                State = EArcadeInstanceState::Running;
                if (!ActiveRun->Start()) Fail(TEXT("Session start failed."));
            }
            else if (FPlatformTime::Seconds() - PhaseStarted > FMath::Max(1.f, TimeoutSeconds))
                Fail(TEXT("Preparation timed out; entry must call MarkReady."));
        }
    }
}

void AArcadeMinigameInstanceHost::RestoreInput()
{
    if (!Input.IsValid() || Input->GetInputReceiver(ActiveSeat) != this) return;
    UObject* Target = PreviousReceiver.Get();
    if (GetWorld()->bIsTearingDown) Target = nullptr;
    if (auto* Actor = Cast<AActor>(Target); Actor && (Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld()))
        Target = nullptr;
    auto* Board = Cast<AArcadeBoardGameMode>(GetWorld()->GetAuthGameMode());
    if (Target && Board && Board->GetSeatCharacter(ActiveSeat) == Target && Board->RestoreBoardInput(ActiveSeat)) return;
    Input->SetInputReceiver(ActiveSeat, Target);
}

void AArcadeMinigameInstanceHost::BeginUnload()
{
    if (State == EArcadeInstanceState::Unloading || State == EArcadeInstanceState::Idle) return;
    State = EArcadeInstanceState::Unloading;
    if (ActiveRun && ActiveRun->State != EArcadeMinigameState::Ended) ActiveRun->Cancel();
    RestoreInput();
    if (IsValid(ActiveEntry)) ActiveEntry->SetPresentationWidget(nullptr);
    if (IsValid(Presentation)) Presentation->Destroy();
    Presentation = nullptr;
    if (StreamingLevel)
    {
        StreamingLevel->SetShouldBeVisible(false);
        StreamingLevel->SetShouldBeLoaded(false);
        StreamingLevel->SetIsRequestingUnloadAndRemoval(true);
    }
    if (ActiveRun && !bEndingPlay) OnInstanceEnded.Broadcast(ActiveRun->Result);
}

void AArcadeMinigameInstanceHost::Fail(const FString& Message)
{
    LastError = Message;
    UE_LOG(LogArcadeInstance, Warning, TEXT("Seat %d: %s"), ActiveSeat, *Message);
    BeginUnload();
    if (!bEndingPlay) OnLoadFailed.Broadcast(Message);
}

void AArcadeMinigameInstanceHost::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    BeginUnload();
    Super::EndPlay(EndPlayReason);
}
void AArcadeMinigameInstanceHost::StopForMatchEnd()
{
    bStartAutomatically = false;
    BeginUnload();
}
