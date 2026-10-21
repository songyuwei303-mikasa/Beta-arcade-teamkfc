#include "ArcadeMinigame.h"
#include "ArcadeInputSubsystem.h"
#include "ArcadeBoardGameMode.h"
#include "ArcadeBoardCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Engine/Level.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcadeMinigame, Log, All);

AArcadeMinigameEntry::AArcadeMinigameEntry()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AArcadeMinigameEntry::OnPrepare_Implementation(UArcadeMinigameSession* Run)
{
    Run->MarkReady();
}

void AArcadeMinigameEntry::SetPresentationWidget(UUserWidget* Widget)
{
    PresentationWidget = Widget;
}

UCameraComponent* AArcadeMinigameEntry::GetMinigameCamera_Implementation() const
{
    AActor* Source = CameraSourceActor;
    if (!Source)
    {
        for (AActor* Actor : GetLevel()->Actors)
            if (IsValid(Actor) && Actor->ActorHasTag(TEXT("ArcadeMinigameCamera")))
            {
                if (Source) return nullptr; // Ambiguous tags are configuration errors.
                Source = Actor;
            }
    }
    if (!IsValid(Source) || Source->GetLevel() != GetLevel()) return nullptr;
    TInlineComponentArray<UCameraComponent*> Cameras(Source);
    UCameraComponent* Selected = nullptr;
    for (auto* Camera : Cameras)
        if (CameraComponentTag.IsNone() ? Camera->IsActive() : Camera->ComponentHasTag(CameraComponentTag))
        {
            if (Selected) return nullptr;
            Selected = Camera;
        }
    return Selected;
}

void AArcadeMinigameEntry::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    if (Session)
        Session->End(EArcadeMinigameOutcome::Cancelled,
            GetWorld()->bIsTearingDown ? EArcadeMinigameEndReason::WorldEnded : EArcadeMinigameEndReason::EntryDestroyed, 0);
    Super::EndPlay(EndPlayReason);
}

bool UArcadeMinigameSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UArcadeMinigameSession* UArcadeMinigameSubsystem::GetSession(int32 Seat) const
{
    return Sessions.IsValidIndex(Seat) ? Sessions[Seat].Get() : nullptr;
}

UArcadeMinigameSession* UArcadeMinigameSubsystem::PrepareSession(int32 Seat, AArcadeMinigameEntry* Entry)
{
    auto* GI = GetWorld()->GetGameInstance();
    return PrepareWithInput(Seat, Entry, GI ? GI->GetSubsystem<UArcadeInputSubsystem>() : nullptr);
}

UArcadeMinigameSession* UArcadeMinigameSubsystem::PrepareWithInput(int32 Seat, AArcadeMinigameEntry* Entry, UArcadeInputSubsystem* Input,
    TOptional<TWeakObjectPtr<UObject>> OriginalReceiver)
{
    if (bShuttingDown || GetWorld()->bIsTearingDown || Seat < 0 || Seat > 1 || !IsValid(Input) ||
        !IsValid(Entry) || Entry->bEndingPlay || !Entry->HasActorBegunPlay() || Entry->GetWorld() != GetWorld() ||
        Entry->Session || GetSession(Seat))
        return nullptr;

    auto* Run = NewObject<UArcadeMinigameSession>(this);
    Run->SessionId = FGuid::NewGuid();
    Run->Seat = Seat;
    Run->Manager = this;
    Run->Entry = Entry;
    Run->Input = Input;
    Run->ReturnReceiver = OriginalReceiver.IsSet() ? OriginalReceiver.GetValue() : TWeakObjectPtr<UObject>(Input->GetInputReceiver(Seat));
    Sessions.SetNum(2);
    Sessions[Seat] = Run;
    Entry->Session = Run;

    // Route through a session gate even during preparation, so the board stops immediately.
    // State-changing calls from the old receiver's cancellation callback are rejected.
    Run->bChangingRoute = true;
    Input->SetInputReceiver(Seat, Run);
    Run->bChangingRoute = false;
    if (!IsValid(Entry) || Entry->bEndingPlay || bShuttingDown || GetWorld()->bIsTearingDown)
    {
        Run->End(EArcadeMinigameOutcome::Cancelled, EArcadeMinigameEndReason::EntryDestroyed, 0);
        return Run;
    }
    Entry->OnPrepare(Run);
    return Run;
}

