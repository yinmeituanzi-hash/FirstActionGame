#include "Char/ActionPlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "Common/ActionGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/InputBufferComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Logging/LogMacros.h"
#include "TimerManager.h"
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
	static const FName AttackDodgeCancelStart = TEXT("AttackDodgeCancelStart");
	static const FName AttackComboWindowStart = TEXT("AttackComboWindowStart");
	static const FName AttackTurnWindowStart = TEXT("AttackTurnWindowStart");
	static const FName DodgeRecoveryStart = TEXT("DodgeRecoveryStart");
}

AActionPlayerCharacter::AActionPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

	static ConstructorHelpers::FObjectFinder<UInputMappingContext> CombatMappingContextFinder(
		TEXT("/Game/Input/Contexts/IMC_PlayerCombat.IMC_PlayerCombat"));
	if (CombatMappingContextFinder.Succeeded())
	{
		RuntimeCombatMappingContext = CombatMappingContextFinder.Object;
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
	return Super::CanAttack();
}

bool AActionPlayerCharacter::CanDodge() const
{
	return !IsDead()
		&& !HasActionTag(ActionGameplayTags::Block_Dodge)
		&& HasAvailableDodgeCharge();
}

void AActionPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (GetCharacterMovement() != nullptr)
	{
		GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = true;
	}

	BuildRuntimeCombatInputMapping();

	if (UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	}

	CurrentDodgeCharges = FMath::Max(1, MaxDodgeCharges);

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
		}
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
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AActionPlayerCharacter::ClearMoveInput);
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AActionPlayerCharacter::ClearMoveInput);
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
}

void AActionPlayerCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	LastMoveInput = MovementVector;

	if (!CanMove())
	{
		return;
	}

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
}

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

void AActionPlayerCharacter::OnAttackInput()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: OnAttackInput called."));
	InputBufferComponent->PushInput(ActionPlayerInputNames::Attack, InputBufferLifetime);
	TryStartAttack();
}

void AActionPlayerCharacter::OnDodgeInput()
{
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: OnDodgeInput called."));

	if (!HasAvailableDodgeCharge())
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: Dodge input ignored because no dodge charges are available."));
		return;
	}

	InputBufferComponent->PushInput(ActionPlayerInputNames::Dodge, InputBufferLifetime);
	TryStartDodge();
}

void AActionPlayerCharacter::TryStartAttack()
{
	if (IsInActionState(EActionCharacterState::Attacking))
	{
		if (HasActionTag(ActionGameplayTags::Window_Attack_CanCombo) && TryConsumeAttackInput())
		{
			TryStartNextComboAttack();
		}
		else
		{
			UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Attack input buffered while combo window is closed."));
		}

		return;
	}

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

	StartAttackComboAtIndex(0);
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

bool AActionPlayerCharacter::TryConsumeDodgeInput()
{
	if (InputBufferComponent == nullptr)
	{
		return false;
	}

	const bool bConsumed = InputBufferComponent->ConsumeInput(ActionPlayerInputNames::Dodge);
	if (bConsumed)
	{
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Dodge input consumed from buffer."));
	}

	return bConsumed;
}

bool AActionPlayerCharacter::StartAttackComboAtIndex(int32 ComboIndex)
{
	UAnimMontage* PreviousAttackMontage = ActiveAttackMontage;
	UAnimMontage* SelectedAttackMontage = GetAttackMontageForComboIndex(ComboIndex);
	if (SelectedAttackMontage == nullptr)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: No attack montage for combo index %d."), ComboIndex);
		return false;
	}

	UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: AnimInstance was not found on character mesh."));
		return false;
	}

	if (PreviousAttackMontage != nullptr && AnimInstance->Montage_IsPlaying(PreviousAttackMontage))
	{
		AnimInstance->Montage_Stop(0.12f, PreviousAttackMontage);
	}

	if (ComboIndex > 0 && HasActionTag(ActionGameplayTags::Window_Attack_CanTurn))
	{
		ApplyAttackTurnAtComboStart();
	}

	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &AActionPlayerCharacter::OnMontageNotifyBegin);
	AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AActionPlayerCharacter::OnMontageNotifyBegin);

	const float PlayedLength = PlayAnimMontage(SelectedAttackMontage);
	if (PlayedLength <= 0.0f)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: Failed to play attack combo montage %d."), ComboIndex);
		return false;
	}

	ActiveAttackMontage = SelectedAttackMontage;

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AActionPlayerCharacter::OnAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, SelectedAttackMontage);

	BeginAttackSequence(ComboIndex);
	return true;
}

