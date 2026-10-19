#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArcadeInputReceiver.h"
#include "ArcadeMenuWidget.generated.h"

class UButton;

/** Shared UI mouse support is native UMG; seat navigation uses this explicit ordered button list. */
UCLASS()
class BETAARCADETEAMKFC_API UArcadeMenuWidget : public UUserWidget, public IArcadeInputReceiver
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Arcade|UI") void SetNavigationButtons(const TArray<UButton*>& Buttons);
    UFUNCTION(BlueprintPure, Category="Arcade|UI") int32 GetSelectedButton(int32 Seat) const;
    virtual void ArcadeDirection_Implementation(int32 Seat, FVector2D Direction) override;
    virtual void ArcadeButton_Implementation(int32 Seat, EArcadeInputAction Action, bool bPressed) override;
    virtual void ArcadeInputCancelled_Implementation(int32 Seat) override;
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|UI") void OnMenuExit(int32 Seat);
    UFUNCTION(BlueprintImplementableEvent, Category="Arcade|UI") void OnSelectionChanged(int32 Seat, int32 Index);
protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual void NativeDestruct() override;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> NavigationButtons;
private:
    void MoveSelection(int32 Seat, int32 Step);
    void UpdateHighlights();
    int32 Selection[2] = {0, 0};
    int32 NavigationStep[2] = {0, 0};
    float RepeatRemaining[2] = {0.f, 0.f};
};
