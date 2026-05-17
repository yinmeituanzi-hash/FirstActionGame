#include "Combat/ActionFeatures/AttackFeature.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Char/ActionCharacterMovementComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "Char/ActionPlayerCharacter.h"
#include "Combat/ActionCombatLibrary.h"
#include "Combat/ActionCombatNotifies.h"
#include "Combat/Components/ActionCombatComponent.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "Common/ActionGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAttackFeature, Log, All);

UAttackFeature::UAttackFeature()
{
	FeatureName = TEXT("Attack");
	TargetState = EActionCharacterState::Attacking;
	bEnableTick = false;

	BlockTags.AddTag(ActionGameplayTags::Block_Attack);
}

void UAttackFeature::Execute()
{
	// 第一段攻击的入口。如果当前已激活（在攻击中），让玩家走 TryAdvanceCombo。
	if (bIsActive)
	{
		UE_LOG(LogAttackFeature, Log, TEXT("AttackFeature: Execute called while active. Use TryAdvanceCombo instead."));
		return;
	}

	if (!CanExecute())
	{
		UE_LOG(LogAttackFeature, Log, TEXT("AttackFeature: Execute blocked by CanExecute."));
		return;
	}

	StartComboAtIndex(0);
}

bool UAttackFeature::TryAdvanceCombo()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr || !bIsActive)
	{
		return false;
	}

	if (!Owner->HasActionTag(ActionGameplayTags::Window_Attack_CanCombo))
	{
		return false;
	}

	const int32 NextIndex = GetNextComboIndex();
	if (NextIndex == INDEX_NONE)
	{
		return false;
	}

	StartComboAtIndex(NextIndex);
	return true;
}

void UAttackFeature::StartComboAtIndex(int32 ComboIndex)
{
	UAnimMontage* Selected = GetMontageForComboIndex(ComboIndex);
	if (Selected == nullptr)
	{
		UE_LOG(LogAttackFeature, Warning, TEXT("AttackFeature: No montage for combo index %d."), ComboIndex);
		return;
	}

	UAnimInstance* AnimInstance = GetOwnerAnimInstance();
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (AnimInstance == nullptr || Owner == nullptr)
	{
		return;
	}

	// 连段切换：先用 ActionCombatComponent 平滑停掉前一段（保留你们已经调过的 BlendOut 行为）。
	UAnimMontage* PreviousMontage = ActiveMontage;
	if (PreviousMontage != nullptr && AnimInstance->Montage_IsPlaying(PreviousMontage))
	{
		if (UActionCombatComponent* CombatComp = Owner->GetActionCombatComponent())
		{
			CombatComp->StopAttackMontageForComboTransition(AnimInstance, PreviousMontage);
		}
		else
		{
			AnimInstance->Montage_Stop(0.08f, PreviousMontage);
		}
	}

	// 转向窗口处理：
	// 优先级 1：如果有锁定目标，每段攻击启动时都强制朝向锁定目标（覆盖玩家方向输入）。
	// 优先级 2：上一段标记了 CanTurn，那么进入下一段时消费玩家方向输入做转向。
	if (Owner->HasLockOnTarget())
	{
		if (UActionCombatComponent* CombatComp = Owner->GetActionCombatComponent())
		{
			const FVector ToTarget = (Owner->GetLockOnTargetLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				const float TargetYaw = ToTarget.Rotation().Yaw;
				// 复用 Movement 的转向通道，与 RootMotion 同帧推进。
				if (UActionCharacterMovementComponent* ActionMove = Cast<UActionCharacterMovementComponent>(Owner->GetCharacterMovement()))
				{
					ActionMove->RequestAttackComboTurn(TargetYaw, CombatComp->AttackTurnMaxYawSpeedDeg);
				}
				else
				{
					FRotator NewRot = Owner->GetActorRotation();
					NewRot.Yaw = TargetYaw;
					Owner->SetActorRotation(NewRot);
				}
			}
		}
	}
	else if (ComboIndex > 0 && Owner->HasActionTag(ActionGameplayTags::Window_Attack_CanTurn))
	{
		if (UActionCombatComponent* CombatComp = Owner->GetActionCombatComponent())
		{
			CombatComp->ApplyAttackTurnAtComboStart(Owner, Owner->GetLastMoveInput());
		}
	}

	// 真正激活（基类负责互斥、状态切换）。
	if (!bIsActive)
	{
		BeginActive();
	}

	// 启动新一段：清掉所有窗口、命中目标缓存。
	HitActorsThisSwing.Reset();
	Owner->RemoveActionTagExternal(ActionGameplayTags::Window_Attack_CanCombo);
	Owner->RemoveActionTagExternal(ActionGameplayTags::Window_Attack_CanTurn);
	Owner->RemoveActionTagExternal(ActionGameplayTags::Window_Attack_CanDodgeCancel);
	Owner->AddActionTagExternal(ActionGameplayTags::Block_Attack);
	Owner->AddActionTagExternal(ActionGameplayTags::Block_Dodge);
	Owner->AddActionTagExternal(ActionGameplayTags::Block_Move);

	if (UCharacterMovementComponent* Movement = Owner->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	const float Duration = PlayMontageInternal(Selected);
	if (Duration <= 0.0f)
	{
		UE_LOG(LogAttackFeature, Warning, TEXT("AttackFeature: Failed to play combo montage %d."), ComboIndex);
		Stop(true);
		return;
	}

	CurrentComboIndex = ComboIndex;
	UE_LOG(LogAttackFeature, Log, TEXT("AttackFeature: Combo started at index %d."), ComboIndex);
}

UAnimMontage* UAttackFeature::GetMontageForComboIndex(int32 ComboIndex) const
{
	if (ComboMontages.IsValidIndex(ComboIndex) && ComboMontages[ComboIndex] != nullptr)
	{
		return ComboMontages[ComboIndex];
	}
	return ComboIndex == 0 ? FallbackAttackMontage : nullptr;
}

int32 UAttackFeature::GetNextComboIndex() const
{
	const int32 Next = CurrentComboIndex + 1;
	return GetMontageForComboIndex(Next) != nullptr ? Next : INDEX_NONE;
}

void UAttackFeature::OnNotify(FName NotifyName, const FBranchingPointNotifyPayload& /*Payload*/)
{
	if (NotifyName == ActionCombatNotifies::AttackHitCheck)
	{
		HandleHitCheck();
	}
	else if (NotifyName == ActionCombatNotifies::AttackComboWindowStart)
	{
		HandleComboWindowStart();
	}
	else if (NotifyName == ActionCombatNotifies::AttackTurnWindowStart)
	{
		HandleTurnWindowStart();
	}
	else if (NotifyName == ActionCombatNotifies::AttackDodgeCancelStart)
	{
		HandleDodgeCancelStart();
	}
}

void UAttackFeature::OnMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
	UE_LOG(LogAttackFeature, Log, TEXT("AttackFeature: Combo ended (interrupted=%s)."), bInterrupted ? TEXT("true") : TEXT("false"));
	CurrentComboIndex = INDEX_NONE;
	HitActorsThisSwing.Reset();

	// 整段连击结束（无论是被打断还是自然结束）：清掉残留的连段转向请求，
	// 避免下一次站桩攻击仍以前一段的目标 Yaw 继续推进。
	if (AActionPlayerCharacter* Owner = OwnerChar.Get())
	{
		if (UActionCombatComponent* CombatComp = Owner->GetActionCombatComponent())
		{
			CombatComp->ClearAttackTurn(Owner);
		}
	}

	Stop(bInterrupted);
}