UAnimMontage* AActionPlayerCharacter::GetAttackMontageForComboIndex(int32 ComboIndex) const
{
	if (AttackComboMontages.IsValidIndex(ComboIndex) && AttackComboMontages[ComboIndex] != nullptr)
	{
		return AttackComboMontages[ComboIndex];
	}

	return ComboIndex == 0 ? AttackMontage : nullptr;
}

int32 AActionPlayerCharacter::GetNextAttackComboIndex() const
{
	const int32 NextComboIndex = CurrentAttackComboIndex + 1;
	return GetAttackMontageForComboIndex(NextComboIndex) != nullptr ? NextComboIndex : INDEX_NONE;
}

bool AActionPlayerCharacter::TryStartNextComboAttack()
{
	const int32 NextComboIndex = GetNextAttackComboIndex();
	if (NextComboIndex == INDEX_NONE)
	{
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Combo input ignored because there is no next combo montage."));
		return false;
	}

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Starting next combo attack immediately."));
	return StartAttackComboAtIndex(NextComboIndex);
}

void AActionPlayerCharacter::OpenAttackTurnWindow()
{
	AddActionTag(ActionGameplayTags::Window_Attack_CanTurn);
}

void AActionPlayerCharacter::ApplyAttackTurnAtComboStart()
{
	const FVector2D Input2D = LastMoveInput.GetSafeNormal();
	if (Input2D.IsNearlyZero() || Controller == nullptr || AttackTurnMaxDegrees <= 0.0f)
	{
		return;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector DesiredDirection = (ForwardDirection * Input2D.Y + RightDirection * Input2D.X).GetSafeNormal2D();
	if (DesiredDirection.IsNearlyZero())
	{
		return;
	}

	const float CurrentYaw = GetActorRotation().Yaw;
	const float DesiredYaw = DesiredDirection.Rotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
	const float ClampedDeltaYaw = FMath::Clamp(DeltaYaw, -AttackTurnMaxDegrees, AttackTurnMaxDegrees);

	StartAttackTurnInterpolation(CurrentYaw + ClampedDeltaYaw);
	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Attack combo turn consumed %.1f yaw."), ClampedDeltaYaw);
}

void AActionPlayerCharacter::StartAttackTurnInterpolation(float TargetYaw)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTurnInterpolationTimerHandle);
	}

	AttackTurnStartRotation = GetActorRotation();
	AttackTurnTargetRotation = FRotator(0.0f, TargetYaw, 0.0f);
	AttackTurnInterpolationElapsed = 0.0f;

	if (AttackTurnInterpDuration <= KINDA_SMALL_NUMBER || GetWorld() == nullptr)
	{
		SetActorRotation(AttackTurnTargetRotation);
		return;
	}

	GetWorldTimerManager().SetTimer(
		AttackTurnInterpolationTimerHandle,
		this,
		&AActionPlayerCharacter::UpdateAttackTurnInterpolation,
		1.0f / 60.0f,
		true);
}

void AActionPlayerCharacter::UpdateAttackTurnInterpolation()
{
	AttackTurnInterpolationElapsed += 1.0f / 60.0f;
	const float Alpha = FMath::Clamp(AttackTurnInterpolationElapsed / FMath::Max(AttackTurnInterpDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(AttackTurnStartRotation.Yaw, AttackTurnTargetRotation.Yaw);
	const float NewYaw = AttackTurnStartRotation.Yaw + DeltaYaw * Alpha;
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));

	if (Alpha >= 1.0f)
	{
		GetWorldTimerManager().ClearTimer(AttackTurnInterpolationTimerHandle);
	}
}

void AActionPlayerCharacter::BeginAttackSequence(int32 ComboIndex)
{
	bAttackInProgress = true;
	bAttackHitTriggeredThisSequence = false;
	CurrentAttackComboIndex = ComboIndex;
	HitActorsThisAttack.Reset();
	RemoveActionTag(ActionGameplayTags::Window_Attack_CanCombo);
	RemoveActionTag(ActionGameplayTags::Window_Attack_CanTurn);
	RemoveActionTag(ActionGameplayTags::Window_Attack_CanDodgeCancel);
	AddActionTag(ActionGameplayTags::Block_Attack);
	AddActionTag(ActionGameplayTags::Block_Dodge);
	AddActionTag(ActionGameplayTags::Block_Move);
	SetActionState(EActionCharacterState::Attacking);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: AttackStart combo index %d."), ComboIndex);
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
	CurrentAttackComboIndex = INDEX_NONE;
	ActiveAttackMontage = nullptr;
	HitActorsThisAttack.Reset();

	if (IsDead())
	{
		SetActionState(EActionCharacterState::Dead);
	}
	else if (IsInActionState(EActionCharacterState::Attacking))
	{
		SetActionState(EActionCharacterState::Idle);
	}

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: AttackEnd."));

	if (!bHitNotifyTriggered)
	{
		UE_LOG(
			LogActionPlayerCharacter,
			Warning,
			TEXT("ActionPlayerCharacter: No AttackHitCheck notify was received. Add a Montage Notify named 'AttackHitCheck' to the attack montage."));
	}

	if (InputBufferComponent != nullptr && InputBufferComponent->HasValidInput(ActionPlayerInputNames::Dodge))
	{
		TryStartDodge();
		return;
	}

	if (InputBufferComponent != nullptr && InputBufferComponent->HasValidInput(ActionPlayerInputNames::Attack))
	{
		TryStartAttack();
	}
}

