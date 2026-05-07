#include "Char/ActionMonsterCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Logging/LogMacros.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogActionMonsterCharacter, Log, All);

AActionMonsterCharacter::AActionMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 目前怪物先沿用基类移动配置。
	// 等后面接入 AI 后，再按怪物类型细分移动速度、转向和攻击距离。
}

void AActionMonsterCharacter::ApplyDamage(float InDamage)
{
	if (IsDead())
	{
		UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Ignore damage because monster is already dead."));
		return;
	}

	const float HPBefore = GetCurrentHP();
	const bool bWillDieFromThisHit = HPBefore - FMath::Max(InDamage, 0.0f) <= 0.0f;
	Super::ApplyDamage(InDamage);

	UE_LOG(
		LogActionMonsterCharacter,
		Log,
		TEXT("ActionMonsterCharacter: Took %.1f damage. HP %.1f -> %.1f"),
		InDamage,
		HPBefore,
		GetCurrentHP());

	if (!bWillDieFromThisHit && HitReactMontage != nullptr && GetMesh() != nullptr && GetMesh()->GetAnimInstance() != nullptr)
	{
		const float PlayedLength = PlayAnimMontage(HitReactMontage);
		if (PlayedLength > 0.0f)
		{
			UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Played hit react montage."));
		}
	}
}

void AActionMonsterCharacter::Die()
{
	if (IsDead())
	{
		return;
	}

	Super::Die();

	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Monster died."));

	float FinalLifeSpan = DeathLifeSpan;

	// 当前先给一个最小死亡表现：
	// 如果配置了死亡蒙太奇，就先播放；否则仍然按兜底寿命自动销毁。
	if (DeathMontage != nullptr && GetMesh() != nullptr && GetMesh()->GetAnimInstance() != nullptr)
	{
		const float PlayedLength = PlayAnimMontage(DeathMontage);
		if (PlayedLength > 0.0f)
		{
			FinalLifeSpan = FMath::Max(FinalLifeSpan, PlayedLength);
			UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Played death montage."));

			// 死亡动画真正播完前，Montage 往往已经开始混出到默认状态机了。
			// 所以这里不是卡“最后一瞬间”，而是提前一点冻结，避免先混回 idle。
			if (UWorld* World = GetWorld())
			{
				const float BlendOutTime = DeathMontage->BlendOut.GetBlendTime();
				const float FreezeLeadTime = FMath::Max(DeathPoseFreezeLeadTime, BlendOutTime);
				const float FreezeDelay = FMath::Max(PlayedLength - FreezeLeadTime, 0.0f);
				World->GetTimerManager().SetTimer(
					FreezeDeathPoseTimerHandle,
					this,
					&AActionMonsterCharacter::FreezeDeathPose,
					FreezeDelay,
					false);
			}
		}
	}

	if (FinalLifeSpan > 0.0f)
	{
		SetLifeSpan(FinalLifeSpan);
	}
}

void AActionMonsterCharacter::StartMonsterAttack()
{
	if (!CanAttack())
	{
		UE_LOG(LogActionMonsterCharacter, Warning, TEXT("ActionMonsterCharacter: StartMonsterAttack blocked because character cannot attack."));
		return;
	}

	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: StartMonsterAttack called."));
}

// ============================================================================
// IActionLockableInterface
// ============================================================================

bool AActionMonsterCharacter::CanBeLockedOn_Implementation() const
{
	return !IsDead();
}

FVector AActionMonsterCharacter::GetLockOnTargetLocation_Implementation() const
{
	if (LockOnSocketName != NAME_None && GetMesh() != nullptr && GetMesh()->DoesSocketExist(LockOnSocketName))
	{
		return GetMesh()->GetSocketLocation(LockOnSocketName);
	}
	// 默认锁角色根位置；建议在 BP 里配 LockOnSocketName 指到胸口/头部 socket，体感更好。
	return GetActorLocation();
}

void AActionMonsterCharacter::OnLockedOn_Implementation()
{
	bIsBeingLockedOn = true;
	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Locked on by player."));
}

void AActionMonsterCharacter::OnLockedOff_Implementation()
{
	bIsBeingLockedOn = false;
	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Locked off."));
}

void AActionMonsterCharacter::FreezeDeathPose()
{
	if (GetMesh() == nullptr)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.0f);
	}

	// 同时暂停动画、冻结播放速率，并停掉 Mesh Tick。
	// 这样比只设 bPauseAnims 更稳，不容易被 AnimBP 拉回到待机。
	GetMesh()->bPauseAnims = true;
	GetMesh()->GlobalAnimRateScale = 0.0f;
	GetMesh()->SetComponentTickEnabled(false);

	UE_LOG(LogActionMonsterCharacter, Log, TEXT("ActionMonsterCharacter: Death pose frozen before montage blend-out."));
}
