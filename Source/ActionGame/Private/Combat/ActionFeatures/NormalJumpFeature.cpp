#include "Combat/ActionFeatures/NormalJumpFeature.h"
#include "Char/ActionPlayerCharacter.h"
#include "Common/ActionGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogJumpFeature, Log, All);

UNormalJumpFeature::UNormalJumpFeature()
{
	FeatureName = TEXT("NormalJump");
	// 跳跃不切角色主状态（让 ABP 用 Falling 自己驱动），保持 Idle，避免和 Attacking/Dodging 冲突。
	TargetState = EActionCharacterState::Idle;
	bEnableTick = false;

	// 跳跃不需要被 Block 阻挡（除非死亡），也不需要 BlockTags。
	// 跳跃打断攻击/闪避的逻辑放在 Execute 里主动处理。
}

bool UNormalJumpFeature::CanExecute() const
{
	const AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr || Owner->IsDead())
	{
		return false;
	}

	// 还有剩余跳跃次数才允许跳。
	if (JumpCount >= MaxJumpCount)
	{
		return false;
	}

	return true;
}

void UNormalJumpFeature::Execute()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}

	if (!CanExecute())
	{
		UE_LOG(LogJumpFeature, Log, TEXT("JumpFeature: blocked. JumpCount=%d Max=%d"), JumpCount, MaxJumpCount);
		return;
	}

	// 跳跃打断攻击/闪避：把当前活跃 Feature 停掉。
	// 这等价于 010 PlayerCharacter::DoWalkCancel 里"跳跃可以取消普攻/闪避"的设计。
	UActionFeatureBase* CurrentActive = Owner->GetCurrentActiveFeature();
	if (CurrentActive != nullptr && CurrentActive != this && CurrentActive->IsActive())
	{
		UE_LOG(LogJumpFeature, Log, TEXT("JumpFeature: interrupting active feature %s."), *CurrentActive->FeatureName.ToString());
		CurrentActive->Stop(true);
	}

	// 决定这是第几段跳。
	const ENormalJumpState NewState = (JumpCount == 0) ? ENormalJumpState::FirstJump : ENormalJumpState::SecondJump;
	DoSingleJump(NewState);
}

void UNormalJumpFeature::DoSingleJump(ENormalJumpState NewState)
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}

	RestoreMovementSettings();

	const float ZSpeed = (NewState == ENormalJumpState::FirstJump) ? FirstJumpZVelocity : SecondJumpZVelocity;

	FVector LaunchVelocity(0.0f, 0.0f, ZSpeed);

	if (bOverrideXYVelocityOnJump && Owner->GetController() != nullptr)
	{
		const FRotator ControlRot = Owner->GetController()->GetControlRotation();
		const FVector Forward = FRotator(0.0f, ControlRot.Yaw, 0.0f).Vector();
		LaunchVelocity.X = Forward.X * Movement->MaxWalkSpeed;
		LaunchVelocity.Y = Forward.Y * Movement->MaxWalkSpeed;
	}

	// 跳跃统计：第一段跳进入 Falling 之前先 +1，二段跳直接 +1。
	JumpCount++;
	CurrentJumpState = NewState;

	Owner->LaunchCharacter(LaunchVelocity, /*bXYOverride*/ bOverrideXYVelocityOnJump, /*bZOverride*/ true);

	UE_LOG(LogJumpFeature, Log, TEXT("JumpFeature: jumped (state=%d, count=%d/%d, Z=%.0f)."),
		(int32)NewState, JumpCount, MaxJumpCount, ZSpeed);
}

void UNormalJumpFeature::NotifyLanded()
{
	UE_LOG(LogJumpFeature, Log, TEXT("JumpFeature: landed, reset jump count."));
	JumpCount = 0;
	CurrentJumpState = ENormalJumpState::Grounded;

	StartLandingStartup();
}

