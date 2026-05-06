#include "Combat/Components/ActionCombatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

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

	StartAttackTurnInterpolation(Character, CurrentYaw + ClampedDeltaYaw);
}

void UActionCombatComponent::ClearAttackTurnInterpolation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTurnInterpolationTimerHandle);
	}

	AttackTurnCharacter.Reset();
	AttackTurnInterpolationElapsed = 0.0f;
}

void UActionCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAttackTurnInterpolation();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DodgeChargeRestoreTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UActionCombatComponent::StartAttackTurnInterpolation(ACharacter* Character, float TargetYaw)
{
	ClearAttackTurnInterpolation();

	if (Character == nullptr)
	{
		return;
	}

	AttackTurnCharacter = Character;
	AttackTurnStartRotation = Character->GetActorRotation();
	AttackTurnTargetRotation = FRotator(0.0f, TargetYaw, 0.0f);
	AttackTurnInterpolationElapsed = 0.0f;

	if (AttackTurnInterpDuration <= KINDA_SMALL_NUMBER || GetWorld() == nullptr)
	{
		Character->SetActorRotation(AttackTurnTargetRotation);
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AttackTurnInterpolationTimerHandle,
		this,
		&UActionCombatComponent::UpdateAttackTurnInterpolation,
		1.0f / 60.0f,
		true);
}

void UActionCombatComponent::UpdateAttackTurnInterpolation()
{
	ACharacter* Character = AttackTurnCharacter.Get();
	if (Character == nullptr)
	{
		ClearAttackTurnInterpolation();
		return;
	}

	AttackTurnInterpolationElapsed += 1.0f / 60.0f;
	const float Alpha = FMath::Clamp(AttackTurnInterpolationElapsed / FMath::Max(AttackTurnInterpDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(AttackTurnStartRotation.Yaw, AttackTurnTargetRotation.Yaw);
	const float NewYaw = AttackTurnStartRotation.Yaw + DeltaYaw * Alpha;
	Character->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));

	if (Alpha >= 1.0f)
	{
		ClearAttackTurnInterpolation();
	}
}

void UActionCombatComponent::InitializeDodgeCharges()
{
	CurrentDodgeCharges = FMath::Max(1, MaxDodgeCharges);
}

bool UActionCombatComponent::HasAvailableDodgeCharge() const
{
	return CurrentDodgeCharges > 0;
}

void UActionCombatComponent::ConsumeDodgeCharge()
{
	const int32 EffectiveMaxCharges = FMath::Max(1, MaxDodgeCharges);
	CurrentDodgeCharges = FMath::Clamp(CurrentDodgeCharges - 1, 0, EffectiveMaxCharges);

	if (CurrentDodgeCharges < EffectiveMaxCharges && GetWorld() != nullptr && !GetWorld()->GetTimerManager().IsTimerActive(DodgeChargeRestoreTimerHandle))
	{
		const float RestoreDelay = DodgeChargeCooldown > 0.0f ? DodgeChargeCooldown : KINDA_SMALL_NUMBER;
		GetWorld()->GetTimerManager().SetTimer(
			DodgeChargeRestoreTimerHandle,
			this,
			&UActionCombatComponent::RestoreDodgeCharge,
			RestoreDelay,
			false);
	}
}

void UActionCombatComponent::RestoreDodgeCharge()
{
	const int32 EffectiveMaxCharges = FMath::Max(1, MaxDodgeCharges);
	CurrentDodgeCharges = FMath::Clamp(CurrentDodgeCharges + 1, 0, EffectiveMaxCharges);

	if (CurrentDodgeCharges < EffectiveMaxCharges && GetWorld() != nullptr)
	{
		const float RestoreDelay = DodgeChargeCooldown > 0.0f ? DodgeChargeCooldown : KINDA_SMALL_NUMBER;
		GetWorld()->GetTimerManager().SetTimer(
			DodgeChargeRestoreTimerHandle,
			this,
			&UActionCombatComponent::RestoreDodgeCharge,
			RestoreDelay,
			false);
	}
}
