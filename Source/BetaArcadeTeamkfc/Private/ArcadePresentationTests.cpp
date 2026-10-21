#include "ArcadeMinigamePresentation.h"
#include "ArcadeMinigameInstanceHost.h"
#include "ArcadeMinigameTestGameMode.h"
#include "ArcadeInputSubsystem.h"
#include "ArcadeBoardGameMode.h"
#include "ArcadeBoardRound.h"
#include "ArcadeBoardCharacter.h"
#include "Engine/GameInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "UnrealClient.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FArcadePresentationTestAccess
{
    static void Bind(AArcadeMinigamePresentation* Presenter, AArcadeMinigameEntry* Entry)
    { Presenter->Entry = Entry; }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeCameraPresentationTest, "Arcade.Minigame.CameraAndView", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeCameraPresentationTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    auto* Entry = World->SpawnActor<AArcadeMinigameEntry>();
    auto* Camera = World->SpawnActor<ACameraActor>();
    Camera->Tags.Add(TEXT("ArcadeMinigameCamera"));
    TestTrue(TEXT("Legacy CameraActor resolved"), Entry->GetMinigameCamera() == Camera->GetCameraComponent());
    auto* Duplicate = World->SpawnActor<ACameraActor>();
    Duplicate->Tags.Add(TEXT("ArcadeMinigameCamera"));
    TestNull(TEXT("Ambiguous fallback rejected"), Entry->GetMinigameCamera());
    Entry->CameraSourceActor = Camera;
    TestTrue(TEXT("Explicit source overrides tag search"), Entry->GetMinigameCamera() == Camera->GetCameraComponent());
    auto* CameraPawn = World->SpawnActor<AArcadeMinigameEntry>();
    auto* FirstPerson = NewObject<UCameraComponent>(CameraPawn);
    FirstPerson->RegisterComponent();
    FirstPerson->ComponentTags.Add(TEXT("FirstPerson"));
    auto* ThirdPerson = NewObject<UCameraComponent>(CameraPawn);
    ThirdPerson->RegisterComponent();
    ThirdPerson->ComponentTags.Add(TEXT("ThirdPerson"));
    Entry->CameraSourceActor = CameraPawn;
    TestNull(TEXT("Two active components require selection"), Entry->GetMinigameCamera());
    Entry->CameraComponentTag = TEXT("FirstPerson");
    TestTrue(TEXT("Select camera inside actor"), Entry->GetMinigameCamera() == FirstPerson);
    auto* Presenter = World->SpawnActor<AArcadeMinigamePresentation>();
    TestEqual(TEXT("UI receives tone-mapped linear color"), Presenter->Capture->CaptureSource.GetValue(), SCS_FinalToneCurveHDR);
    FArcadePresentationTestAccess::Bind(Presenter, Entry);
    FirstPerson->SetWorldLocation(FVector(500, 200, 90));
    FirstPerson->SetFieldOfView(75);
    TestTrue(TEXT("Camera sync succeeds"), Presenter->SyncCamera(0));
    TestTrue(TEXT("Source transform copied"), Presenter->Capture->GetComponentLocation().Equals(FirstPerson->GetComponentLocation()));
    TestEqual(TEXT("FOV copied"), Presenter->Capture->FOVAngle, 75.f);
    TestTrue(TEXT("Owner visibility uses source actor"), Presenter->Capture->GetViewOwner() == CameraPawn);
    Entry->CameraComponentTag = TEXT("ThirdPerson");
    ThirdPerson->SetProjectionMode(ECameraProjectionMode::Orthographic);
    ThirdPerson->SetOrthoWidth(800);
    TestTrue(TEXT("Runtime camera switching supported"), Presenter->SyncCamera(0));
    TestEqual(TEXT("Orthographic mode copied"), Presenter->Capture->ProjectionType.GetValue(), ECameraProjectionMode::Orthographic);
    TestEqual(TEXT("Ortho width copied"), Presenter->Capture->OrthoWidth, 800.f);
    TestTrue(TEXT("Instance actors enter show-only list"), Presenter->Capture->ShowOnlyActors.Contains(CameraPawn));
    auto* ForeignLevel = NewObject<ULevel>(World);
    ForeignLevel->OwningWorld = World;
    FActorSpawnParameters Spawn;
    Spawn.OverrideLevel = ForeignLevel;
    auto* ForeignCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    Entry->CameraSourceActor = ForeignCamera;
    Entry->CameraComponentTag = NAME_None;
    TestNull(TEXT("Cross-instance camera rejected"), Entry->GetMinigameCamera());
    TestFalse(TEXT("Capture refuses foreign camera"), Presenter->SyncCamera(0));
    TestFalse(TEXT("Other instance excluded from render list"), Presenter->Capture->ShowOnlyActors.Contains(ForeignCamera));
    auto* Frame = NewObject<UArcadeMinigameView>(World);
    Frame->Initialize();
    auto* UI = NewObject<UArcadeMinigameView>(World);
    UI->Initialize();
    auto* Texture = NewObject<UTextureRenderTarget2D>(World);
    TestTrue(TEXT("Native layout built without designer assets"), Frame->Configure(Texture, UI, 1, FVector2D(1920, 1080)));
    TestNotNull(TEXT("Gameplay UI is nested in frame"), UI->GetParent());
    TestEqual(TEXT("Frame seat assigned"), Frame->Seat, 1);
    Frame->Open();
    TestTrue(TEXT("Default opening completes"), Frame->IsOpeningFinished());
    Frame->Close(FArcadeMinigameResult());
    TestTrue(TEXT("Default closing completes"), Frame->IsClosingFinished());
    ForeignCamera->Destroy();
    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

/** Opt-in rendered smoke test: launch -game L_Main; the test explicitly starts its hosts. */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FArcadeRenderedFinish, FAutomationTestBase*, Test);
bool FArcadeRenderedFinish::Update()
{
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Game)
            for (TActorIterator<AArcadeMinigameInstanceHost> It(Context.World()); It; ++It)
                if (It->Seat == 0) Test->TestTrue(TEXT("Rendered run accepts completion"), It->ActiveRun && It->ActiveRun->Finish(2.5));
    return true;
}
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FArcadeRenderedCleanup, FAutomationTestBase*, Test);
bool FArcadeRenderedCleanup::Update()
{
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Game)
        {
            UWorld* World = Context.World();
            for (TActorIterator<AArcadeMinigameInstanceHost> It(World); It; ++It)
                if (It->Seat == 0)
                {
                    Test->TestEqual(TEXT("Finished rendered instance unloaded"), It->State, EArcadeInstanceState::Idle);
                    Test->TestNull(TEXT("Presentation released"), It->Presentation.Get());
                    auto* Board = Cast<AArcadeBoardGameMode>(World->GetAuthGameMode());
                    auto* Input = World->GetGameInstance()->GetSubsystem<UArcadeInputSubsystem>();
                    Test->TestTrue(TEXT("Rendered run restores board input"), Board && Input->GetInputReceiver(0) == Board->GetSeatCharacter(0));
                }
                else Test->TestTrue(TEXT("Other rendered instance continues"), It->State == EArcadeInstanceState::Running && It->Presentation && It->Presentation->Frame->IsInViewport());
        }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadePresentationRuntimeTest, "Arcade.Minigame.PresentationRuntime", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FArcadePresentationRuntimeTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("Running game world"), World)) return false;
    TArray<AArcadeMinigameInstanceHost*> Hosts;
    for (TActorIterator<AArcadeMinigameInstanceHost> It(World); It; ++It) Hosts.Add(*It);
    TestEqual(TEXT("Two configured L_Main hosts"), Hosts.Num(), 2);
    if (Hosts.Num() != 2) return false;
    // Formal boards no longer auto-start. Keep this isolated presentation regression runnable.
    if (Parameters.IsEmpty())
    {
        for (TActorIterator<AArcadeBoardRound> It(World); It; ++It) TestTrue(TEXT("Explicit match start"), It->StartNewRound());
        for (auto* Host : Hosts)
            if (Host->State == EArcadeInstanceState::Idle) TestTrue(TEXT("Explicit presentation test start"), Host->StartMinigame());
        ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(3.f));
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this]() { RunTest(TEXT("VerifyStartedHosts")); return true; }));
        return true;
    }
    UTextureRenderTarget2D* FirstRT = nullptr;
    TOptional<FVector2D> FirstPosition;
    for (auto* Host : Hosts)
    {
        TestEqual(TEXT("Instance running with presentation"), Host->State, EArcadeInstanceState::Running);
        auto* P = Host->Presentation.Get();
        if (!TestNotNull(TEXT("Presentation exists"), P)) continue;
        if (!TestNotNull(TEXT("RenderTarget exists"), P->RenderTarget.Get())) continue;
        TestEqual(TEXT("Linear floating-point capture target"), P->RenderTarget->GetFormat(), PF_FloatRGBA);
        TestFalse(TEXT("No implicit sRGB texture conversion"), P->RenderTarget->IsSRGB());
        TestEqual(TEXT("Capture output gamma is linear"), P->RenderTarget->GetDisplayGamma(), 1.f);
        TestTrue(TEXT("Frame visible in viewport"), P->Frame && P->Frame->IsInViewport());
        if (P->Frame)
        {
            const FVector2D Position = P->Frame->GetCachedGeometry().GetAbsolutePosition();
            if (FirstPosition.IsSet()) TestTrue(TEXT("Seat windows occupy different screen positions"), FMath::Abs(Position.X - FirstPosition->X) > 10);
            FirstPosition = Position;
        }
        TestTrue(TEXT("Sample HUD nested"), P->GameUI && P->GameUI->GetParent());
        if (FirstRT) TestTrue(TEXT("Each seat owns a separate RT"), FirstRT != P->RenderTarget);
        FirstRT = P->RenderTarget;
        TArray<FColor> Pixels;
        const bool Read = P->RenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        TestTrue(TEXT("GPU pixels read"), Read && Pixels.Num() > 0);
        if (Read && Pixels.Num())
        {
            bool NonUniform = false;
            uint8 MaxAlpha = 0;
            for (const auto& Pixel : Pixels)
            {
                NonUniform |= Pixel.R != Pixels[0].R || Pixel.G != Pixels[0].G || Pixel.B != Pixels[0].B;
                MaxAlpha = FMath::Max(MaxAlpha, Pixel.A);
            }
            TestTrue(TEXT("Capture contains scene detail"), NonUniform);
            TestTrue(TEXT("Texture not entirely transparent"), MaxAlpha > 0);
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(P->RenderTarget->SizeX, P->RenderTarget->SizeY, Pixels, PNG);
            FFileHelper::SaveArrayToFile(PNG, *FString::Printf(TEXT("%sScreenshots/MinigameSeat%d.png"), *FPaths::ProjectSavedDir(), Host->Seat));
        }
        auto* PC = World->GetFirstPlayerController();
        TestTrue(TEXT("Main camera not stolen"), PC && PC->GetViewTarget() && PC->GetViewTarget()->GetLevel() == World->PersistentLevel);
    }
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MinigamePresentation.png"), true, false);
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
    ADD_LATENT_AUTOMATION_COMMAND(FArcadeRenderedFinish(this));
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.f));
    ADD_LATENT_AUTOMATION_COMMAND(FArcadeRenderedCleanup(this));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeStandalonePresentationRuntimeTest, "Arcade.Minigame.StandalonePresentationRuntime", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FArcadeStandalonePresentationRuntimeTest::RunTest(const FString&)
{
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("Standalone world"), World)) return false;
    auto* Mode = Cast<AArcadeMinigameTestGameMode>(World->GetAuthGameMode());
    if (!TestNotNull(TEXT("Shared standalone host"), Mode)) return false;
    if (!TestNotNull(TEXT("Standalone session"), Mode->TestSession.Get())) return false;
    TestEqual(TEXT("Standalone running through capture path"), Mode->TestSession->State, EArcadeMinigameState::Running);
    auto* P = Mode->Presentation.Get();
    if (!TestNotNull(TEXT("Common presenter"), P)) return false;
    TestTrue(TEXT("Standalone frame ready and displayed"), P->IsReady() && P->Frame->IsInViewport());
    TestTrue(TEXT("Standalone gameplay UI nested"), P->GameUI && P->GameUI->GetParent());
    TestTrue(TEXT("Standalone completion accepted"), Mode->TestSession->Finish(1.5));
    Mode->Tick(0);
    TestTrue(TEXT("Standalone retains result frame"), P->Frame->IsInViewport());
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MinigameStandalone.png"), true, false);
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
    return true;
}
#endif



