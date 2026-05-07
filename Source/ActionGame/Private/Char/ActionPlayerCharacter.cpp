#include "Char/ActionPlayerCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Combat/ActionFeatures/ActionFeatureBase.h"
#include "Combat/ActionFeatures/AttackFeature.h"
#include "Combat/ActionFeatures/DodgeFeature.h"
#include "Combat/ActionFeatures/NormalJumpFeature.h"
#include "Combat/Components/ActionCombatComponent.h"
#include "Combat/LockOn/ActionLockableInterface.h"
#include "Combat/LockOn/LockOnComponent.h"
#include "Common/ActionGameplayTags.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/InputBufferComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Logging/LogMacros.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionPlayerCharacter, Log, All);

namespace ActionPlayerInputNames
{
	static const FName Attack = TEXT("Attack");
	static const FName Dodge = TEXT("Dodge");
	static const FName Jump = TEXT("Jump");
}

AActionPlayerCharacter::AActionPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// ---------- 相机 ----------
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<UInputAction> MoveActionFinder(
		TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
	if (MoveActionFinder.Succeeded())
	{
		MoveAction = MoveActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> LookActionFinder(
		TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
	if (LookActionFinder.Succeeded())
	{
		LookAction = LookActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> JumpActionFinder(
		TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
	if (JumpActionFinder.Succeeded())
	{
		JumpAction = JumpActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> AttackActionFinder(
		TEXT("/Game/Input/Actions/IA_Attack.IA_Attack"));
	if (AttackActionFinder.Succeeded())
	{
		AttackAction = AttackActionFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> DodgeActionFinder(
		TEXT("/Game/Input/Actions/IA_Dodge.IA_Dodge"));
	if (DodgeActionFinder.Succeeded())
	{
		DodgeAction = DodgeActionFinder.Object;
	}

	// LockOn 输入资源（蓝图可覆盖）。如未提供请在 BP_ActionPlayerCharacter 上指定 LockOnAction。
	static ConstructorHelpers::FObjectFinder<UInputAction> LockOnActionFinder(
		TEXT("/Game/Input/Actions/IA_LockOn.IA_LockOn"));
	if (LockOnActionFinder.Succeeded())
	{
		LockOnAction = LockOnActionFinder.Object;
	}

	// ---------- 子组件 ----------
	InputBufferComponent = CreateDefaultSubobject<UInputBufferComponent>(TEXT("InputBufferComponent"));
	ActionCombatComponent = CreateDefaultSubobject<UActionCombatComponent>(TEXT("ActionCombatComponent"));
	LockOnComponent = CreateDefaultSubobject<ULockOnComponent>(TEXT("LockOnComponent"));

	// ---------- Feature 默认子类（蓝图可覆盖）----------
	AttackFeatureClass = UAttackFeature::StaticClass();
	DodgeFeatureClass = UDodgeFeature::StaticClass();
	JumpFeatureClass = UNormalJumpFeature::StaticClass();
}

bool AActionPlayerCharacter::CanAttack() const
{
	return Super::CanAttack();
}

bool AActionPlayerCharacter::CanDodge() const
{
	if (IsDead() || HasActionTag(ActionGameplayTags::Block_Dodge))
	{
		return false;
	}
	return DodgeFeature != nullptr ? DodgeFeature->HasAvailableCharge() : false;
}

void AActionPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion = true;
	}

	BuildRuntimeCombatInputMapping();

	if (UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	}

	if (ActionCombatComponent != nullptr)
	{
		ActionCombatComponent->InitializeDodgeCharges();
	}

	InitializeFeatures();
	BindMontageNotifyDelegateIfNeeded();

	// Enhanced Input MappingContext 注册。
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
			{
				if (RuntimeCombatMappingContext != nullptr)
				{
					Subsystem->AddMappingContext(RuntimeCombatMappingContext, 0);
				}
			}
		}
	}
}

void AActionPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 1) 给需要的 Feature 转发 Tick。
	for (UActionFeatureBase* Feature : Features)
	{
		if (Feature != nullptr && Feature->bEnableTick && Feature->IsActive())
		{
			Feature->Tick(DeltaSeconds);
		}
	}

	// 2) 检测从地面到下落（非主动跳）的瞬间，让 JumpFeature 把第一段算消耗掉。
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		const bool bInAirNow = Movement->IsFalling();
		if (bInAirNow && !bWasInAirLastFrame && JumpFeature != nullptr && JumpFeature->GetJumpCount() == 0)
		{
			JumpFeature->NotifyFallingFromLedge();
		}
		bWasInAirLastFrame = bInAirNow;
	}
}

void AActionPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (JumpFeature != nullptr)
	{
		JumpFeature->NotifyLanded();
	}
}

void AActionPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInputComponent == nullptr)
	{
		return;
	}

	if (JumpAction != nullptr)
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnJumpInput);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AActionPlayerCharacter::OnJumpInputReleased);
	}

	if (MoveAction != nullptr)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AActionPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AActionPlayerCharacter::ClearMoveInput);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AActionPlayerCharacter::ClearMoveInput);
	}

	if (LookAction != nullptr)
	{
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AActionPlayerCharacter::Look);
	}

	if (AttackAction != nullptr)
	{
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnAttackInput);
	}

	if (DodgeAction != nullptr)
	{
		EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnDodgeInput);
	}

	if (LockOnAction != nullptr)
	{
		EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnLockOnInput);
	}
}

void AActionPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	LastMoveInput = MovementVector;

	if (!CanMove() || Controller == nullptr)
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

void AActionPlayerCharacter::ClearMoveInput()
{
	LastMoveInput = FVector2D::ZeroVector;
}

void AActionPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller == nullptr)
	{
		return;
	}

	// 锁定状态下：横向输入只转给 LockOnComponent 用于切换目标；纵向输入保留少量手动 Pitch。
	if (LockOnComponent != nullptr && LockOnComponent->IsLocked())
	{
		LockOnComponent->NotifyLookInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y * LockedLookPitchSensitivity);
		return;
	}

	AddControllerYawInput(LookAxisVector.X * LookYawSensitivity);
	AddControllerPitchInput(LookAxisVector.Y * LookPitchSensitivity);
}

void AActionPlayerCharacter::BuildRuntimeCombatInputMapping()
{
	if (RuntimeCombatMappingContext == nullptr || AttackAction == nullptr || DodgeAction == nullptr)
	{
		UE_LOG(
			LogActionPlayerCharacter,
			Log,
			TEXT("ActionPlayerCharacter: Combat input assets not fully assigned (this is OK if combat IMC is added in BP). Mapping=%s AttackAction=%s DodgeAction=%s"),
			*GetNameSafe(RuntimeCombatMappingContext),
			*GetNameSafe(AttackAction),
			*GetNameSafe(DodgeAction));
	}
}

// =============================================================================
// State transitions
// =============================================================================

void AActionPlayerCharacter::OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState)
{
	Super::OnActionStateExit(OldState, NewState);

	if (OldState == EActionCharacterState::Attacking)
	{
		RemoveActionTag(ActionGameplayTags::Block_Attack);
		RemoveActionTag(ActionGameplayTags::Block_Dodge);
		RemoveActionTag(ActionGameplayTags::Block_Move);
		RemoveActionTag(ActionGameplayTags::Window_Attack_CanDodgeCancel);
		RemoveActionTag(ActionGameplayTags::Window_Attack_CanCombo);
		RemoveActionTag(ActionGameplayTags::Window_Attack_CanTurn);
	}

	if (OldState == EActionCharacterState::Dodging)
	{
		RemoveActionTag(ActionGameplayTags::Block_Attack);
		RemoveActionTag(ActionGameplayTags::Block_Dodge);
		RemoveActionTag(ActionGameplayTags::Block_Move);
		RemoveActionTag(ActionGameplayTags::Window_Dodge_CanRecover);
	}
}

