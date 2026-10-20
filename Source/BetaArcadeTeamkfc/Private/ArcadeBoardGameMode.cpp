#include "ArcadeBoardGameMode.h"
#include "ArcadeBoardCharacter.h"
#include "ArcadeInputSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcadeBoard, Log, All);

AArcadeBoardController::AArcadeBoardController()
{
    bAutoManageActiveCameraTarget = false;
}

void AArcadeBoardController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
}

AArcadeBoardGameMode::AArcadeBoardGameMode()
{
    PlayerControllerClass = AArcadeBoardController::StaticClass();
    DefaultPawnClass = nullptr;
    BoardCharacterClass = AArcadeBoardCharacter::StaticClass();
    DefaultCameraTransform = FTransform(FRotator(-60, 0, 0), FVector(-1000, 0, 1732));
}

void AArcadeBoardGameMode::StartPlay()
{
    // BeginPlay first, then install the formal camera and receivers exactly once.
    Super::StartPlay();
    if (SeatCharacters.Num() != 0) return;
    ACameraActor* Camera = nullptr;
    for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
    {
        if (!It->ActorHasTag(TEXT("ArcadeBoardCamera"))) continue;
        if (Camera)
        {
            UE_LOG(LogArcadeBoard, Error, TEXT("Use exactly one camera tagged ArcadeBoardCamera."));
            return;
        }
        Camera = *It;
    }
    if (!Camera)
    {
        Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), DefaultCameraTransform);
        if (Camera) Camera->GetCameraComponent()->SetFieldOfView(CameraFOV);
    }
    if (!Camera) return;
    if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController()) PC->SetViewTarget(Camera);

    // Tagged PlayerStarts make the seat mapping explicit and independent of enumeration order.
    APlayerStart* Starts[2] = {};
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        for (int32 Seat = 0; Seat < 2; ++Seat)
        {
            const FName Tag = Seat == 0 ? FName(TEXT("Seat0")) : FName(TEXT("Seat1"));
            if (It->PlayerStartTag != Tag) continue;
            if (Starts[Seat])
            {
                UE_LOG(LogArcadeBoard, Error, TEXT("Duplicate Player Start Tag %s."), *Tag.ToString());
                return;
            }
            Starts[Seat] = *It;
        }
    }
    if (!Starts[0] || !Starts[1] || !BoardCharacterClass)
    {
        UE_LOG(LogArcadeBoard, Error, TEXT("Board requires PlayerStarts with Player Start Tag Seat0 and Seat1, and BoardCharacterClass."));
        return;
    }
    SeatCharacters.SetNum(2);
    for (int32 Seat = 0; Seat < 2; ++Seat)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        auto* Character = GetWorld()->SpawnActor<AArcadeBoardCharacter>(BoardCharacterClass, Starts[Seat]->GetActorTransform(), Params);
        SeatCharacters[Seat] = Character;
        if (!Character)
        {
            UE_LOG(LogArcadeBoard, Error, TEXT("Could not spawn seat %d. Check PlayerStart collision/clearance."), Seat);
            for (const auto& Spawned : SeatCharacters) if (IsValid(Spawned)) Spawned->Destroy();
            SeatCharacters.Reset();
            return;
        }
        Character->InitializeSeat(Seat, Camera->GetCameraComponent()->GetComponentRotation().Yaw);
    }
    for (int32 Seat = 0; Seat < 2; ++Seat) RestoreBoardInput(Seat);
}

AArcadeBoardCharacter* AArcadeBoardGameMode::GetSeatCharacter(int32 Seat) const
{
    return SeatCharacters.IsValidIndex(Seat) && IsValid(SeatCharacters[Seat]) ? SeatCharacters[Seat].Get() : nullptr;
}

bool AArcadeBoardGameMode::RestoreBoardInput(int32 Seat)
{
    auto* Character = GetSeatCharacter(Seat);
    auto* GI = GetGameInstance();
    auto* Input = GI ? GI->GetSubsystem<UArcadeInputSubsystem>() : nullptr;
    return Character && Input && Input->SetInputReceiver(Seat, Character);
}

void AArcadeBoardGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* GI = GetGameInstance())
        if (auto* Input = GI->GetSubsystem<UArcadeInputSubsystem>())
            for (int32 Seat = 0; Seat < SeatCharacters.Num(); ++Seat)
                if (SeatCharacters[Seat] && Input->GetInputReceiver(Seat) == SeatCharacters[Seat]) Input->SetInputReceiver(Seat, nullptr);
    Super::EndPlay(EndPlayReason);
}
