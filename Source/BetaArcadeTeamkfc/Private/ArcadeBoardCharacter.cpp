#include "ArcadeBoardCharacter.h"
#include "ArcadeInputSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AArcadeBoardCharacter::AArcadeBoardCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    AutoPossessAI = EAutoPossessAI::Disabled;
    bUseControllerRotationYaw = false;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
    auto* Movement = GetCharacterMovement();
    Movement->bRunPhysicsWithNoController = true;
    Movement->MaxWalkSpeed = 400.f;
    Movement->MaxAcceleration = 2400.f;
    Movement->BrakingDecelerationWalking = 2400.f;
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0, 720, 0);
    Movement->bEnablePhysicsInteraction = false;

    PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    PlaceholderBody->SetupAttachment(GetRootComponent());
    PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlaceholderBody->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.5f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) PlaceholderBody->SetStaticMesh(Cube.Object);

    SeatLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SeatLabel"));
    SeatLabel->SetupAttachment(GetRootComponent());
    SeatLabel->SetAbsolute(false, true, false);
    SeatLabel->SetRelativeLocation(FVector(0, 0, 110));
    SeatLabel->SetHorizontalAlignment(EHTA_Center);
    SeatLabel->SetWorldSize(45.f);
}

void AArcadeBoardCharacter::InitializeSeat(int32 InSeat, float InMovementYaw)
{
    StopBoardMovement();
    LogicalSeat = InSeat >= 0 && InSeat < 2 ? InSeat : INDEX_NONE;
    MovementYaw = InMovementYaw;
    SeatLabel->SetText(FText::FromString(FString::Printf(TEXT("P%d"), LogicalSeat + 1)));
    SeatLabel->SetTextRenderColor(LogicalSeat == 0 ? FColor(70, 150, 255) : FColor(255, 150, 40));
    SeatLabel->SetWorldRotation(FRotator(60.f, MovementYaw + 180.f, 0.f));
}

void AArcadeBoardCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FRotationMatrix Basis(FRotator(0, MovementYaw, 0));
    AddMovementInput(Basis.GetUnitAxis(EAxis::X), MoveDirection.Y);
    AddMovementInput(Basis.GetUnitAxis(EAxis::Y), MoveDirection.X);
}

void AArcadeBoardCharacter::ArcadeDirection_Implementation(int32 Seat, FVector2D Direction)
{
    if (Seat != LogicalSeat || LogicalSeat == INDEX_NONE) return;
    MoveDirection = Direction.ContainsNaN() ? FVector2D::ZeroVector : Direction.GetClampedToMaxSize(1.0);
}

void AArcadeBoardCharacter::ArcadeButton_Implementation(int32 Seat, EArcadeInputAction Action, bool bPressed)
{
    if (Seat == LogicalSeat && LogicalSeat != INDEX_NONE) OnBoardButton(Seat, Action, bPressed);
}

void AArcadeBoardCharacter::StopBoardMovement()
{
    MoveDirection = FVector2D::ZeroVector;
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
}

void AArcadeBoardCharacter::ArcadeInputCancelled_Implementation(int32 Seat)
{
    if (Seat != LogicalSeat || LogicalSeat == INDEX_NONE) return;
    StopBoardMovement();
    OnBoardInputCancelled(Seat);
}

void AArcadeBoardCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (auto* GI = GetGameInstance())
        if (auto* Input = GI->GetSubsystem<UArcadeInputSubsystem>(); Input && LogicalSeat != INDEX_NONE && Input->GetInputReceiver(LogicalSeat) == this)
            Input->SetInputReceiver(LogicalSeat, nullptr);
    StopBoardMovement();
    Super::EndPlay(EndPlayReason);
}