void AActionPlayerCharacter::OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState)
{
	Super::OnActionStateEnter(OldState, NewState);

	if (NewState == EActionCharacterState::Attacking)
	{
		AddActionTag(ActionGameplayTags::Block_Attack);
		AddActionTag(ActionGameplayTags::Block_Dodge);
		AddActionTag(ActionGameplayTags::Block_Move);
	}
	if (NewState == EActionCharacterState::Dodging)
	{
		AddActionTag(ActionGameplayTags::Block_Attack);
		AddActionTag(ActionGameplayTags::Block_Dodge);
		AddActionTag(ActionGameplayTags::Block_Move);
	}
}

EActionCharacterState AActionPlayerCharacter::ResolveDefaultActionState() const
{
	if (IsDead())
	{
		return EActionCharacterState::Dead;
	}
	if (HasActionTag(ActionGameplayTags::State_Action_HitReact))
	{
		return EActionCharacterState::HitReact;
	}
	return EActionCharacterState::Idle;
}

void AActionPlayerCharacter::RequestActionState(EActionCharacterState InState)
{
	if (InState == EActionCharacterState::Idle)
	{
		SetActionState(ResolveDefaultActionState());
	}
	else
	{
		SetActionState(InState);
	}
}

// =============================================================================
// Inputs → Features
// =============================================================================

void AActionPlayerCharacter::OnAttackInput()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: OnAttackInput."));

	if (InputBufferComponent != nullptr)
	{
		InputBufferComponent->PushInput(ActionPlayerInputNames::Attack, InputBufferLifetime);
	}

	if (AttackFeature == nullptr)
	{
		return;
	}

	// 已经在攻击中：尝试连段。
	if (AttackFeature->IsActive())
	{
		if (AttackFeature->TryAdvanceCombo())
		{
			InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Attack);
		}
		return;
	}

	// 未在攻击中：尝试启动第一段。
	if (AttackFeature->CanExecute())
	{
		InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Attack);
		AttackFeature->Execute();
	}
}

void AActionPlayerCharacter::OnDodgeInput()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: OnDodgeInput."));

	if (InputBufferComponent != nullptr)
	{
		InputBufferComponent->PushInput(ActionPlayerInputNames::Dodge, InputBufferLifetime);
	}

	if (DodgeFeature == nullptr)
	{
		return;
	}

	if (DodgeFeature->CanExecute())
	{
		InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Dodge);
		DodgeFeature->Execute();
	}
}

void AActionPlayerCharacter::OnJumpInput()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: OnJumpInput."));

	if (InputBufferComponent != nullptr)
	{
		InputBufferComponent->PushInput(ActionPlayerInputNames::Jump, InputBufferLifetime);
	}

	if (JumpFeature != nullptr && JumpFeature->CanExecute())
	{
		InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Jump);
		JumpFeature->Execute();
	}
	else
	{
		// 兜底：让 ACharacter 默认跳跃也工作（如果没配 JumpFeature 子类）。
		Jump();
	}
}

void AActionPlayerCharacter::OnJumpInputReleased()
{
	StopJumping();
}

//待仔细阅读
void AActionPlayerCharacter::OnLockOnInput()
{
	if (LockOnComponent != nullptr)
	{
		LockOnComponent->ToggleLockOn();
	}
}

FVector AActionPlayerCharacter::GetLockOnTargetLocation() const
{
	if (LockOnComponent == nullptr || !LockOnComponent->IsLocked())
	{
		return FVector::ZeroVector;
	}
	AActor* Target = LockOnComponent->GetCurrentTarget();
	if (Target == nullptr)
	{
		return FVector::ZeroVector;
	}
	if (Target->GetClass()->ImplementsInterface(UActionLockableInterface::StaticClass()))
	{
		const FVector InterfaceLoc = IActionLockableInterface::Execute_GetLockOnTargetLocation(Target);
		if (!InterfaceLoc.IsZero())
		{
			return InterfaceLoc;
		}
	}
	return Target->GetActorLocation();
}

bool AActionPlayerCharacter::HasLockOnTarget() const
{
	return LockOnComponent != nullptr && LockOnComponent->IsLocked();
}

// =============================================================================
// Feature management
// =============================================================================