bool UArcadeMinigameSession::IsCurrent() const
{
    return Manager.IsValid() && Manager->GetSession(Seat) == this &&
        State != EArcadeMinigameState::Ending && State != EArcadeMinigameState::Ended;
}

bool UArcadeMinigameSession::MarkReady()
{
    if (!IsCurrent() || bChangingRoute || State != EArcadeMinigameState::Preparing || !Entry.IsValid()) return false;
    State = EArcadeMinigameState::Ready;
    return true;
}

bool UArcadeMinigameSession::Start()
{
    if (!IsCurrent() || bChangingRoute || State != EArcadeMinigameState::Ready || !Entry.IsValid() ||
        !Input.IsValid() || Input->GetInputReceiver(Seat) != this) return false;
    // New gate instance is not needed, but SetInputReceiver(same object) is a no-op.
    // Detach/reattach deliberately to require neutral again after preparation.
    bChangingRoute = true;
    Input->SetInputReceiver(Seat, nullptr);
    Input->SetInputReceiver(Seat, this);
    bChangingRoute = false;
    if (!Entry.IsValid() || Entry->bEndingPlay || Manager->bShuttingDown || Manager->GetWorld()->bIsTearingDown)
    {
        End(EArcadeMinigameOutcome::Cancelled, EArcadeMinigameEndReason::EntryDestroyed, 0);
        return false;
    }
    StartSeconds = Manager->GetWorld()->GetTimeSeconds();
    bStarted = true;
    State = EArcadeMinigameState::Running;
    Entry->OnStarted(this);
    return true;
}

double UArcadeMinigameSession::GetElapsedSeconds() const
{
    if (State == EArcadeMinigameState::Ended || State == EArcadeMinigameState::Ending) return Result.ElapsedSeconds;
    return bStarted && Manager.IsValid() ? FMath::Max(0.0, Manager->GetWorld()->GetTimeSeconds() - StartSeconds) : 0.0;
}

bool UArcadeMinigameSession::Finish(double Score, bool bSucceeded)
{
    if (State != EArcadeMinigameState::Running || !FMath::IsFinite(Score)) return false;
    return End(bSucceeded ? EArcadeMinigameOutcome::Completed : EArcadeMinigameOutcome::Failed,
        EArcadeMinigameEndReason::Submitted, Score);
}

bool UArcadeMinigameSession::Cancel()
{
    return End(EArcadeMinigameOutcome::Cancelled, EArcadeMinigameEndReason::Requested, 0);
}

bool UArcadeMinigameSession::RegisterSpawnedActor(AActor* Actor)
{
    if (!IsCurrent() || !Entry.IsValid() || !IsValid(Actor) || Actor == Entry.Get() ||
        Actor->GetLevel() != Entry->GetLevel() || Actor->IsA<AArcadeBoardCharacter>() ||
        Actor->IsA<AArcadeMinigameEntry>()) return false;
    // Do not let two sessions claim the same temporary actor.
    for (const auto& Other : Manager->Sessions)
        if (Other && Other.Get() != this && Other->SpawnedActors.Contains(Actor)) return false;
    SpawnedActors.AddUnique(Actor);
    return true;
}

void UArcadeMinigameSession::ArcadeDirection_Implementation(int32 InSeat, FVector2D Value)
{
    if (!IsCurrent() || State != EArcadeMinigameState::Running || InSeat != Seat || !Entry.IsValid()) return;
    Direction = Value.ContainsNaN() ? FVector2D::ZeroVector : Value.GetClampedToMaxSize(1.0);
    Entry->OnDirection(this, Direction);
}

