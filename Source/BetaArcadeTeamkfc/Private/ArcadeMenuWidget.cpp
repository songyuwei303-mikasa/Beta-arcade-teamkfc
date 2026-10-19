#include "ArcadeMenuWidget.h"
#include "ArcadeInputSubsystem.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"

void UArcadeMenuWidget::SetNavigationButtons(const TArray<UButton*>& Buttons)
{
    NavigationButtons.Reset();
    for (auto* Button : Buttons) if (IsValid(Button)) NavigationButtons.AddUnique(Button);
    for (int32 I = 0; I < 2; ++I) { Selection[I] = -1; NavigationStep[I] = 0; MoveSelection(I, 1); }
}
int32 UArcadeMenuWidget::GetSelectedButton(int32 Seat) const { return Seat >= 0 && Seat < 2 ? Selection[Seat] : INDEX_NONE; }

void UArcadeMenuWidget::MoveSelection(int32 Seat, int32 Step)
{
    const int32 Count = NavigationButtons.Num();
    if (!Count) return;
    int32 Candidate = Selection[Seat];
    for (int32 I = 0; I < Count; ++I)
    {
        Candidate = (Candidate + Step + Count) % Count;
        auto* B = NavigationButtons[Candidate].Get();
        if (IsValid(B) && B->GetIsEnabled() && B->IsVisible())
        {
            Selection[Seat] = Candidate;
            UpdateHighlights();
            OnSelectionChanged(Seat, Candidate);
            return;
        }
    }
    Selection[Seat] = INDEX_NONE;
    UpdateHighlights();
}
void UArcadeMenuWidget::UpdateHighlights()
{
    for (int32 I = 0; I < NavigationButtons.Num(); ++I)
    {
        const bool P1 = Selection[0] == I, P2 = Selection[1] == I;
        NavigationButtons[I]->SetBackgroundColor(P1 && P2 ? FLinearColor(0.6f, 0.4f, 1.f) :
            P1 ? FLinearColor(0.1f, 0.6f, 1.f) : P2 ? FLinearColor(1.f, 0.5f, 0.1f) : FLinearColor(0.2f, 0.2f, 0.25f));
    }
}
void UArcadeMenuWidget::ArcadeDirection_Implementation(int32 Seat, FVector2D Direction)
{
    if (Seat < 0 || Seat > 1) return;
    const float Axis = FMath::Abs(Direction.Y) >= FMath::Abs(Direction.X) ? -Direction.Y : Direction.X;
    const int32 Step = FMath::Abs(Axis) > 0.5f ? (Axis > 0 ? 1 : -1) : 0;
    if (Step != NavigationStep[Seat])
    {
        NavigationStep[Seat] = Step;
        RepeatRemaining[Seat] = 0.4f;
        if (Step) MoveSelection(Seat, Step);
    }
}
void UArcadeMenuWidget::ArcadeButton_Implementation(int32 Seat, EArcadeInputAction Action, bool bPressed)
{
    if (Seat < 0 || Seat > 1 || !bPressed) return;
    if (Action == EArcadeInputAction::Exit) { OnMenuExit(Seat); return; }
    if (NavigationButtons.IsValidIndex(Selection[Seat]))
    {
        auto* Button = NavigationButtons[Selection[Seat]].Get();
        if (IsValid(Button) && Button->GetIsEnabled() && Button->IsVisible()) Button->OnClicked.Broadcast();
    }
}
void UArcadeMenuWidget::ArcadeInputCancelled_Implementation(int32 Seat)
{
    if (Seat >= 0 && Seat < 2) { NavigationStep[Seat] = 0; RepeatRemaining[Seat] = 0.f; }
}
void UArcadeMenuWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    auto* Input = GetGameInstance() ? GetGameInstance()->GetSubsystem<UArcadeInputSubsystem>() : nullptr;
    for (int32 I = 0; I < 2; ++I)
    {
        if (!Input || !Input->CanProcessInput() || Input->GetInputReceiver(I) != this) continue;
        if (NavigationStep[I] && (RepeatRemaining[I] -= DeltaSeconds) <= 0.f)
        {
            MoveSelection(I, NavigationStep[I]);
            RepeatRemaining[I] = 0.12f;
        }
    }
}
void UArcadeMenuWidget::NativeDestruct()
{
    if (auto* GI = GetGameInstance()) if (auto* Input = GI->GetSubsystem<UArcadeInputSubsystem>())
        for (int32 I = 0; I < 2; ++I) if (Input->GetInputReceiver(I) == this) Input->SetInputReceiver(I, nullptr);
    Super::NativeDestruct();
}

