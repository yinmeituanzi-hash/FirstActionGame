#include "Char/ActionCharacterBase.h"
#include "Animation/AnimInstance.h"
#include "Char/ActionCharacterMovementComponent.h"
#include "Combat/HitReact/HitPhysicsComponent.h"
#include "Combat/HitReact/HitReactComponent.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "Combat/HitReact/HitReceiverComponent.h"
#include "Common/ActionGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AActionCharacterBase::AActionCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UActionCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	// 这里先给一个稳定的默认胶囊体尺寸，后续换模型时再按实际角色体型调整。
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 迁移初期先沿用第三人称动作游戏常见的移动朝向配置。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	// 动态生成的测试怪/训练假人可能没有 Controller，但受击 Montage Root Motion 仍然需要移动物理推进胶囊体。
	MovementComponent->bRunPhysicsWithNoController = true;
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	MovementComponent->JumpZVelocity = 700.0f;
	MovementComponent->AirControl = 0.35f;
	MovementComponent->MaxWalkSpeed = 500.0f;
	MovementComponent->MinAnalogWalkSpeed = 20.0f;
	MovementComponent->BrakingDecelerationWalking = 2000.0f;
	MovementComponent->BrakingDecelerationFalling = 1500.0f;

	// 说明：
	// ACharacter 自带一个叫 Mesh 的骨骼网格组件，所以我们不用自己再创建“角色身体组件”。
	// 以后在 BP_ActionPlayerCharacter / BP_ActionMonsterCharacter 里看到的 Mesh，就是从这里继承下来的。

	// 受击调度组件：所有角色共用同一套受击入口，统一走三层调度（Feedback / React / Physics）。
	HitReceiverComponent = CreateDefaultSubobject<UHitReceiverComponent>(TEXT("HitReceiverComponent"));

	// 受击动画反应组件：玩家和怪物各自在 BP 里指向不同 DataTable，但 C++ 创建在基类。
	HitReactComponent = CreateDefaultSubobject<UHitReactComponent>(TEXT("HitReactComponent"));

	// 受击物理组件：Day 5 先接 HitFly 击飞位移，Day 6 再扩展 Ragdoll / GetUp。
	HitPhysicsComponent = CreateDefaultSubobject<UHitPhysicsComponent>(TEXT("HitPhysicsComponent"));
}

void AActionCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		// 所有战斗角色都可能通过 Montage 播放攻击、闪避、受击等 Root Motion 动作。
		// 放在基类里统一初始化，避免怪物只播受击动画但不应用位移。
		MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion = true;
		if (!IsDead() && CurrentHP > 0.0f && MovementComponent->MovementMode == MOVE_None)
		{
			// 无 Controller 的怪物开局可能停在 MOVE_None；此时动画 Root Motion 会被提取，但不会交给移动组件应用。
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}

	if (UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr)
	{
		// 项目统一使用 Montage Root Motion；普通 Locomotion 仍由移动组件驱动。
		AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	}

	// 运行开始时做一次保护，避免在编辑器里把 CurrentHP 改成无效值后直接带入运行时。
	MaxHP = FMath::Max(MaxHP, 1.0f);
	CurrentHP = FMath::Clamp(CurrentHP, 0.0f, MaxHP);
	SetActionState(bIsDead ? EActionCharacterState::Dead : EActionCharacterState::Idle);
}

bool AActionCharacterBase::CanAttack() const
{
	return !IsDead()
		&& !HasActionTag(ActionGameplayTags::Block_Attack);
}

bool AActionCharacterBase::CanMove() const
{
	return !IsDead()
		&& !HasActionTag(ActionGameplayTags::Block_Move);
}

void AActionCharacterBase::ApplyDamage(float InDamage)
{
	if (IsDead())
	{
		return;
	}

	const float ActualDamage = FMath::Max(InDamage, 0.0f);
	CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.0f, MaxHP);

	if (CurrentHP <= 0.0f)
	{
		Die();
	}
}

void AActionCharacterBase::ReceiveHit(const FHitContext& HitCtx)
{
	if (IsDead())
	{
		return;
	}

	// 第一步：扣血。无论受击表现是否被霸体屏蔽，伤害都要正常结算。
	ApplyDamage(HitCtx.DamageAmount);

	// 第二步：交给 HitReceiver 做表现/动画/物理三层调度。
	// 死亡后 Receiver 会自己 short-circuit，不需要在这里多加判断。
	if (HitReceiverComponent != nullptr)
	{
		HitReceiverComponent->ReceiveHit(HitCtx);
	}
}

