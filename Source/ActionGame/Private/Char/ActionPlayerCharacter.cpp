#include "Char/ActionPlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/InputBufferComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Logging/LogMacros.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionPlayerCharacter, Log, All);

namespace ActionPlayerInputNames
{
	static const FName Attack = TEXT("Attack");
	static const FName Dodge = TEXT("Dodge");
}

namespace ActionPlayerNotifyNames
{
	// 约定：在攻击 Montage 里添加一个 Montage Notify，并命名为 AttackHitCheck。
	// 后面这套代码就会在那个精确帧触发命中判定。
	static const FName AttackHitCheck = TEXT("AttackHitCheck");
}

AActionPlayerCharacter::AActionPlayerCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// 继续复用模板里的移动/视角/跳跃输入资源。
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultMappingContextFinder(
		TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default"));
	if (DefaultMappingContextFinder.Succeeded())
	{
		DefaultMappingContext = DefaultMappingContextFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionFinder(
		TEXT("/Game/ThirdPerson/Input/Actions/IA_Move.IA_Move"));
	if (MoveActionFinder.Succeeded())
	{
		MoveAction = MoveActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionFinder(
		TEXT("/Game/ThirdPerson/Input/Actions/IA_Look.IA_Look"));
	if (LookActionFinder.Succeeded())
	{
		LookAction = LookActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionFinder(
		TEXT("/Game/ThirdPerson/Input/Actions/IA_Jump.IA_Jump"));
	if (JumpActionFinder.Succeeded())
	{
		JumpAction = JumpActionFinder.Object;
	}

	InputBufferComponent = CreateDefaultSubobject<UInputBufferComponent>(TEXT("InputBufferComponent"));

	static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageFinder(
		TEXT("/Game/Animations/Retargeted/DualSword/AM_DualSword_Attack01.AM_DualSword_Attack01"));
	if (AttackMontageFinder.Succeeded())
	{
		AttackMontage = AttackMontageFinder.Object;
	}
}

bool AActionPlayerCharacter::CanAttack() const
{
	return Super::CanAttack() && !bAttackInProgress;
}

void AActionPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	BuildRuntimeCombatInputMapping();

	// Enhanced Input 的 MappingContext 通常加在 LocalPlayerSubsystem 上。
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				if (DefaultMappingContext != nullptr)
				{
					Subsystem->AddMappingContext(DefaultMappingContext, 0);
				}
				else
				{
					UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: DefaultMappingContext is null in BeginPlay."));
				}

				if (RuntimeCombatMappingContext != nullptr)
				{
					Subsystem->AddMappingContext(RuntimeCombatMappingContext, 1);
				}
				else
				{
					UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: RuntimeCombatMappingContext is null in BeginPlay."));
				}
			}
			else
			{
				UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: EnhancedInputLocalPlayerSubsystem was not found in BeginPlay."));
			}
		}
		else
		{
			UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: LocalPlayer was null in BeginPlay."));
		}
	}
	else
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: PlayerController was null in BeginPlay."));
	}
}

void AActionPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction != nullptr)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}
		else
		{
			UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: JumpAction is null in SetupPlayerInputComponent."));
		}

		if (MoveAction != nullptr)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AActionPlayerCharacter::Move);
		}
		else
		{
			UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: MoveAction is null in SetupPlayerInputComponent."));
		}

		if (LookAction != nullptr)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AActionPlayerCharacter::Look);
		}
		else
		{
			UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: LookAction is null in SetupPlayerInputComponent."));
		}

		if (AttackAction != nullptr)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnAttackInput);
		}
		else
		{
			UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: AttackAction is null in SetupPlayerInputComponent."));
		}

		if (DodgeAction != nullptr)
		{
			EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnDodgeInput);
		}
		else
		{
			UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: DodgeAction is null in SetupPlayerInputComponent."));
		}
	}
	else
	{
		UE_LOG(LogActionPlayerCharacter, Error, TEXT("ActionPlayerCharacter: Enhanced Input component was not found on PlayerInputComponent."));
	}
}

void AActionPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller == nullptr)
	{
		return;
	}

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void AActionPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller == nullptr)
	{
		return;
	}

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AActionPlayerCharacter::BuildRuntimeCombatInputMapping()
{
	if (RuntimeCombatMappingContext == nullptr || AttackAction == nullptr || DodgeAction == nullptr)
	{
		UE_LOG(
			LogActionPlayerCharacter,
			Warning,
			TEXT("ActionPlayerCharacter: Combat input assets are not fully assigned. Mapping=%s AttackAction=%s DodgeAction=%s"),
			*GetNameSafe(RuntimeCombatMappingContext),
			*GetNameSafe(AttackAction),
			*GetNameSafe(DodgeAction));
		return;
	}

	// 现在不再在代码里手工 MapKey。
	// Attack / Dodge 的实际键位由 IMC_PlayerCombat 这类正式输入资源负责。
}

