#pragma once
#include "CoreMinimal.h"
#include "ArcadeMenuWidget.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ArcadeInputDemo.generated.h"
class UTextBlock;

UCLASS()
class BETAARCADETEAMKFC_API UArcadeInputDemoWidget : public UArcadeMenuWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual void ArcadeButton_Implementation(int32 Seat, EArcadeInputAction Action, bool bPressed) override;
    virtual void ArcadeDirection_Implementation(int32 Seat, FVector2D Direction) override;
private:
    UFUNCTION() void TestClick();
    UFUNCTION() void Reassign();
    UFUNCTION() void ToggleNavigation();
    bool bNavigationEnabled = true;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
    int32 ClickCount = 0;
};

UCLASS()
class BETAARCADETEAMKFC_API AArcadeInputDemoController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
};

/** Temporary playable input workbench, not the final board GameMode. */
UCLASS()
class BETAARCADETEAMKFC_API AArcadeInputDemoGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AArcadeInputDemoGameMode();
};
