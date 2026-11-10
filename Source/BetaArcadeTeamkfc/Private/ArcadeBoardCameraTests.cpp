#include "ArcadeBoardGameMode.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FArcadeBoardCameraTestAccess
{
    static void Bind(AArcadeBoardGameMode* Mode, ACameraActor* Camera)
    {
        Mode->BoardCamera = Camera;
        Mode->CameraCenter = Camera->GetActorLocation();
        Mode->CameraScreenRight = Camera->GetActorRightVector();
        Mode->CameraPanOffset = 0.f;
    }
    static void Update(AArcadeBoardGameMode* Mode, bool Left, bool Right, float Dt)
    { Mode->UpdateBoardCamera(Left, Right, Dt); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeBoardCameraPanTest, "Arcade.Board.CameraPan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeBoardCameraPanTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);

    auto* Mode = World->SpawnActor<AArcadeBoardGameMode>();
    auto* Camera = World->SpawnActor<ACameraActor>();
    const FVector Center(100, 200, 900);
    const FRotator Rotation(-60, 75, 0);
    Camera->SetActorLocationAndRotation(Center, Rotation);
    FArcadeBoardCameraTestAccess::Bind(Mode, Camera);
    const FVector Right = Camera->GetActorRightVector();
    Mode->MinigameCameraPanDistance = 400.f;
    Mode->MinigameCameraPanSpeed = 0.f;
    FArcadeBoardCameraTestAccess::Update(Mode, true, false, .1f);
    TestTrue(TEXT("Left window moves camera screen-left"), Camera->GetActorLocation().Equals(Center - Right * 400, .01));
    FArcadeBoardCameraTestAccess::Update(Mode, false, true, .1f);
    TestTrue(TEXT("Right window moves camera screen-right"), Camera->GetActorLocation().Equals(Center + Right * 400, .01));
    FArcadeBoardCameraTestAccess::Update(Mode, true, true, .1f);
    TestTrue(TEXT("Both windows recenter"), Camera->GetActorLocation().Equals(Center, .01));
    FArcadeBoardCameraTestAccess::Update(Mode, true, false, .1f);
    FArcadeBoardCameraTestAccess::Update(Mode, false, false, .1f);
    TestTrue(TEXT("No windows recenter"), Camera->GetActorLocation().Equals(Center, .01));
    Mode->MinigameCameraPanSpeed = 4.f;
    FArcadeBoardCameraTestAccess::Update(Mode, true, false, .1f);
    const double Offset = FVector::DotProduct(Camera->GetActorLocation() - Center, Right);
    TestTrue(TEXT("Smooth move is partial, with no overshoot"), Offset < 0 && Offset > -400);
    const FVector OneStep = Camera->GetActorLocation();
    Camera->SetActorLocation(Center);
    FArcadeBoardCameraTestAccess::Bind(Mode, Camera);
    FArcadeBoardCameraTestAccess::Update(Mode, true, false, .05f);
    FArcadeBoardCameraTestAccess::Update(Mode, true, false, .05f);
    TestTrue(TEXT("Smoothing is frame-rate independent"), Camera->GetActorLocation().Equals(OneStep, .01));
    Mode->bEnableMinigameCameraPan = false;
    Mode->MinigameCameraPanSpeed = 0.f;
    FArcadeBoardCameraTestAccess::Update(Mode, true, false, .1f);
    TestTrue(TEXT("Disabling returns to original center"), Camera->GetActorLocation().Equals(Center, .01));
    TestTrue(TEXT("Rotation is unchanged"), Camera->GetActorRotation().Equals(Rotation, .01));
    World->DestroyWorld(false);
    return true;
}
#endif