void AActionCharacterBase::Die()
{
	if (IsDead())
	{
		return;
	}

	bIsDead = true;
	CurrentHP = 0.0f;
	SetActionState(EActionCharacterState::Dead);

	// Day 2 先只做最小死亡状态：
	// 停止移动并关闭碰撞，确保后续接入怪物死亡时不会继续参与交互。
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UActionCharacterMovementComponent* AActionCharacterBase::GetActionCharacterMovement() const
{
	return Cast<UActionCharacterMovementComponent>(GetCharacterMovement());
}

bool AActionCharacterBase::IsDead() const
{
	return bIsDead || HasActionTag(ActionGameplayTags::State_Action_Dead);
}

void AActionCharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer = ActionTags;
}

bool AActionCharacterBase::HasActionTag(FGameplayTag Tag) const
{
	return ActionTags.HasTagExact(Tag);
}

bool AActionCharacterBase::HasAnyActionTags(const FGameplayTagContainer& QueryTags) const
{
	return ActionTags.HasAnyExact(QueryTags);
}

bool AActionCharacterBase::HasAllActionTags(const FGameplayTagContainer& QueryTags) const
{
	return ActionTags.HasAllExact(QueryTags);
}

FString AActionCharacterBase::GetActionTagsDebugString() const
{
	return ActionTags.ToStringSimple();
}

void AActionCharacterBase::AddActionTagExternal(FGameplayTag Tag)
{
	AddActionTag(Tag);
}

void AActionCharacterBase::RemoveActionTagExternal(FGameplayTag Tag)
{
	RemoveActionTag(Tag);
}

void AActionCharacterBase::SetEnableRootMotionZExtraction(bool bEnableExtraction)
{
	if (UActionCharacterMovementComponent* MovementComponent = GetActionCharacterMovement())
	{
		MovementComponent->bEnableRootMotionZExtraction = bEnableExtraction;
	}
}

void AActionCharacterBase::SetRootMotionZScale(float InScale)
{
	if (UActionCharacterMovementComponent* MovementComponent = GetActionCharacterMovement())
	{
		MovementComponent->RootMotionZScale = FMath::Max(0.0f, InScale);
	}
}

bool AActionCharacterBase::CanChangeActionState(EActionCharacterState OldState, EActionCharacterState NewState) const
{
	return OldState != EActionCharacterState::Dead || NewState == EActionCharacterState::Dead;
}

void AActionCharacterBase::OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState)
{
	ResetActionTagsForState(OldState);
	BP_OnActionStateExit(OldState, NewState);
}

void AActionCharacterBase::OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState)
{
	switch (NewState)
	{
	case EActionCharacterState::Attacking:
		AddActionTag(ActionGameplayTags::State_Action_Attacking);
		break;
	case EActionCharacterState::Dodging:
		AddActionTag(ActionGameplayTags::State_Action_Dodging);
		break;
	case EActionCharacterState::HitReact:
		AddActionTag(ActionGameplayTags::State_Action_HitReact);
		break;
	case EActionCharacterState::Dead:
		AddActionTag(ActionGameplayTags::State_Action_Dead);
		AddActionTag(ActionGameplayTags::Block_Attack);
		AddActionTag(ActionGameplayTags::Block_Dodge);
		AddActionTag(ActionGameplayTags::Block_Move);
		break;
	default:
		break;
	}

	BP_OnActionStateEnter(OldState, NewState);
}

void AActionCharacterBase::SetActionState(EActionCharacterState NewState)
{
	if (CurrentActionState == NewState)
	{
		return;
	}

	const EActionCharacterState OldState = CurrentActionState;
	if (!CanChangeActionState(OldState, NewState))
	{
		return;
	}

	OnActionStateExit(OldState, NewState);
	CurrentActionState = NewState;
	OnActionStateEnter(OldState, NewState);
	OnActionStateChanged.Broadcast(OldState, NewState);
}

void AActionCharacterBase::AddActionTag(FGameplayTag Tag)
{
	if (Tag.IsValid())
	{
		ActionTags.AddTag(Tag);
	}
}

void AActionCharacterBase::RemoveActionTag(FGameplayTag Tag)
{
	if (Tag.IsValid())
	{
		ActionTags.RemoveTag(Tag);
	}
}

void AActionCharacterBase::ResetActionTagsForState(EActionCharacterState State)
{
	switch (State)
	{
	case EActionCharacterState::Attacking:
		RemoveActionTag(ActionGameplayTags::State_Action_Attacking);
		break;
	case EActionCharacterState::Dodging:
		RemoveActionTag(ActionGameplayTags::State_Action_Dodging);
		break;
	case EActionCharacterState::HitReact:
		RemoveActionTag(ActionGameplayTags::State_Action_HitReact);
		break;
	case EActionCharacterState::Dead:
		RemoveActionTag(ActionGameplayTags::State_Action_Dead);
		RemoveActionTag(ActionGameplayTags::Block_Attack);
		RemoveActionTag(ActionGameplayTags::Block_Dodge);
		RemoveActionTag(ActionGameplayTags::Block_Move);
		break;
	default:
		break;
	}
}