void AActionPlayerCharacter::OnAttackInput()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: OnAttackInput called."));
	InputBufferComponent->PushInput(ActionPlayerInputNames::Attack, InputBufferLifetime);
	TryStartAttack();
}

void AActionPlayerCharacter::OnDodgeInput()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: OnDodgeInput called."));
	InputBufferComponent->PushInput(ActionPlayerInputNames::Dodge, InputBufferLifetime);
}

void AActionPlayerCharacter::TryStartAttack()
{
	if (!CanAttack())
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: TryStartAttack blocked because character cannot attack."));
		return;
	}

	if (!TryConsumeAttackInput())
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: TryStartAttack found no buffered attack input."));
		return;
	}

	if (AttackMontage == nullptr)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: AttackMontage is not set."));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: AnimInstance was not found on character mesh."));
		return;
	}

	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &AActionPlayerCharacter::OnAttackMontageNotifyBegin);
	AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AActionPlayerCharacter::OnAttackMontageNotifyBegin);

	const float PlayedLength = PlayAnimMontage(AttackMontage);
	if (PlayedLength <= 0.0f)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: Failed to play attack montage."));
		return;
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AActionPlayerCharacter::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);

	BeginAttackSequence();
}

bool AActionPlayerCharacter::TryConsumeAttackInput()
{
	if (InputBufferComponent == nullptr)
	{
		return false;
	}

	const bool bConsumed = InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Attack);
	if (bConsumed)
	{
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Attack input consumed from buffer."));
	}

	return bConsumed;
}

void AActionPlayerCharacter::BeginAttackSequence()
{
	bAttackInProgress = true;
	bAttackHitTriggeredThisSequence = false;
	HitActorsThisAttack.Reset();

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: AttackStart."));
}

void AActionPlayerCharacter::HandleAttackHitCheck()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: HitCheck."));

	const FVector AttackCenter = GetActorLocation() + GetActorForwardVector() * AttackHitCheckForwardOffset;
	DrawDebugSphere(GetWorld(), AttackCenter, AttackHitCheckRadius, 16, FColor::Red, false, 1.0f);

	TArray<AActor*> OverlappedActors;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	const bool bHitAnything = UKismetSystemLibrary::SphereOverlapActors(
		this,
		AttackCenter,
		AttackHitCheckRadius,
		ObjectTypes,
		AActionMonsterCharacter::StaticClass(),
		ActorsToIgnore,
		OverlappedActors);

	if (!bHitAnything)
	{
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: HitCheck found no monster in range."));
		return;
	}

	for (AActor* OverlappedActor : OverlappedActors)
	{
		AActionMonsterCharacter* MonsterCharacter = Cast<AActionMonsterCharacter>(OverlappedActor);
		if (MonsterCharacter == nullptr)
		{
			continue;
		}

		if (HitActorsThisAttack.Contains(MonsterCharacter))
		{
			UE_LOG(
				LogActionPlayerCharacter,
				Log,
				TEXT("ActionPlayerCharacter: Skip duplicate hit on %s in the same attack."),
				*GetNameSafe(MonsterCharacter));
			continue;
		}

		HitActorsThisAttack.Add(MonsterCharacter);

		MonsterCharacter->ApplyDamage(GetAttackPower());
		UE_LOG(
			LogActionPlayerCharacter,
			Log,
			TEXT("ActionPlayerCharacter: Hit monster %s for %.1f damage."),
			*GetNameSafe(MonsterCharacter),
			GetAttackPower());
	}
}

void AActionPlayerCharacter::EndAttackSequence()
{
	if (!bAttackInProgress)
	{
		return;
	}

	const bool bHitNotifyTriggered = bAttackHitTriggeredThisSequence;

	bAttackInProgress = false;
	bAttackHitTriggeredThisSequence = false;
	HitActorsThisAttack.Reset();

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: AttackEnd."));

	if (!bHitNotifyTriggered)
	{
		UE_LOG(
			LogActionPlayerCharacter,
			Warning,
			TEXT("ActionPlayerCharacter: No AttackHitCheck notify was received. Add a Montage Notify named 'AttackHitCheck' to the attack montage."));
	}

	if (InputBufferComponent != nullptr && InputBufferComponent->HasValidInput(ActionPlayerInputNames::Attack))
	{
		TryStartAttack();
	}
}

void AActionPlayerCharacter::OnAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (!bAttackInProgress)
	{
		return;
	}

	if (NotifyName != ActionPlayerNotifyNames::AttackHitCheck)
	{
		return;
	}

	if (bAttackHitTriggeredThisSequence)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: AttackHitCheck notify was triggered more than once in the same attack."));
		return;
	}

	bAttackHitTriggeredThisSequence = true;
	HandleAttackHitCheck();
}

void AActionPlayerCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage)
	{
		return;
	}

	if (bInterrupted)
	{
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Attack montage was interrupted."));
	}

	EndAttackSequence();
}
