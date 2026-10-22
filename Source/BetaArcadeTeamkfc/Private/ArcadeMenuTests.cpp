#include "ArcadeBoardRound.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
UObject* ReadMenuReference(AActor* Manager, const TCHAR* Name)
{
    const auto* Property = FindFProperty<FObjectPropertyBase>(Manager->GetClass(), Name);
    return Property ? Property->GetObjectPropertyValue_InContainer(Manager) : nullptr;
}

class FArcadeMenuRuntimeCommand : public IAutomationLatentCommand
{
public:
    explicit FArcadeMenuRuntimeCommand(FAutomationTestBase* InTest)
        : Test(InTest), Started(FPlatformTime::Seconds()) {}

    virtual bool Update() override
    {
        if (FPlatformTime::Seconds() - Started < 2) return false;
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::Game) { World = Context.World(); break; }
        if (!World) { Test->AddError(TEXT("Menu test requires L_Main in a game world")); return true; }

        AActor* Manager = nullptr;
        for (TActorIterator<AActor> It(World); It; ++It)
            if (It->GetClass()->GetName() == TEXT("BP_ArcadeUIManager_C")) { Manager = *It; break; }
        if (!Test->TestNotNull(TEXT("Main map has blueprint UI manager"), Manager)) return true;
        auto* Root = Cast<UUserWidget>(ReadMenuReference(Manager, TEXT("RootUI")));
        auto* Menu = Cast<UUserWidget>(ReadMenuReference(Manager, TEXT("MainMenuUI")));
        auto* Round = Cast<AArcadeBoardRound>(ReadMenuReference(Manager, TEXT("RoundRef")));
        if (!Test->TestNotNull(TEXT("Blueprint BeginPlay creates root UI"), Root) ||
            !Test->TestNotNull(TEXT("Nested main menu is bound"), Menu) ||
            !Test->TestNotNull(TEXT("UI manager binds actual round"), Round)) return true;
        auto* Pages = Cast<UWidgetSwitcher>(Root->GetWidgetFromName(TEXT("WS_Pages")));
        if (!Test->TestNotNull(TEXT("Root page switcher exists"), Pages)) return true;
        Test->TestTrue(TEXT("Root UI is in viewport"), Root->IsInViewport());

        if (Phase == 0)
        {
            Test->TestEqual(TEXT("World begins at main menu"), Round->FlowState, EArcadeFlowState::MainMenu);
            Test->TestTrue(TEXT("Main menu page is active"), Pages->GetActiveWidget() == Menu);
            auto* Start = Cast<UButton>(Menu->GetWidgetFromName(TEXT("BTN_StartGame")));
            if (!Test->TestNotNull(TEXT("Start button exists"), Start)) return true;
            // Exercise UMG -> dispatcher -> manager blueprint -> round, not a direct StartNewRound call.
            Start->OnClicked.Broadcast();
            Test->TestEqual(TEXT("Start button starts match"), Round->FlowState, EArcadeFlowState::Playing);
            Test->TestTrue(TEXT("Flow delegate switches to game HUD"),
                Pages->GetActiveWidget() == Root->GetWidgetFromName(TEXT("Page_GameHUD")));
            if (!Test->TestTrue(TEXT("System can return to menu"), Round->ReturnToMainMenu())) return true;
            Phase = 1;
            return false;
        }
        if (Round->FlowState != EArcadeFlowState::MainMenu)
        {
            if (FPlatformTime::Seconds() - Started < 10) return false;
            Test->AddError(TEXT("Return to main menu timed out"));
            return true;
        }
        Test->TestTrue(TEXT("Cleanup restores main menu page"), Pages->GetActiveWidget() == Menu);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/MenuRuntime.png"), true, false);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started;
    int32 Phase = 0;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArcadeMenuRuntimeTest, "Arcade.UI.MenuRuntime", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FArcadeMenuRuntimeTest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FArcadeMenuRuntimeCommand(this));
    return true;
}
#endif
