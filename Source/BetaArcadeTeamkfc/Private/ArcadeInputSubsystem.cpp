#include "ArcadeInputSubsystem.h"
#include "ArcadeSonyInput.h"
#include "ArcadeGamepadMath.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Xinput.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class FArcadeInputProcessor final : public IInputProcessor
{
public:
    explicit FArcadeInputProcessor(UArcadeInputSubsystem* InOwner) : Owner(InOwner) {}
    virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& E) override
    {
        if (auto* S = Owner.Get(); S && S->CanProcessInput())
            return E.GetKey().IsGamepadKey() || S->HandleKey(E.GetKey(), true);
        return false;
    }
    virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& E) override
    {
        if (auto* S = Owner.Get(); S && S->CanProcessInput())
            return E.GetKey().IsGamepadKey() || S->HandleKey(E.GetKey(), false);
        return false;
    }
    virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& E) override
    {
        auto* S = Owner.Get();
        return S && S->CanProcessInput() && E.GetKey().IsGamepadKey();
    }
    virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& E) override
    {
        if (!E.GetCursorDelta().IsNearlyZero()) if (auto* S = Owner.Get()) S->MouseActivity();
        return false; // Native UMG mouse hit testing and clicks remain intact.
    }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override
    {
        if (auto* S = Owner.Get()) S->MouseActivity();
        return false;
    }
    virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent&, const FPointerEvent*) override
    {
        if (auto* S = Owner.Get()) S->MouseActivity();
        return false;
    }
private:
    TWeakObjectPtr<UArcadeInputSubsystem> Owner;
};

void UArcadeInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (IsRunningDedicatedServer() || IsRunningCommandlet()) return;
    LastMouseActivity = FPlatformTime::Seconds();
    if (FSlateApplication::IsInitialized())
    {
        Processor = MakeShared<FArcadeInputProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(Processor, 0);
    }
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick));
}

void UArcadeInputSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    if (FSlateApplication::IsInitialized() && Processor)
        FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
    Processor.Reset();
    SonyInput.Reset();
    for (int32 Seat = 0; Seat < 2; ++Seat) CancelSeat(Seat);
    Super::Deinitialize();
}

bool UArcadeInputSubsystem::CanProcessInput() const
{
    if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive()) return false;
    const auto* GI = GetGameInstance();
    const auto* Viewport = GI ? GI->GetGameViewportClient() : nullptr;
    const auto Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
    return Widget && (Widget->HasKeyboardFocus() || Widget->HasFocusedDescendants());
}

bool UArcadeInputSubsystem::HandleKey(FKey Key, bool bDown)
{
    const bool bMapped = Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D ||
        Key == EKeys::SpaceBar || Key == EKeys::Escape || Key == EKeys::Up || Key == EKeys::Down ||
        Key == EKeys::Left || Key == EKeys::Right || Key == EKeys::Enter || Key == EKeys::NumPadZero || Key == EKeys::Delete;
    if (!bMapped) return false;
    if (bDown) HeldKeys.Add(Key); else HeldKeys.Remove(Key);
    if (CanProcessInput()) for (int32 I = 0; I < 2; ++I) ProcessSeat(I, true);
    return true; // Prevent duplicate Slate navigation/accept and PIE Escape while playing.
}

void UArcadeInputSubsystem::MouseActivity()
{
    LastMouseActivity = FPlatformTime::Seconds();
    if (CanProcessInput()) if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController()) PC->bShowMouseCursor = true;
}