void AActionPlayerCharacter::InitializeFeatures()
{
	Features.Reset();

	if (AttackFeatureClass != nullptr)
	{
		AttackFeature = Cast<UAttackFeature>(CreateFeatureInstance(AttackFeatureClass));
		if (AttackFeature != nullptr) { Features.Add(AttackFeature); }
	}

	if (DodgeFeatureClass != nullptr)
	{
		DodgeFeature = Cast<UDodgeFeature>(CreateFeatureInstance(DodgeFeatureClass));
		if (DodgeFeature != nullptr) { Features.Add(DodgeFeature); }
	}

	if (JumpFeatureClass != nullptr)
	{
		JumpFeature = Cast<UNormalJumpFeature>(CreateFeatureInstance(JumpFeatureClass));
		if (JumpFeature != nullptr) { Features.Add(JumpFeature); }
	}

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: %d feature(s) initialized."), Features.Num());
}

UActionFeatureBase* AActionPlayerCharacter::CreateFeatureInstance(TSubclassOf<UActionFeatureBase> FeatureClass)
{
	if (FeatureClass == nullptr)
	{
		return nullptr;
	}
	UActionFeatureBase* NewFeature = NewObject<UActionFeatureBase>(this, FeatureClass, NAME_None, RF_Transactional);
	if (NewFeature != nullptr)
	{
		NewFeature->Initialize(this);
	}
	return NewFeature;
}

UActionFeatureBase* AActionPlayerCharacter::GetFeatureByClass(TSubclassOf<UActionFeatureBase> FeatureClass) const
{
	if (FeatureClass == nullptr)
	{
		return nullptr;
	}
	for (UActionFeatureBase* Feature : Features)
	{
		if (Feature != nullptr && Feature->IsA(FeatureClass))
		{
			return Feature;
		}
	}
	return nullptr;
}

void AActionPlayerCharacter::SetCurrentActiveFeature(UActionFeatureBase* InFeature)
{
	CurrentActiveFeature = InFeature;
}

void AActionPlayerCharacter::ClearCurrentActiveFeature(UActionFeatureBase* InFeature)
{
	if (CurrentActiveFeature == InFeature)
	{
		CurrentActiveFeature = nullptr;
		// Feature 自然结束 → 回到 Idle/HitReact/Dead。
		SetActionState(ResolveDefaultActionState());

		// 缓存输入处理：让连段感觉更跟手。
		if (InputBufferComponent != nullptr)
		{
			if (InputBufferComponent->HasValidInput(ActionPlayerInputNames::Dodge) && DodgeFeature != nullptr && DodgeFeature->CanExecute())
			{
				InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Dodge);
				DodgeFeature->Execute();
				return;
			}
			if (InputBufferComponent->HasValidInput(ActionPlayerInputNames::Attack) && AttackFeature != nullptr && AttackFeature->CanExecute())
			{
				InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Attack);
				AttackFeature->Execute();
				return;
			}
		}
	}
}

// =============================================================================
// Montage notify dispatch
// =============================================================================

void AActionPlayerCharacter::BindMontageNotifyDelegateIfNeeded()
{
	if (bMontageNotifyDelegateBound)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr)
	{
		return;
	}

	AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AActionPlayerCharacter::OnAnyMontageNotifyBegin);
	bMontageNotifyDelegateBound = true;
}

void AActionPlayerCharacter::OnAnyMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	// 仅转发给当前激活的 Feature。Feature 内部用 ActiveMontage 校验只处理自己的 Notify。
	if (CurrentActiveFeature != nullptr)
	{
		CurrentActiveFeature->OnNotify(NotifyName, BranchingPointPayload);
	}
}

// =============================================================================
// Misc public API
// =============================================================================

int32 AActionPlayerCharacter::GetCurrentDodgeCharges() const
{
	if (DodgeFeature != nullptr)
	{
		return DodgeFeature->GetCurrentCharges();
	}
	return ActionCombatComponent != nullptr ? ActionCombatComponent->GetCurrentDodgeCharges() : 0;
}
