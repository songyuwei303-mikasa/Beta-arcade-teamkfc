#include "ArcadeMinigamePresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

bool UArcadeMinigameView::Configure(UTextureRenderTarget2D* Texture, UUserWidget* GameUI, int32 InSeat, FVector2D DesignSize)
{
    Seat = InSeat;
    ViewTexture = Texture;
    MinigameUI = GameUI;
    SetIsFocusable(false);
    if (!WidgetTree->RootWidget)
    {
        auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("FitView"));
        Scale->SetStretch(EStretch::ScaleToFit);
        auto* Background = WidgetTree->ConstructWidget<UBorder>();
        Background->SetBrushColor(FLinearColor::Black);
        Background->SetPadding(FMargin(0));
        Background->AddChild(Scale);
        WidgetTree->RootWidget = Background;
        DesignSurface = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DesignSurface"));
        Scale->AddChild(DesignSurface);
        auto* Layers = WidgetTree->ConstructWidget<UOverlay>();
        Layers->SetClipping(EWidgetClipping::ClipToBounds);
        DesignSurface->AddChild(Layers);
        ViewImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ViewImage"));
        auto* ImageSlot = Layers->AddChildToOverlay(ViewImage);
        ImageSlot->SetHorizontalAlignment(HAlign_Fill);
        ImageSlot->SetVerticalAlignment(VAlign_Fill);
        GameUIRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GameUIRoot"));
        auto* UISlot = Layers->AddChildToOverlay(GameUIRoot);
        UISlot->SetHorizontalAlignment(HAlign_Fill);
        UISlot->SetVerticalAlignment(VAlign_Fill);
    }
    if (!ViewImage || !GameUIRoot || !DesignSurface) return false;
    DesignSurface->SetWidthOverride(DesignSize.X);
    DesignSurface->SetHeightOverride(DesignSize.Y);
    FSlateBrush Brush;
    Brush.ImageSize = DesignSize;
    Brush.SetResourceObject(Texture);
    if (ViewMaterial)
    {
        ViewMID = UMaterialInstanceDynamic::Create(ViewMaterial, this);
        ViewMID->SetTextureParameterValue(TEXT("MinigameTexture"), Texture);
        Brush.SetResourceObject(ViewMID);
    }
    ViewImage->SetBrush(Brush);
    ViewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    GameUIRoot->ClearChildren();
    if (GameUI)
    {
        auto* ContentSlot = GameUIRoot->AddChildToOverlay(GameUI);
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Fill);
    }
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    return true;
}

void UArcadeMinigameView::Open() { bOpeningFinished = false; OnPresentationOpened(); }
void UArcadeMinigameView::Close(FArcadeMinigameResult Result) { bClosingFinished = false; OnPresentationClosing(Result); }
void UArcadeMinigameView::OnPresentationOpened_Implementation() { FinishOpening(); }
void UArcadeMinigameView::OnPresentationClosing_Implementation(FArcadeMinigameResult Result) { FinishClosing(); }

AArcadeMinigamePresentation::AArcadeMinigamePresentation()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    Capture = CreateDefaultSubobject<UArcadeMinigameCapture>(TEXT("Capture"));
    SetRootComponent(Capture);
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->bAlwaysPersistRenderingState = true;
    // Keep tone mapping, but output linear color. Slate/UI performs the final display
    // conversion; feeding display-encoded LDR through that path can lift midtones twice.
    Capture->CaptureSource = SCS_FinalToneCurveHDR;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
}

bool AArcadeMinigamePresentation::Initialize(AArcadeMinigameEntry* InEntry, int32 Seat, const FArcadePresentationSettings& Settings, bool Fullscreen)
{
    if (Entry || !IsValid(InEntry) || InEntry->GetWorld() != GetWorld()) return false;
    Entry = InEntry;
    Config = Settings;
    PresentationSeat = Seat;
    bFullscreen = Fullscreen;
    const auto D = Config.DesignResolution;
    const auto R = Config.RenderResolution;
    if (D.X < 1 || D.Y < 1 || R.X < 16 || R.Y < 16 || R.X > 4096 || R.Y > 4096 ||
        FMath::Abs(double(D.X) / D.Y - double(R.X) / R.Y) > 0.01 ||
        Config.WindowSize.ContainsNaN() || Config.WindowSize.X <= 0 || Config.WindowSize.Y <= 0)
        return false;
    if (!SyncCamera(0)) return false;
    auto* GI = GetGameInstance();
    auto* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
    if (!PC) return false;
    RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    RenderTarget->ClearColor = FLinearColor::Black;
    RenderTarget->TargetGamma = 1.f;
    RenderTarget->InitCustomFormat(R.X, R.Y, PF_FloatRGBA, true);
    RenderTarget->UpdateResourceImmediate(true);
    Capture->TextureTarget = RenderTarget;
    if (Entry->MinigameWidgetClass)
    {
        GameUI = CreateWidget<UUserWidget>(PC, Entry->MinigameWidgetClass);
        if (!GameUI) return false;
    }
    Entry->SetPresentationWidget(GameUI);
    Frame = CreateWidget<UArcadeMinigameView>(PC, Config.FrameClass ? Config.FrameClass.Get() : UArcadeMinigameView::StaticClass());
    if (!Frame || !Frame->Configure(RenderTarget, GameUI, Seat, FVector2D(D))) return false;
    Frame->AddToViewport(100 + Seat);
    Layout();
    Capture->CaptureScene();
    LastCapture = FPlatformTime::Seconds();
    Frame->Open();
    return true;
}

