#include "Char/ActionCharacterMovementComponent.h"
#include "GameFramework/Character.h"

FVector UActionCharacterMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	FVector Result = Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);

	if (!bEnableRootMotionZExtraction)
	{
		Result.Z = CurrentVelocity.Z;
	}

	Result.Z *= RootMotionZScale;
	return Result;
}

void UActionCharacterMovementComponent::RequestAttackComboTurn(float TargetWorldYaw, float MaxYawSpeedDeg)
{
	bAttackComboTurnActive = true;
	AttackComboTurnTargetYaw = FRotator::NormalizeAxis(TargetWorldYaw);
	AttackComboTurnMaxYawSpeedDeg = MaxYawSpeedDeg;

	// 瞬时到位的特殊路径：直接 set 一次就行，不用走 PhysicsRotation。
	if (MaxYawSpeedDeg <= 0.0f)
	{
		if (ACharacter* Owner = GetCharacterOwner())
		{
			FRotator NewRot = Owner->GetActorRotation();
			NewRot.Yaw = AttackComboTurnTargetYaw;
			Owner->SetActorRotation(NewRot);
		}
		bAttackComboTurnActive = false;
	}
}

void UActionCharacterMovementComponent::ClearAttackComboTurn()
{
	bAttackComboTurnActive = false;
}

void UActionCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	// 优先让 AttackComboTurn 处理；
	// 注意：本 override 在 RootMotion 应用之后被调用（UE 默认顺序），
	// 因此我们的 Yaw 写入会"压在"RootMotion 旋转之上，覆盖逻辑只发生一次/帧，
	// 不会再出现 Timer vs RootMotion 互相打架的情况。
	if (bAttackComboTurnActive)
	{
		if (ACharacter* Owner = GetCharacterOwner())
		{
			const FRotator Current = Owner->GetActorRotation();
			const float NewYaw = StepYawTowards(Current.Yaw, AttackComboTurnTargetYaw, DeltaTime);

			FRotator NewRot = Current;
			NewRot.Yaw = NewYaw;
			Owner->SetActorRotation(NewRot);

			// 到达目标后自动结束。
			if (FMath::IsNearlyEqual(NewYaw, AttackComboTurnTargetYaw, 0.1f))
			{
				bAttackComboTurnActive = false;
			}
		}
		else
		{
			bAttackComboTurnActive = false;
		}
		return;
	}

	Super::PhysicsRotation(DeltaTime);
}

float UActionCharacterMovementComponent::StepYawTowards(float CurrentYaw, float TargetYaw, float DeltaTime) const
{
	const float DeltaToTarget = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
	const float MaxStep = AttackComboTurnMaxYawSpeedDeg * DeltaTime;

	if (FMath::Abs(DeltaToTarget) <= MaxStep)
	{
		return TargetYaw;
	}

	const float Sign = DeltaToTarget >= 0.0f ? 1.0f : -1.0f;
	return FRotator::NormalizeAxis(CurrentYaw + Sign * MaxStep);
}
