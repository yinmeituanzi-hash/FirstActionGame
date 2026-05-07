#include "Combat/ActionFeatures/DodgeFeature.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterMovementComponent.h"
#include "Char/ActionPlayerCharacter.h"
#include "Common/ActionGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDodgeFeature, Log, All);

namespace DodgeFeatureNotifies
{
	static const FName DodgeRecoveryStart = TEXT("DodgeRecoveryStart");
}

UDodgeFeature::UDodgeFeature()
{
	FeatureName = TEXT("Dodge");
	TargetState = EActionCharacterState::Dodging;
	bEnableTick = false;

	// 默认 BlockTags：进入闪避时禁止重叠触发。
	BlockTags.AddTag(ActionGameplayTags::Block_Dodge);
}

void UDodgeFeature::Initialize(AActionPlayerCharacter* InOwner)
{
	Super::Initialize(InOwner);
	CurrentCharges = MaxCharges;
}

bool UDodgeFeature::CanExecute() const
{
	if (!Super::CanExecute())
	{
		return false;
	}

	if (!HasAvailableCharge())
	{
		return false;
	}

	return true;
}

void UDodgeFeature::Execute()
{
	if (!CanExecute())
	{
		UE_LOG(LogDodgeFeature, Log, TEXT("DodgeFeature: Execute blocked by CanExecute."));
		return;
	}

	UAnimMontage* Selected = SelectMontageForCurrentInput();
	if (Selected == nullptr)
	{
		UE_LOG(LogDodgeFeature, Warning, TEXT("DodgeFeature: No dodge montage assigned."));
		return;
	}

	BeginActive();
	ConsumeCharge();
	StartChargeRestoreTimerIfNeeded();

	// 锁定状态下：闪避启动瞬间把角色 Yaw 对齐到目标方向，
	// 这样 4 方向蒙太奇的"前/后/左/右"语义对玩家来说是稳定的（始终相对锁定目标）。
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner != nullptr && Owner->HasLockOnTarget())
	{
		const FVector ToTarget = (Owner->GetLockOnTargetLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero())
		{
			FRotator NewRot = Owner->GetActorRotation();
			NewRot.Yaw = ToTarget.Rotation().Yaw;
			Owner->SetActorRotation(NewRot);
		}
	}

	const float Duration = PlayMontageInternal(Selected);
	if (Duration <= 0.0f)
	{
		UE_LOG(LogDodgeFeature, Warning, TEXT("DodgeFeature: Failed to play dodge montage %s."), *GetNameSafe(Selected));
		Stop(true);
	}
}

void UDodgeFeature::OnNotify(FName NotifyName, const FBranchingPointNotifyPayload& /*Payload*/)
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}

	if (NotifyName == DodgeFeatureNotifies::DodgeRecoveryStart)
	{
		// 进入恢复窗口：允许移动、攻击，标记 Window 让玩家可以衔接下一次闪避。
		Owner->RemoveActionTagExternal(ActionGameplayTags::Block_Move);
		Owner->RemoveActionTagExternal(ActionGameplayTags::Block_Attack);
		Owner->AddActionTagExternal(ActionGameplayTags::Window_Dodge_CanRecover);
		UE_LOG(LogDodgeFeature, Log, TEXT("DodgeFeature: DodgeRecoveryStart received."));
	}
}

void UDodgeFeature::OnMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
	UE_LOG(LogDodgeFeature, Log, TEXT("DodgeFeature: Montage ended (interrupted=%s)."), bInterrupted ? TEXT("true") : TEXT("false"));
	Stop(bInterrupted);
}

UAnimMontage* UDodgeFeature::SelectMontageForCurrentInput() const
{
	const AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return DodgeDefaultMontage;
	}

	const FVector2D Move = Owner->GetLastMoveInput();

	// 根据移动输入挑方向蒙太奇；近似 010 的 4 方向选择，简单 abs 比较。
	if (Move.IsNearlyZero(0.1f))
	{
		// 没有输入：优先后撤，没配的话用通用蒙太奇。
		if (DodgeBackwardMontage != nullptr) return DodgeBackwardMontage;
		if (DodgeDefaultMontage != nullptr) return DodgeDefaultMontage;
		return DodgeForwardMontage;
	}

	const float AbsX = FMath::Abs(Move.X);
	const float AbsY = FMath::Abs(Move.Y);
	if (AbsY >= AbsX)
	{
		if (Move.Y >= 0.0f && DodgeForwardMontage != nullptr) return DodgeForwardMontage;
		if (Move.Y < 0.0f && DodgeBackwardMontage != nullptr) return DodgeBackwardMontage;
	}
	else
	{
		if (Move.X >= 0.0f && DodgeRightMontage != nullptr) return DodgeRightMontage;
		if (Move.X < 0.0f && DodgeLeftMontage != nullptr) return DodgeLeftMontage;
	}

	return DodgeDefaultMontage != nullptr ? DodgeDefaultMontage : DodgeForwardMontage;
}

void UDodgeFeature::ConsumeCharge()
{
	CurrentCharges = FMath::Max(0, CurrentCharges - 1);
}

void UDodgeFeature::StartChargeRestoreTimerIfNeeded()
{
	if (CurrentCharges >= MaxCharges)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (World->GetTimerManager().IsTimerActive(ChargeRestoreTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		ChargeRestoreTimerHandle,
		FTimerDelegate::CreateUObject(this, &UDodgeFeature::RestoreOneCharge),
		ChargeCooldownTime,
		false);
}

void UDodgeFeature::RestoreOneCharge()
{
	CurrentCharges = FMath::Min(MaxCharges, CurrentCharges + 1);

	if (CurrentCharges < MaxCharges)
	{
		// 还没满，继续排下一个恢复 timer。
		StartChargeRestoreTimerIfNeeded();
	}
}