void AActionPlayerCharacter::TryStartDodge()
{
	if (!CanDodge())
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: TryStartDodge blocked because character cannot dodge."));
		return;
	}

	if (!TryConsumeDodgeInput())
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: TryStartDodge found no buffered dodge input."));
		return;
	}

	UAnimMontage* SelectedDodgeMontage = SelectDodgeMontage();
	if (SelectedDodgeMontage == nullptr)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: No dodge montage is set for the current direction."));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance == nullptr)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: AnimInstance was not found on character mesh."));
		return;
	}

	AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &AActionPlayerCharacter::OnMontageNotifyBegin);
	AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AActionPlayerCharacter::OnMontageNotifyBegin);

	const float PlayedLength = PlayAnimMontage(SelectedDodgeMontage);
	if (PlayedLength <= 0.0f)
	{
		UE_LOG(LogActionPlayerCharacter, Warning, TEXT("ActionPlayerCharacter: Failed to play dodge montage."));
		return;
	}

	ActiveDodgeMontage = SelectedDodgeMontage;

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AActionPlayerCharacter::OnDodgeMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, SelectedDodgeMontage);

	BeginDodgeSequence();
}

void AActionPlayerCharacter::BeginDodgeSequence()
{
	bDodgeInProgress = true;
	ConsumeDodgeCharge();
	SetActionState(EActionCharacterState::Dodging);

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: DodgeStart."));
}

void AActionPlayerCharacter::EndDodgeSequence()
{
	if (!bDodgeInProgress)
	{
		return;
	}

	bDodgeInProgress = false;
	ActiveDodgeMontage = nullptr;
	SetActionState(IsDead() ? EActionCharacterState::Dead : EActionCharacterState::Idle);

	UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: DodgeEnd."));

	if (InputBufferComponent != nullptr && InputBufferComponent->HasValidInput(ActionPlayerInputNames::Dodge))
	{
		TryStartDodge();
		return;
	}

	if (InputBufferComponent != nullptr && InputBufferComponent->HasValidInput(ActionPlayerInputNames::Attack))
	{
		TryStartAttack();
	}
}

void AActionPlayerCharacter::ConsumeDodgeCharge()
{
	const int32 EffectiveMaxCharges = FMath::Max(1, MaxDodgeCharges);
	CurrentDodgeCharges = FMath::Clamp(CurrentDodgeCharges - 1, 0, EffectiveMaxCharges);

	UE_LOG(
		LogActionPlayerCharacter,
		Log,
		TEXT("ActionPlayerCharacter: Dodge charge consumed. %d / %d remaining."),
		CurrentDodgeCharges,
		EffectiveMaxCharges);

	if (CurrentDodgeCharges < EffectiveMaxCharges && !GetWorldTimerManager().IsTimerActive(DodgeChargeRestoreTimerHandle))
	{
		const float RestoreDelay = DodgeChargeCooldown > 0.0f ? DodgeChargeCooldown : KINDA_SMALL_NUMBER;
		GetWorldTimerManager().SetTimer(
			DodgeChargeRestoreTimerHandle,
			this,
			&AActionPlayerCharacter::RestoreDodgeCharge,
			RestoreDelay,
			false);
	}
}

void AActionPlayerCharacter::RestoreDodgeCharge()
{
	const int32 EffectiveMaxCharges = FMath::Max(1, MaxDodgeCharges);
	CurrentDodgeCharges = FMath::Clamp(CurrentDodgeCharges + 1, 0, EffectiveMaxCharges);

	UE_LOG(
		LogActionPlayerCharacter,
		Log,
		TEXT("ActionPlayerCharacter: Dodge charge restored. %d / %d available."),
		CurrentDodgeCharges,
		EffectiveMaxCharges);

	if (CurrentDodgeCharges < EffectiveMaxCharges)
	{
		const float RestoreDelay = DodgeChargeCooldown > 0.0f ? DodgeChargeCooldown : KINDA_SMALL_NUMBER;
		GetWorldTimerManager().SetTimer(
			DodgeChargeRestoreTimerHandle,
			this,
			&AActionPlayerCharacter::RestoreDodgeCharge,
			RestoreDelay,
			false);
	}
}

