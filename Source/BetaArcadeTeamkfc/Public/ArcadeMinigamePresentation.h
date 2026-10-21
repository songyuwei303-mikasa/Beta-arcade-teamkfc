#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcadeMinigame.h"
#include "Components/SceneCaptureComponent2D.h"
#include "ArcadeMinigamePresentation.generated.h"

class UImage;
class UOverlay;
class USizeBox;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Match OnlyOwnerSee / OwnerNoSee semantics to the source pawn, not the capture helper. */
UCLASS(Transient)
class BETAARCADETEAMKFC_API UArcadeMinigameCapture : public USceneCaptureComponent2D
{
    GENERATED_BODY()
public:
    TWeakObjectPtr<AActor> CameraOwner;
    virtual const AActor* GetViewOwner() const override { return CameraOwner.Get(); }
};

/** Designer-extensible frame. Named widgets are optional only for the native fallback layout. */
UCLASS(Blueprintable)
class BETAARCADETEAMKFC_API UArcadeMinigameView : public UUserWidget
{
    GENERATED_BODY()
public:
    bool Configure(UTextureRenderTarget2D* Texture, UUserWidget* GameUI, int32 InSeat, FVector2D DesignSize);
    void Open();
    void Close(FArcadeMinigameResult Result);
    UFUNCTION(BlueprintCallable, Category="Arcade|Presentation") void FinishOpening() { bOpeningFinished = true; }
    UFUNCTION(BlueprintCallable, Category="Arcade|Presentation") void FinishClosing() { bClosingFinished = true; }
    UFUNCTION(BlueprintNativeEvent, Category="Arcade|Presentation") void OnPresentationOpened();
    UFUNCTION(BlueprintNativeEvent, Category="Arcade|Presentation") void OnPresentationClosing(FArcadeMinigameResult Result);
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Presentation") int32 Seat = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Presentation") TObjectPtr<UTextureRenderTarget2D> ViewTexture;
    UPROPERTY(BlueprintReadOnly, Category="Arcade|Presentation") TObjectPtr<UUserWidget> MinigameUI;
    /** Optional UI-domain material, texture parameter MinigameTexture. Opaque blend is recommended. */
    UPROPERTY(EditDefaultsOnly, Category="Arcade|Presentation") TObjectPtr<UMaterialInterface> ViewMaterial;
    bool IsOpeningFinished() const { return bOpeningFinished; }
    bool IsClosingFinished() const { return bClosingFinished; }
protected:
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Arcade|Presentation") TObjectPtr<UImage> ViewImage;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Arcade|Presentation") TObjectPtr<UOverlay> GameUIRoot;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Arcade|Presentation") TObjectPtr<USizeBox> DesignSurface;
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ViewMID;
    bool bOpeningFinished = false;
    bool bClosingFinished = false;
};

USTRUCT(BlueprintType)
struct FArcadePresentationSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation") TSubclassOf<UArcadeMinigameView> FrameClass;
    /** Reference coordinates for gameplay UI; keep same aspect ratio as render resolution. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation") FIntPoint DesignResolution = FIntPoint(1920, 1080);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation") FIntPoint RenderResolution = FIntPoint(1280, 720);
    /** Outer window in DPI-scaled UI units; native layout letterboxes if its aspect differs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation") FVector2D WindowSize = FVector2D(640, 360);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation") FVector2D ScreenMargin = FVector2D(24, 24);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation", meta=(ClampMin="1", ClampMax="60")) float CaptureFPS = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation", meta=(ClampMin="0", ClampMax="30")) float CloseTimeout = 5;
};

/** Shared SceneCapture/RT/UI path for streamed and standalone minigames. */
UCLASS(NotBlueprintable, Transient)
class BETAARCADETEAMKFC_API AArcadeMinigamePresentation : public AActor
{
    GENERATED_BODY()
public:
    AArcadeMinigamePresentation();
    bool Initialize(AArcadeMinigameEntry* InEntry, int32 Seat, const FArcadePresentationSettings& Settings, bool bFullscreen = false);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool IsReady() const;
    bool HasFailed() const { return bFailed; }
    void BeginClose(FArcadeMinigameResult Result);
    bool IsClosed() const;
    void Freeze();
    /** Exposed read-only for diagnostics/tests; gameplay never needs the capture actor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arcade|Presentation") TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Arcade|Presentation") TObjectPtr<UTextureRenderTarget2D> RenderTarget;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Arcade|Presentation") TObjectPtr<UArcadeMinigameView> Frame;
    UPROPERTY(Transient, BlueprintReadOnly, Category="Arcade|Presentation") TObjectPtr<UUserWidget> GameUI;
    /** Returns false for missing or foreign camera; also synchronizes show-only instance actors. */
    bool SyncCamera(float DeltaSeconds);
private:
    friend struct FArcadePresentationTestAccess;
    UPROPERTY(Transient) TObjectPtr<AArcadeMinigameEntry> Entry;
    FArcadePresentationSettings Config;
    int32 PresentationSeat = 0;
    bool bFullscreen = false;
    bool bFrozen = false;
    bool bClosing = false;
    bool bFailed = false;
    double LastCapture = -1;
    double CloseStarted = 0;
    void Layout();
};
