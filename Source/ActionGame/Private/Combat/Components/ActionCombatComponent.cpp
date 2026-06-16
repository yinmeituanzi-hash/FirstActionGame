#include "Combat/Components/ActionCombatComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UActionCombatComponent::UActionCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UActionCombatComponent::StopAttackMontageForComboTransition(UAnimInstance* AnimInstance, UAnimMontage* PreviousAttackMontage)
{
	if (AnimInstance == nullptr || PreviousAttackMontage == nullptr || !AnimInstance->Montage_IsPlaying(PreviousAttackMontage))
	{
		return false;
	}

	AnimInstance->Montage_Stop(AttackComboMontageBlendOutTime, PreviousAttackMontage);
	return true;
}

void UActionCombatComponent::ApplyAttackTurnToWorldYaw(ACharacter* Character, float TargetWorldYaw)
{
	if (Character == nullptr)
	{
		return;
	}

	// 转向仍交给 MovementComponent，避免技能层直接 SetActorRotation 和 RootMotion 打架。
	UActionCharacterMovementComponent* ActionMove = Cast<UActionCharacterMovementComponent>(Character->GetCharacterMovement());
	if (ActionMove != nullptr)
	{
		ActionMove->RequestAttackComboTurn(TargetWorldYaw, AttackTurnMaxYawSpeedDeg);
	}
	else
	{
		FRotator NewRot = Character->GetActorRotation();
		NewRot.Yaw = TargetWorldYaw;
		Character->SetActorRotation(NewRot);
	}
}

void UActionCombatComponent::ApplyAttackTurnAtComboStart(ACharacter* Character, const FVector2D& LastMoveInput)
{
	if (Character == nullptr || Character->GetController() == nullptr || AttackTurnMaxDegrees <= 0.0f)
	{
		return;
	}

	const FVector2D Input2D = LastMoveInput.GetSafeNormal();
	if (Input2D.IsNearlyZero())
	{
		return;
	}

	// 把 WSAD 转成"相对相机的世界方向"。
	const FRotator ControlRotation = Character->GetController()->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector DesiredDirection = (ForwardDirection * Input2D.Y + RightDirection * Input2D.X).GetSafeNormal2D();
	if (DesiredDirection.IsNearlyZero())
	{
		return;
	}

	const float CurrentYaw = Character->GetActorRotation().Yaw;
	const float DesiredYaw = DesiredDirection.Rotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
	const float ClampedDeltaYaw = FMath::Clamp(DeltaYaw, -AttackTurnMaxDegrees, AttackTurnMaxDegrees);
	const float TargetYaw = CurrentYaw + ClampedDeltaYaw;

	// 关键修复：不再用 60Hz Timer 调 SetActorRotation，
	// 而是把目标 Yaw 交给 ActionCharacterMovementComponent，在 PhysicsRotation 内与 RootMotion 同管线推进。
	UActionCharacterMovementComponent* ActionMove = Cast<UActionCharacterMovementComponent>(Character->GetCharacterMovement());
	if (ActionMove != nullptr)
	{
		ActionMove->RequestAttackComboTurn(TargetYaw, AttackTurnMaxYawSpeedDeg);
	}
	else
	{
		// 兜底：如果不是 ActionCharacterMovementComponent，就保持瞬时转向（仍然不抖，因为只发生一次）。
		FRotator NewRot = Character->GetActorRotation();
		NewRot.Yaw = TargetYaw;
		Character->SetActorRotation(NewRot);
	}
}

void UActionCombatComponent::ClearAttackTurn(ACharacter* Character)
{
	if (Character == nullptr)
	{
		return;
	}
	if (UActionCharacterMovementComponent* ActionMove = Cast<UActionCharacterMovementComponent>(Character->GetCharacterMovement()))
	{
		ActionMove->ClearAttackComboTurn();
	}
}
