#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "InputCoreTypes.h"
#include "ArcadeInputReceiver.h"
#include "ArcadeInputSubsystem.generated.h"

class FArcadeInputProcessor;
class FArcadeSonyInput;

UENUM(BlueprintType)
enum class EArcadeInputSource : uint8 { None, Keyboard1, Keyboard2, Gamepad1, Gamepad2, Gamepad3, Gamepad4, PlayStation1, PlayStation2, PlayStation3, PlayStation4 };

USTRUCT(BlueprintType)
struct FArcadeInputState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EArcadeInputSource Source = EArcadeInputSource::None;
    UPROPERTY(BlueprintReadOnly) bool bConnected = false;
    UPROPERTY(BlueprintReadOnly) FString DeviceName;
    UPROPERTY(BlueprintReadOnly) FVector2D Direction = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bConfirm = false;
    UPROPERTY(BlueprintReadOnly) bool bExit = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArcadeSeatChanged, int32, Seat, FArcadeInputState, State);

/** Two logical seats, independent of UE LocalPlayer indexes. Mouse never occupies a seat. */
UCLASS(Config=Game)
class BETAARCADETEAMKFC_API UArcadeInputSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category="Arcade|Input") FArcadeInputState GetSeatState(int32 Seat) const;
    UFUNCTION(BlueprintPure, Category="Arcade|Input") TArray<EArcadeInputSource> GetAvailableSources() const;
    /** Explicitly rerun priority assignment, e.g. on a future join screen. Cancels previous input. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Input") void AutoAssignSeats();
    /** Rejects disconnected or already claimed sources. None explicitly unassigns a seat. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Input") bool AssignSeat(int32 Seat, EArcadeInputSource Source);
    /** Pass None to stop delivery. Held controls must return to neutral before a new target receives them. */
    UFUNCTION(BlueprintCallable, Category="Arcade|Input") bool SetInputReceiver(int32 Seat, UObject* Receiver);
    UFUNCTION(BlueprintPure, Category="Arcade|Input") UObject* GetInputReceiver(int32 Seat) const;

    UPROPERTY(BlueprintAssignable, Category="Arcade|Input") FArcadeSeatChanged OnSeatChanged;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Arcade|Input", meta=(ClampMin="0.0")) float CursorIdleSeconds = 3.f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Arcade|Input", meta=(ClampMin="0.0", ClampMax="0.95")) float StickDeadZone = 0.2f;

    bool CanProcessInput() const;
    bool HandleKey(FKey Key, bool bDown);
    void MouseActivity();
private:
    friend struct FArcadeInputTestAccess;
    void ProcessSeat(int32 Seat, bool bActive);
    bool Tick(float DeltaSeconds);
    void PollSources();
    void CancelSeat(int32 Seat);
    bool IsConnected(EArcadeInputSource Source) const;
    FArcadeInputState ReadSource(EArcadeInputSource Source) const;
    FArcadeInputState Seats[2];
    TWeakObjectPtr<UObject> Receivers[2];
    FArcadeInputState Pads[8];
    TSharedPtr<FArcadeSonyInput> SonyInput;
    bool bWaitingForNeutral[2] = {true, true};
    bool bWasActive = false;
    bool bAssigned = false;
    TSet<FKey> HeldKeys;
    double LastMouseActivity = 0;
    TSharedPtr<FArcadeInputProcessor> Processor;
    FTSTicker::FDelegateHandle TickHandle;
};