bool AArcadeMinigamePresentation::SyncCamera(float DeltaSeconds)
{
    if (!IsValid(Entry)) return false;
    auto* Camera = Entry->GetMinigameCamera();
    if (!IsValid(Camera) || !Camera->GetOwner() || Camera->GetOwner()->GetLevel() != Entry->GetLevel()) return false;
    FMinimalViewInfo View;
    Camera->GetCameraView(DeltaSeconds, View);
    Capture->SetWorldLocationAndRotation(View.Location, View.Rotation);
    Capture->ProjectionType = View.ProjectionMode;
    Capture->FOVAngle = View.FOV;
    CastChecked<UArcadeMinigameCapture>(Capture)->CameraOwner = Camera->GetOwner();
    Capture->FirstPersonFieldOfView = View.FirstPersonFOV;
    Capture->FirstPersonScale = View.FirstPersonScale;
    Capture->bEnableFirstPersonFieldOfView = Camera->bEnableFirstPersonFieldOfView;
    Capture->bEnableFirstPersonScale = Camera->bEnableFirstPersonScale;
    Capture->bOverride_CustomNearClippingPlane = View.PerspectiveNearClipPlane > 0;
    Capture->CustomNearClippingPlane = View.GetFinalPerspectiveNearClipPlane();
    Capture->OrthoWidth = View.OrthoWidth;
    Capture->PostProcessSettings = View.PostProcessSettings;
    Capture->PostProcessBlendWeight = View.PostProcessBlendWeight;
    Capture->ShowOnlyActors.Reset();
    for (AActor* Actor : Entry->GetLevel()->Actors)
        if (IsValid(Actor) && Actor != this) Capture->ShowOnlyActors.Add(Actor);
    return true;
}

void AArcadeMinigamePresentation::Layout()
{
    if (!Frame || !Frame->GetOwningPlayer()) return;
    int32 X = 0, Y = 0;
    Frame->GetOwningPlayer()->GetViewportSize(X, Y);
    const float DPI = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    FVector2D Size = bFullscreen ? FVector2D(X, Y) / DPI : Config.WindowSize;
    if (Size.X <= 0 || Size.Y <= 0) return;
    // SetPositionInViewport uses absolute viewport offsets. Compute the right-hand position
    // explicitly instead of combining it with an anchored/aligned negative offset.
    const FVector2D Position = bFullscreen ? FVector2D::ZeroVector : FVector2D(
        PresentationSeat == 1 ? FMath::Max(0.0, X / DPI - Size.X - Config.ScreenMargin.X) : Config.ScreenMargin.X,
        Config.ScreenMargin.Y);
    Frame->SetAnchorsInViewport(FAnchors(0, 0));
    Frame->SetAlignmentInViewport(FVector2D::ZeroVector);
    Frame->SetPositionInViewport(Position, false);
    Frame->SetDesiredSizeInViewport(Size);
}

void AArcadeMinigamePresentation::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Layout();
    if (bFrozen || bFailed) return;
    if (!SyncCamera(DeltaSeconds)) { bFailed = true; return; }
    const double Now = FPlatformTime::Seconds();
    if (Now - LastCapture >= 1.0 / FMath::Clamp(Config.CaptureFPS, 1.f, 60.f))
    {
        Capture->CaptureScene();
        LastCapture = Now;
    }
}

bool AArcadeMinigamePresentation::IsReady() const { return Frame && Frame->IsOpeningFinished() && !bFailed; }
void AArcadeMinigamePresentation::Freeze() { bFrozen = true; }
void AArcadeMinigamePresentation::BeginClose(FArcadeMinigameResult Result)
{
    if (bClosing) return;
    bClosing = true;
    Freeze();
    CloseStarted = FPlatformTime::Seconds();
    if (Frame) Frame->Close(Result);
}
bool AArcadeMinigamePresentation::IsClosed() const
{
    return bClosing && (!Frame || Frame->IsClosingFinished() || FPlatformTime::Seconds() - CloseStarted >= FMath::Clamp(Config.CloseTimeout, 0.f, 30.f));
}
void AArcadeMinigamePresentation::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(Entry) && Entry->GetMinigameWidget() == GameUI) Entry->SetPresentationWidget(nullptr);
    if (Frame) Frame->RemoveFromParent();
    if (GameUI) GameUI->RemoveFromParent();
    Capture->TextureTarget = nullptr;
    Capture->ShowOnlyActors.Reset();
    Frame = nullptr;
    GameUI = nullptr;
    RenderTarget = nullptr;
    Super::EndPlay(Reason);
}