void UArcadeInputSubsystem::PollSources()
{
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Pads[Index] = FArcadeInputState();
#if PLATFORM_WINDOWS
        XINPUT_STATE State{};
        if (XInputGetState(Index, &State) == ERROR_SUCCESS)
        {
            auto& P = Pads[Index];
            P.bConnected = true;
            const auto& G = State.Gamepad;
            FVector2D V(FMath::Clamp(G.sThumbLX / 32767.f, -1.f, 1.f), FMath::Clamp(G.sThumbLY / 32767.f, -1.f, 1.f));
            const FVector2D DPad(int32((G.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0) - int32((G.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0),
                int32((G.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0) - int32((G.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0));
            P.Direction = ArcadeGamepad::Direction(V, DPad, StickDeadZone);
            P.DeviceName = TEXT("Xbox / XInput");
            P.bConfirm = (G.wButtons & (XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X)) != 0;
            P.bExit = (G.wButtons & XINPUT_GAMEPAD_B) != 0;
        }
#endif
    }
    if (!SonyInput) SonyInput = MakeShared<FArcadeSonyInput>();
    SonyInput->Poll(&Pads[4], StickDeadZone);
}

bool UArcadeInputSubsystem::IsConnected(EArcadeInputSource Source) const
{
    if (Source == EArcadeInputSource::Keyboard1 || Source == EArcadeInputSource::Keyboard2) return true;
    const int32 Index = int32(Source) - int32(EArcadeInputSource::Gamepad1);
    return Index >= 0 && Index < UE_ARRAY_COUNT(Pads) && Pads[Index].bConnected;
}

TArray<EArcadeInputSource> UArcadeInputSubsystem::GetAvailableSources() const
{
    TArray<EArcadeInputSource> Result;
    for (int32 I = 0; I < UE_ARRAY_COUNT(Pads); ++I) if (Pads[I].bConnected) Result.Add(EArcadeInputSource(int32(EArcadeInputSource::Gamepad1) + I));
    Result.Add(EArcadeInputSource::Keyboard1);
    Result.Add(EArcadeInputSource::Keyboard2);
    return Result;
}

FArcadeInputState UArcadeInputSubsystem::ReadSource(EArcadeInputSource Source) const
{
    FArcadeInputState Result;
    Result.Source = Source;
    Result.bConnected = IsConnected(Source);
    if (!Result.bConnected) return Result;
    if (Source == EArcadeInputSource::Keyboard1 || Source == EArcadeInputSource::Keyboard2)
    {
        const bool K1 = Source == EArcadeInputSource::Keyboard1;
        Result.Direction = FVector2D(int32(HeldKeys.Contains(K1 ? EKeys::D : EKeys::Right)) - int32(HeldKeys.Contains(K1 ? EKeys::A : EKeys::Left)),
            int32(HeldKeys.Contains(K1 ? EKeys::W : EKeys::Up)) - int32(HeldKeys.Contains(K1 ? EKeys::S : EKeys::Down))).GetClampedToMaxSize(1.0);
        Result.bConfirm = K1 ? HeldKeys.Contains(EKeys::SpaceBar) : HeldKeys.Contains(EKeys::Enter) || HeldKeys.Contains(EKeys::NumPadZero);
        Result.bExit = HeldKeys.Contains(K1 ? EKeys::Escape : EKeys::Delete);
    }
    else
    {
        Result = Pads[int32(Source) - int32(EArcadeInputSource::Gamepad1)];
        Result.Source = Source;
    }
    return Result;
}

FArcadeInputState UArcadeInputSubsystem::GetSeatState(int32 Seat) const { return Seat >= 0 && Seat < 2 ? Seats[Seat] : FArcadeInputState(); }
UObject* UArcadeInputSubsystem::GetInputReceiver(int32 Seat) const { return Seat >= 0 && Seat < 2 ? Receivers[Seat].Get() : nullptr; }

void UArcadeInputSubsystem::CancelSeat(int32 Seat)
{
    Seats[Seat].Direction = FVector2D::ZeroVector;
    Seats[Seat].bConfirm = Seats[Seat].bExit = false;
    bWaitingForNeutral[Seat] = true;
    if (auto* Target = Receivers[Seat].Get())
    {
        IArcadeInputReceiver::Execute_ArcadeInputCancelled(Target, Seat);
    }
}

bool UArcadeInputSubsystem::SetInputReceiver(int32 Seat, UObject* Receiver)
{
    if (Seat < 0 || Seat > 1 || (Receiver && !Receiver->GetClass()->ImplementsInterface(UArcadeInputReceiver::StaticClass()))) return false;
    if (Receivers[Seat].Get() == Receiver) return true;
    CancelSeat(Seat);
    Receivers[Seat] = Receiver;
    return true;
}

bool UArcadeInputSubsystem::AssignSeat(int32 Seat, EArcadeInputSource Source)
{
    if (Seat < 0 || Seat > 1) return false;
    if (Source != EArcadeInputSource::None && (!IsConnected(Source) || Seats[1 - Seat].Source == Source)) return false;
    CancelSeat(Seat);
    Seats[Seat].Source = Source;
    Seats[Seat].bConnected = IsConnected(Source);
    OnSeatChanged.Broadcast(Seat, Seats[Seat]);
    return true;
}

void UArcadeInputSubsystem::AutoAssignSeats()
{
    // Explicit rescan for newly attached Sony pads; avoids per-second EnumDevices hitches.
    if (!SonyInput) SonyInput = MakeShared<FArcadeSonyInput>();
    SonyInput->RequestDeviceScan();
    PollSources();
    for (int32 I = 0; I < 2; ++I) { CancelSeat(I); Seats[I] = FArcadeInputState(); }
    const auto Sources = GetAvailableSources();
    AssignSeat(0, Sources[0]);
    AssignSeat(1, Sources[1]);
    bAssigned = true;
}

bool UArcadeInputSubsystem::Tick(float)
{
    PollSources();
    if (!bAssigned) AutoAssignSeats();
    const bool bActive = CanProcessInput();
    if (bWasActive && !bActive)
    {
        HeldKeys.Reset();
        for (int32 I = 0; I < 2; ++I) CancelSeat(I);
    }
    bWasActive = bActive;
    for (int32 I = 0; I < 2; ++I) ProcessSeat(I, bActive);
    if (bActive) if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController())
        PC->bShowMouseCursor = CursorIdleSeconds <= 0.f || FPlatformTime::Seconds() - LastMouseActivity < CursorIdleSeconds;
    return true;
}

void UArcadeInputSubsystem::ProcessSeat(int32 I, bool bActive)
{

    const FArcadeInputState Raw = ReadSource(Seats[I].Source);
    if (Raw.bConnected != Seats[I].bConnected)
    {
        CancelSeat(I);
        Seats[I].bConnected = Raw.bConnected;
        OnSeatChanged.Broadcast(I, Seats[I]);
    }
    if (!bActive || !Raw.bConnected) return;
    if (bWaitingForNeutral[I])
    {
        if (Raw.Direction.IsNearlyZero() && !Raw.bConfirm && !Raw.bExit) bWaitingForNeutral[I] = false;
        return;
    }
    const auto Old = Seats[I];
    Seats[I] = Raw;
    // A receiver may change during a callback. Stop dispatching the old frame in that case.
    TWeakObjectPtr<UObject> Target = Receivers[I];
    if (Target.IsValid() && !Raw.Direction.Equals(Old.Direction, 0.0001))
        IArcadeInputReceiver::Execute_ArcadeDirection(Target.Get(), I, Raw.Direction);
    if (Target.IsValid() && Receivers[I] == Target && !bWaitingForNeutral[I] && Raw.bConfirm != Old.bConfirm)
        IArcadeInputReceiver::Execute_ArcadeButton(Target.Get(), I, EArcadeInputAction::Confirm, Raw.bConfirm);
    if (Target.IsValid() && Receivers[I] == Target && !bWaitingForNeutral[I] && Raw.bExit != Old.bExit)
        IArcadeInputReceiver::Execute_ArcadeButton(Target.Get(), I, EArcadeInputAction::Exit, Raw.bExit);
}
