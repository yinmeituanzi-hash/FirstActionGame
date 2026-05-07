#include "Char/ActionCharacterBase.h"
#include "Char/ActionCharacterMovementComponent.h"
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
}

void AActionCharacterBase::BeginPlay()
{
	Super::BeginPlay();

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