bool AActionPlayerCharacter::HasAvailableDodgeCharge() const
{
	return CurrentDodgeCharges > 0;
}

FVector AActionPlayerCharacter::ResolveDodgeDirection() const
{
	FVector DodgeDirection = GetVelocity();
	DodgeDirection.Z = 0.0f;

	if (DodgeDirection.Normalize())
	{
		return DodgeDirection;
	}

	const FVector2D Input2D = LastMoveInput.GetSafeNormal();
	if (!Input2D.IsNearlyZero() && Controller != nullptr)
	{
		const FRotator ControlRotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		DodgeDirection = ForwardDirection * Input2D.Y + RightDirection * Input2D.X;
		DodgeDirection.Z = 0.0f;
		DodgeDirection.Normalize();
		return DodgeDirection;
	}

	return -GetActorForwardVector();
}

UAnimMontage* AActionPlayerCharacter::SelectDodgeMontage() const
{
	const FVector DodgeDirection = ResolveDodgeDirection();
	if (DodgeDirection.IsNearlyZero())
	{
		return DodgeBackwardMontage != nullptr ? DodgeBackwardMontage : DodgeMontage;
	}

	const FVector ActorForward = GetActorForwardVector().GetSafeNormal2D();
	const FVector ActorRight = GetActorRightVector().GetSafeNormal2D();
	const float ForwardDot = FVector::DotProduct(ActorForward, DodgeDirection);
	const float RightDot = FVector::DotProduct(ActorRight, DodgeDirection);

	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		return ForwardDot >= 0.0f
			? (DodgeForwardMontage != nullptr ? DodgeForwardMontage : DodgeMontage)
			: (DodgeBackwardMontage != nullptr ? DodgeBackwardMontage : DodgeMontage);
	}

	return RightDot >= 0.0f
		? (DodgeRightMontage != nullptr ? DodgeRightMontage : (DodgeForwardMontage != nullptr ? DodgeForwardMontage : DodgeMontage))
		: (DodgeLeftMontage != nullptr ? DodgeLeftMontage : (DodgeForwardMontage != nullptr ? DodgeForwardMontage : DodgeMontage));
}

void AActionPlayerCharacter::OnMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (NotifyName == ActionPlayerNotifyNames::AttackDodgeCancelStart && bAttackInProgress)
	{
		RemoveActionTag(ActionGameplayTags::Block_Dodge);
		AddActionTag(ActionGameplayTags::Window_Attack_CanDodgeCancel);
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Attack dodge cancel window opened."));

		if (InputBufferComponent != nullptr && InputBufferComponent->HasValidInput(ActionPlayerInputNames::Dodge))
		{
			TryStartDodge();
		}

		return;
	}

	if (NotifyName == ActionPlayerNotifyNames::AttackComboWindowStart && bAttackInProgress)
	{
		AddActionTag(ActionGameplayTags::Window_Attack_CanCombo);
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Attack combo window opened."));

		if (InputBufferComponent != nullptr && InputBufferComponent->HasValidInput(ActionPlayerInputNames::Attack) && TryConsumeAttackInput())
		{
			TryStartNextComboAttack();
		}

		return;
	}

	if (NotifyName == ActionPlayerNotifyNames::AttackTurnWindowStart && bAttackInProgress)
	{
		OpenAttackTurnWindow();
		return;
	}

	if (NotifyName == ActionPlayerNotifyNames::DodgeRecoveryStart && bDodgeInProgress)
	{
		RemoveActionTag(ActionGameplayTags::Block_Attack);
		RemoveActionTag(ActionGameplayTags::Block_Move);
		AddActionTag(ActionGameplayTags::Window_Dodge_CanRecover);
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Dodge recovery window opened."));
		EndDodgeSequence();
		return;
	}

	if (!bAttackInProgress || NotifyName != ActionPlayerNotifyNames::AttackHitCheck)
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
	if (Montage != ActiveAttackMontage)
	{
		return;
	}

	if (bInterrupted)
	{
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Attack montage was interrupted."));
	}

	EndAttackSequence();
}

void AActionPlayerCharacter::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActiveDodgeMontage)
	{
		return;
	}

	if (bInterrupted)
	{
		UE_LOG(LogActionPlayerCharacter, Log, TEXT("ActionPlayerCharacter: Dodge montage was interrupted."));
	}

	EndDodgeSequence();
}
