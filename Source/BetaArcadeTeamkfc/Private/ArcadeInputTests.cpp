#include "ArcadeInputSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "ArcadeGamepadMath.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FArcadeInputTestAccess
{
    static FArcadeInputState Read(UArcadeInputSubsystem* S, EArcadeInputSource Source) { return S->ReadSource(Source); }
    static void Pad(UArcadeInputSubsystem* S, int32 Index, bool Connected, bool Confirm = false)
    { S->Pads[Index].bConnected = Connected; S->Pads[Index].bConfirm = Confirm; }
    static void Process(UArcadeInputSubsystem* S, int32 Seat) { S->ProcessSeat(Seat, true); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeKeyboardTest, "Arcade.Input.KeyboardMappings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeKeyboardTest::RunTest(const FString&)
{
    auto* S = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    const auto K1 = EArcadeInputSource::Keyboard1, K2 = EArcadeInputSource::Keyboard2;
    TestFalse(TEXT("Mouse does not enter player inputs"), S->HandleKey(EKeys::LeftMouseButton, true));
    S->HandleKey(EKeys::W, true); S->HandleKey(EKeys::D, true);
    const auto Diagonal = FArcadeInputTestAccess::Read(S, K1).Direction;
    TestTrue(TEXT("Diagonal is normalized"), FMath::IsNearlyEqual(Diagonal.Size(), 1.0));
    TestTrue(TEXT("Up and right positive"), Diagonal.X > 0 && Diagonal.Y > 0);
    TestTrue(TEXT("Keyboard 2 is independent"), FArcadeInputTestAccess::Read(S, K2).Direction.IsNearlyZero());
    S->HandleKey(EKeys::A, true);
    TestEqual(TEXT("Opposite horizontal keys cancel"), FArcadeInputTestAccess::Read(S, K1).Direction.X, 0.0);
    S->HandleKey(EKeys::Enter, true); S->HandleKey(EKeys::NumPadZero, true);
    S->HandleKey(EKeys::Enter, false);
    TestTrue(TEXT("Second confirm key holds aggregate"), FArcadeInputTestAccess::Read(S, K2).bConfirm);
    S->HandleKey(EKeys::NumPadZero, false);
    TestFalse(TEXT("Confirm releases after both keys"), FArcadeInputTestAccess::Read(S, K2).bConfirm);
    S->HandleKey(EKeys::Escape, true); S->HandleKey(EKeys::Delete, true);
    TestTrue(TEXT("Escape exits K1"), FArcadeInputTestAccess::Read(S, K1).bExit);
    TestTrue(TEXT("Delete exits K2"), FArcadeInputTestAccess::Read(S, K2).bExit);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeSeatTest, "Arcade.Input.SeatIsolationAndReconnect", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeSeatTest::RunTest(const FString&)
{
    auto* S = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    FArcadeInputTestAccess::Pad(S, 1, true);
    const auto Sources = S->GetAvailableSources();
    TestEqual(TEXT("Connected pad precedes keyboards"), Sources[0], EArcadeInputSource::Gamepad2);
    TestEqual(TEXT("Keyboard 1 before keyboard 2"), Sources[1], EArcadeInputSource::Keyboard1);
    TestTrue(TEXT("Assign pad"), S->AssignSeat(0, Sources[0]));
    TestFalse(TEXT("Cannot share physical pad"), S->AssignSeat(1, Sources[0]));
    TestFalse(TEXT("Reject invalid seat"), S->AssignSeat(2, Sources[1]));
    TestFalse(TEXT("Reject nonreceiver object"), S->SetInputReceiver(0, S));
    FArcadeInputTestAccess::Process(S, 0); // neutral unlock
    FArcadeInputTestAccess::Pad(S, 1, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestTrue(TEXT("Confirm delivered"), S->GetSeatState(0).bConfirm);
    FArcadeInputTestAccess::Pad(S, 1, false);
    FArcadeInputTestAccess::Process(S, 0);
    TestFalse(TEXT("Disconnect cancels held confirm"), S->GetSeatState(0).bConfirm);
    TestEqual(TEXT("Disconnect reserves source"), S->GetSeatState(0).Source, EArcadeInputSource::Gamepad2);
    FArcadeInputTestAccess::Pad(S, 1, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestFalse(TEXT("Reconnect while held cannot auto confirm"), S->GetSeatState(0).bConfirm);
    FArcadeInputTestAccess::Pad(S, 1, true, false);
    FArcadeInputTestAccess::Process(S, 0);
    FArcadeInputTestAccess::Pad(S, 1, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestTrue(TEXT("Fresh press after neutral works"), S->GetSeatState(0).bConfirm);
    TestFalse(TEXT("Other seat unchanged"), S->GetSeatState(1).bConfirm);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeDPadTest, "Arcade.Input.DPadAndStick", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArcadeDPadTest::RunTest(const FString&)
{
    using namespace ArcadeGamepad;
    TestTrue(TEXT("Centered POV"), POV(0xffffffff).IsNearlyZero());
    TestTrue(TEXT("16-bit neutral POV"), POV(0xffff).IsNearlyZero());
    TestEqual(TEXT("POV up"), POV(0), FVector2D(0, 1));
    TestEqual(TEXT("POV right"), POV(9000), FVector2D(1, 0));
    TestEqual(TEXT("POV down"), POV(18000), FVector2D(0, -1));
    TestEqual(TEXT("POV left"), POV(27000), FVector2D(-1, 0));
    TestEqual(TEXT("D-pad wins over opposite stick"), Direction(FVector2D(-1, 0), POV(9000), 0.2f), FVector2D(1, 0));
    TestTrue(TEXT("Diagonal limited to unit length"), FMath::IsNearlyEqual(Direction(FVector2D(1, 0), POV(4500), 0.2f).Size(), 1.0));
    TestEqual(TEXT("Released D-pad returns stick"), Direction(FVector2D(0, 1), POV(0xffffffff), 0.2f), FVector2D(0, 1));
    TestTrue(TEXT("Stick drift ignored"), Direction(FVector2D(0.05, 0), FVector2D::ZeroVector, 0.2f).IsNearlyZero());
    auto* S = NewObject<UArcadeInputSubsystem>(NewObject<UGameInstance>());
    FArcadeInputTestAccess::Pad(S, 4, true);
    TestEqual(TEXT("Native PS before keyboard"), S->GetAvailableSources()[0], EArcadeInputSource::PlayStation1);
    TestTrue(TEXT("Native PS can own seat"), S->AssignSeat(0, EArcadeInputSource::PlayStation1));
    FArcadeInputTestAccess::Process(S, 0);
    FArcadeInputTestAccess::Pad(S, 4, true, true);
    FArcadeInputTestAccess::Process(S, 0);
    TestTrue(TEXT("PS confirm delivered through same interface"), S->GetSeatState(0).bConfirm);
    FArcadeInputTestAccess::Pad(S, 4, false);
    FArcadeInputTestAccess::Process(S, 0);
    TestFalse(TEXT("PS disconnect cancels confirm"), S->GetSeatState(0).bConfirm);
    return true;
}
#endif
