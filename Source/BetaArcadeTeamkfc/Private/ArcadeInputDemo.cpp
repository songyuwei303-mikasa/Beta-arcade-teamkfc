#include "ArcadeInputDemo.h"
#include "ArcadeInputSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

TSharedRef<SWidget> UArcadeInputDemoWidget::RebuildWidget()
{

    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.055f, 1.f));
    Background->SetPadding(FMargin(48.f));
    auto* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Column);
    WidgetTree->RootWidget = Background;
    auto AddText = [&](const FString& Text, int32 Size)
    {
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Text));
        auto Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
        Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 0, 0, 18));
        return Label;
    };
    AddText(TEXT("ARCADE / INPUT WORKBENCH"), 28);
    AddText(TEXT("P1: blue   P2: orange   Both: purple\nMouse clicks UI only. Move mouse to reveal cursor (3s idle timeout)."), 16);
    Status = AddText(TEXT("Connecting inputs..."), 20);
    AddText(TEXT("Keyboard 1: WASD / Space / Escape\nKeyboard 2: Arrows / Enter or Numpad 0 / Delete\nXbox: Left stick or D-pad / A or X / B\nPlayStation: Left stick or D-pad / Cross or Square / Circle\nDirection navigates; Confirm clicks. Shift+F1 releases PIE focus."), 16);
    TArray<UButton*> Buttons;
    auto AddButton = [&](const FString& Text)
    {
        auto* Button = WidgetTree->ConstructWidget<UButton>();

        auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Text));
        Button->SetContent(Label);
        Column->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 0, 0, 16));
        Buttons.Add(Button);
        return Button;
    };
    AddButton(TEXT("Test button - mouse and Confirm use the same action"))->OnClicked.AddDynamic(this, &ThisClass::TestClick);
    AddButton(TEXT("Reassign devices now: gamepads > keyboard 1 > keyboard 2"))->OnClicked.AddDynamic(this, &ThisClass::Reassign);
    AddButton(TEXT("Toggle UI navigation / input-only test (mouse can always toggle)"))->OnClicked.AddDynamic(this, &ThisClass::ToggleNavigation);
    Feedback = AddText(TEXT("Ready. Device changes do not steal occupied seats."), 16);
    TSharedRef<SWidget> Result = Super::RebuildWidget();
    SetNavigationButtons(Buttons);
    return Result;
}
void UArcadeInputDemoWidget::NativeConstruct()
{
    Super::NativeConstruct();
    auto* Input = GetGameInstance()->GetSubsystem<UArcadeInputSubsystem>();
    Input->AutoAssignSeats();
    Input->SetInputReceiver(0, this);
    Input->SetInputReceiver(1, this);
}
void UArcadeInputDemoWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    if (!Status) return;
    auto* Input = GetGameInstance()->GetSubsystem<UArcadeInputSubsystem>();
    FString Text;
    for (int32 I = 0; I < 2; ++I)
    {
        const auto S = Input->GetSeatState(I);
        Text += FString::Printf(TEXT("P%d  %s  %s  [%s]\nDirection (%+.2f, %+.2f)    Confirm: %s    Exit: %s\n\n"),
            I + 1, *StaticEnum<EArcadeInputSource>()->GetNameStringByValue(int64(S.Source)), *S.DeviceName, S.bConnected ? TEXT("connected") : TEXT("DISCONNECTED"),
            S.Direction.X, S.Direction.Y, S.bConfirm ? TEXT("held") : TEXT("-"), S.bExit ? TEXT("held") : TEXT("-"));
    }
    Status->SetText(FText::FromString(Text));
}
void UArcadeInputDemoWidget::TestClick()
{
    Feedback->SetText(FText::FromString(FString::Printf(TEXT("Test button activated %d time(s)."), ++ClickCount)));
}
void UArcadeInputDemoWidget::Reassign()
{
    GetGameInstance()->GetSubsystem<UArcadeInputSubsystem>()->AutoAssignSeats();
    Feedback->SetText(FText::FromString(TEXT("Devices reassigned. Release controls to neutral before continuing.")));
}
void UArcadeInputDemoWidget::ArcadeButton_Implementation(int32 Seat, EArcadeInputAction Action, bool bPressed)
{
    if (bNavigationEnabled) Super::ArcadeButton_Implementation(Seat, Action, bPressed);
    else if (Feedback) Feedback->SetText(FText::FromString(FString::Printf(TEXT("Input-only: P%d %s %s. No UI button clicked."), Seat + 1,
        Action == EArcadeInputAction::Confirm ? TEXT("Confirm") : TEXT("Exit"), bPressed ? TEXT("pressed") : TEXT("released"))));
    if (Action == EArcadeInputAction::Exit && bPressed && Feedback)
        Feedback->SetText(FText::FromString(FString::Printf(TEXT("P%d requested Exit. (Test only: application remains open.)"), Seat + 1)));
}
void UArcadeInputDemoWidget::ArcadeDirection_Implementation(int32 Seat, FVector2D Direction)
{
    if (bNavigationEnabled) Super::ArcadeDirection_Implementation(Seat, Direction);
}
void UArcadeInputDemoWidget::ToggleNavigation()
{
    bNavigationEnabled = !bNavigationEnabled;
    for (int32 Seat = 0; Seat < 2; ++Seat) ArcadeInputCancelled_Implementation(Seat);
    Feedback->SetText(FText::FromString(bNavigationEnabled ? TEXT("UI navigation enabled.") : TEXT("Input-only mode: directions and Confirm no longer activate UI. Use mouse to switch back.")));
}
void AArcadeInputDemoController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    if (auto* Widget = CreateWidget<UArcadeInputDemoWidget>(this)) Widget->AddToViewport();
}
AArcadeInputDemoGameMode::AArcadeInputDemoGameMode()
{
    PlayerControllerClass = AArcadeInputDemoController::StaticClass();
    DefaultPawnClass = nullptr;
}