void UArcadeMinigameSession::ArcadeButton_Implementation(int32 InSeat, EArcadeInputAction Action, bool bPressed)
{
    if (!IsCurrent() || State != EArcadeMinigameState::Running || InSeat != Seat || !Entry.IsValid()) return;
    if (Action == EArcadeInputAction::Confirm) bConfirmHeld = bPressed;
    // Gameplay has no voluntary exit. Entries ignore Exit; menus may use it for back navigation.
    Entry->OnButton(this, Action, bPressed);
}

void UArcadeMinigameSession::ArcadeInputCancelled_Implementation(int32 InSeat)
{
    if (InSeat != Seat) return;
    Direction = FVector2D::ZeroVector;
    bConfirmHeld = false;
    if (State == EArcadeMinigameState::Running && Entry.IsValid()) Entry->OnInputCancelled(this);
}

bool UArcadeMinigameSession::End(EArcadeMinigameOutcome Outcome, EArcadeMinigameEndReason Reason, double Score)
{
    if (!IsCurrent() || bChangingRoute) return false;
    Result.SessionId = SessionId;
    Result.Seat = Seat;
    Result.Outcome = Outcome;
    Result.Reason = Reason;
    Result.Score = Score;
    Result.ElapsedSeconds = GetElapsedSeconds();
    State = EArcadeMinigameState::Ending; // Reject reentrant / repeated submissions before any callbacks.
    Direction = FVector2D::ZeroVector;
    bConfirmHeld = false;
    if (Entry.IsValid())
    {
        Entry->OnInputCancelled(this);
        if (Entry.IsValid()) Entry->OnStopped(this, Result);
        if (Entry.IsValid()) Entry->OnCleanup(this);
    }
    for (const auto& Actor : SpawnedActors) if (Actor.IsValid()) Actor->Destroy();
    SpawnedActors.Reset();

    auto* Owner = Manager.Get();
    if (Input.IsValid() && Input->GetInputReceiver(Seat) == this)
    {
        const bool bRestore = !Owner->bShuttingDown && !Owner->GetWorld()->bIsTearingDown &&
            Reason != EArcadeMinigameEndReason::WorldEnded;
        auto* Board = Cast<AArcadeBoardGameMode>(Owner->GetWorld()->GetAuthGameMode());
        // Only restore the original board character, never seize a later menu's input.
        if (!(bRestore && Board && Board->GetSeatCharacter(Seat) &&
            ReturnReceiver.Get() == Board->GetSeatCharacter(Seat) && Board->RestoreBoardInput(Seat)))
        {
            UObject* Target = bRestore ? ReturnReceiver.Get() : nullptr;
            if (auto* Actor = Cast<AActor>(Target); Actor && (Actor->IsActorBeingDestroyed() || Actor->GetWorld() != Owner->GetWorld()))
                Target = nullptr;
            Input->SetInputReceiver(Seat, Target);
        }
    }
    if (Entry.IsValid() && Entry->Session == this) Entry->Session = nullptr;
    State = EArcadeMinigameState::Ended;
    Owner->Sessions[Seat] = nullptr;
    UE_LOG(LogArcadeMinigame, Log, TEXT("Session %s seat %d ended: outcome=%d reason=%d score=%.3f seconds=%.3f"),
        *SessionId.ToString(), Seat, int32(Outcome), int32(Reason), Score, Result.ElapsedSeconds);
    Owner->OnSessionEnded.Broadcast(Result);
    return true;
}

void UArcadeMinigameSubsystem::Shutdown()
{
    bShuttingDown = true;
    const auto Active = Sessions;
    for (const auto& Run : Active)
        if (Run) Run->End(EArcadeMinigameOutcome::Cancelled, EArcadeMinigameEndReason::WorldEnded, 0);
}

void UArcadeMinigameSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
    Shutdown();
    Super::OnWorldEndPlay(InWorld);
}

void UArcadeMinigameSubsystem::Deinitialize()
{
    Shutdown();
    Super::Deinitialize();
}