void UNormalJumpFeature::NotifyFallingFromLedge()
{
	// 玩家直接走下台阶进入下落（没有主动跳）：
	// 把第一段算作"已消耗"，避免空中无限二段跳。
	if (JumpCount == 0)
	{
		JumpCount = 1;
		CurrentJumpState = ENormalJumpState::Falling;
		UE_LOG(LogJumpFeature, Log, TEXT("JumpFeature: falling from ledge, consume first jump implicitly."));
	}
}

void UNormalJumpFeature::StartLandingStartup()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr || Owner->IsDead())
	{
		return;
	}

	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}

	ClearLandingTimers();
	RestoreMovementSettings();
	CacheMovementSettings(*Movement);

	FVector Velocity = Movement->Velocity;
	Velocity.X *= LandingHorizontalVelocityScale;
	Velocity.Y *= LandingHorizontalVelocityScale;
	Movement->Velocity = Velocity;

	Movement->BrakingDecelerationWalking = LandingBrakingDecelerationWalking;
	Movement->GroundFriction = LandingGroundFriction;
	Movement->BrakingFrictionFactor = LandingBrakingFrictionFactor;

	Owner->AddActionTagExternal(ActionGameplayTags::State_Action_LandingStartup);
	Owner->RemoveActionTagExternal(ActionGameplayTags::State_Action_LandingRecovery);
	Owner->SetMovementControlScales(LandingStartupMoveInputScale, LandingStartupMaxSpeedMultiplier);

	if (UWorld* World = Owner->GetWorld())
	{
		if (LandingStartupDuration > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				LandingStartupTimerHandle,
				this,
				&UNormalJumpFeature::BeginLandingRecovery,
				LandingStartupDuration,
				false);
		}
		else
		{
			BeginLandingRecovery();
		}
	}
}

void UNormalJumpFeature::BeginLandingRecovery()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr || Owner->IsDead())
	{
		RestoreMovementSettings();
		return;
	}

	Owner->RemoveActionTagExternal(ActionGameplayTags::State_Action_LandingStartup);
	Owner->AddActionTagExternal(ActionGameplayTags::State_Action_LandingRecovery);
	Owner->SetMovementControlScales(LandingRecoveryMoveInputScale, LandingRecoveryMaxSpeedMultiplier);

	if (UWorld* World = Owner->GetWorld())
	{
		if (LandingRecoveryDuration > 0.0f)
		{
			World->GetTimerManager().SetTimer(
				LandingRecoveryTimerHandle,
				this,
				&UNormalJumpFeature::EndLandingRecovery,
				LandingRecoveryDuration,
				false);
		}
		else
		{
			EndLandingRecovery();
		}
	}
}

void UNormalJumpFeature::EndLandingRecovery()
{
	RestoreMovementSettings();
}

void UNormalJumpFeature::ClearLandingTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LandingStartupTimerHandle);
		World->GetTimerManager().ClearTimer(LandingRecoveryTimerHandle);
	}
}

void UNormalJumpFeature::CacheMovementSettings(UCharacterMovementComponent& Movement)
{
	CachedGroundFriction = Movement.GroundFriction;
	CachedBrakingDecelerationWalking = Movement.BrakingDecelerationWalking;
	CachedBrakingFrictionFactor = Movement.BrakingFrictionFactor;
	bCachedMovementSettingsValid = true;
}

void UNormalJumpFeature::RestoreMovementSettings()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner != nullptr)
	{
		if (UCharacterMovementComponent* Movement = Owner->GetCharacterMovement())
		{
			if (bCachedMovementSettingsValid)
			{
				Movement->GroundFriction = CachedGroundFriction;
				Movement->BrakingDecelerationWalking = CachedBrakingDecelerationWalking;
				Movement->BrakingFrictionFactor = CachedBrakingFrictionFactor;
			}
		}

		Owner->RemoveActionTagExternal(ActionGameplayTags::State_Action_LandingStartup);
		Owner->RemoveActionTagExternal(ActionGameplayTags::State_Action_LandingRecovery);
		Owner->ClearMovementControlScales();
	}

	ClearLandingTimers();
	bCachedMovementSettingsValid = false;
}