void UAttackFeature::HandleHitCheck()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}

	// HitContext 模板：填入所有"攻击者一侧决定"的字段。
	// Library 会复制此模板用于每个被命中目标，并自动填 Attacker / HitLocation / HitDirection。
	FHitContext Template;
	Template.ReactType = EHitReactType::LightHit;  // 普攻默认轻击；最后一段连击可考虑改为 HeavyHit / HitFly
	Template.DamageAmount = Owner->GetAttackPower();
	Template.FeedbackScale = 1.0f;

	FSphereAttackHitParams Params;
	Params.Attacker = Owner;
	Params.Center = Owner->GetActorLocation() + Owner->GetActorForwardVector() * HitCheckForwardOffset;
	Params.Radius = HitCheckRadius;
	Params.TargetClassFilter = AActionMonsterCharacter::StaticClass();
	Params.HitContextTemplate = Template;
	Params.HitLocationBackstep = 40.0f;
	Params.bDrawDebugSphere = bDrawDebugHitSphere;
	Params.DebugDrawDuration = 1.0f;

	const int32 HitCount = UActionCombatLibrary::PerformSphereAttackHit(Owner, Params, HitActorsThisSwing);
	UE_LOG(LogAttackFeature, Verbose, TEXT("AttackFeature: HandleHitCheck hit %d new target(s)."), HitCount);
}

void UAttackFeature::HandleComboWindowStart()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}
	Owner->AddActionTagExternal(ActionGameplayTags::Window_Attack_CanCombo);
}

void UAttackFeature::HandleTurnWindowStart()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}
	Owner->AddActionTagExternal(ActionGameplayTags::Window_Attack_CanTurn);
}

void UAttackFeature::HandleDodgeCancelStart()
{
	AActionPlayerCharacter* Owner = OwnerChar.Get();
	if (Owner == nullptr)
	{
		return;
	}
	Owner->RemoveActionTagExternal(ActionGameplayTags::Block_Dodge);
	Owner->AddActionTagExternal(ActionGameplayTags::Window_Attack_CanDodgeCancel);
}
