#include "Char/ActionCharacterBase.h"

#include "Animation/AnimInstance.h"
#include "Char/ActionCharacterMovementComponent.h"
#include "Combat/Attributes/AttributeComponent.h"
#include "Combat/HitReact/HitPhysicsComponent.h"
#include "Combat/HitReact/HitReactComponent.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "Combat/HitReact/HitReceiverComponent.h"
#include "Combat/Skills/ActionSkillComponent.h"
#include "Combat/VFX/ActionVFXComponent.h"
#include "Common/ActionGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AActionCharacterBase::AActionCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UActionCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

	// 这里先给一个稳定的默认胶囊体尺寸，后续换模型时再按角色体型调整。
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	// 第三人称动作游戏常用的移动朝向配置。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bRunPhysicsWithNoController = true;
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	MovementComponent->JumpZVelocity = 700.0f;
	MovementComponent->AirControl = 0.35f;
	MovementComponent->MaxWalkSpeed = 500.0f;
	MovementComponent->MinAnalogWalkSpeed = 20.0f;
	MovementComponent->BrakingDecelerationWalking = 2000.0f;
	MovementComponent->BrakingDecelerationFalling = 1500.0f;

	AttributeComponent = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComponent"));
	HitReceiverComponent = CreateDefaultSubobject<UHitReceiverComponent>(TEXT("HitReceiverComponent"));
	HitReactComponent = CreateDefaultSubobject<UHitReactComponent>(TEXT("HitReactComponent"));
	HitPhysicsComponent = CreateDefaultSubobject<UHitPhysicsComponent>(TEXT("HitPhysicsComponent"));
	SkillComponent = CreateDefaultSubobject<UActionSkillComponent>(TEXT("SkillComponent"));
	VFXComponent = CreateDefaultSubobject<UActionVFXComponent>(TEXT("ActionVFXComponent"));
}

void AActionCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (AttributeComponent != nullptr)
	{
		AttributeComponent->OnAttributeChanged.AddUniqueDynamic(this, &AActionCharacterBase::HandleAttributeChanged);
		AttributeComponent->InitializeAttributesFromDefaults();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		// 所有战斗角色都可能通过 Montage 播放攻击、闪避、受击等 Root Motion 动作。
		MovementComponent->bAllowPhysicsRotationDuringAnimRootMotion = true;
		if (!IsDead() && GetCurrentHP() > 0.0f && MovementComponent->MovementMode == MOVE_None)
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}

	if (UAnimInstance* AnimInstance = GetMesh() != nullptr ? GetMesh()->GetAnimInstance() : nullptr)
	{
		// 项目统一使用 Montage Root Motion；普通 Locomotion 仍由移动组件驱动。
		AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
	}

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

void AActionCharacterBase::RequestActionState(EActionCharacterState InState)
{
	SetActionState(InState);
}

void AActionCharacterBase::ApplyDamage(float InDamage)
{
	if (IsDead() || AttributeComponent == nullptr)
	{
		return;
	}

	const float ActualDamage = FMath::Max(InDamage, 0.0f);
	if (ActualDamage <= 0.0f)
	{
		return;
	}

	AttributeComponent->ModifyAttribute(EAttributeType::HP, -ActualDamage);

	if (GetCurrentHP() <= 0.0f)
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

	// 伤害结算先发生；表现层是否被霸体屏蔽，不影响扣血。
	ApplyDamage(HitCtx.DamageAmount);

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
	if (AttributeComponent != nullptr)
	{
		AttributeComponent->SetAttribute(EAttributeType::HP, 0.0f);
	}

	if (SkillComponent != nullptr && SkillComponent->IsUsingSkill())
	{
		SkillComponent->StopSkill(EActionSkillStopReason::Death);
	}

	SetActionState(EActionCharacterState::Dead);

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

float AActionCharacterBase::GetCurrentHP() const
{
	return GetAttribute(EAttributeType::HP);
}

float AActionCharacterBase::GetMaxHP() const
{
	return GetAttribute(EAttributeType::MaxHP);
}

float AActionCharacterBase::GetAttackPower() const
{
	return GetAttribute(EAttributeType::AttackPower);
}

float AActionCharacterBase::GetAttribute(EAttributeType Attribute) const
{
	return AttributeComponent != nullptr ? AttributeComponent->GetAttribute(Attribute) : 0.0f;
}

bool AActionCharacterBase::IsDead() const
{
	return bIsDead || HasActionTag(ActionGameplayTags::State_Action_Dead);
}

bool AActionCharacterBase::IsFriendlyTo(const AActionCharacterBase* Other) const
{
	if (Other == nullptr || Other == this)
	{
		return true;
	}

	return CombatTeam != EActionCombatTeam::Neutral
		&& CombatTeam == Other->CombatTeam;
}

bool AActionCharacterBase::CanDamageTarget(const AActionCharacterBase* Other) const
{
	return Other != nullptr
		&& Other != this
		&& !IsFriendlyTo(Other);
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
	case EActionCharacterState::Skill:
		AddActionTag(ActionGameplayTags::State_Action_Skill);
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

void AActionCharacterBase::HandleAttributeChanged(EAttributeType Attribute, float OldValue, float NewValue)
{
	(void)OldValue;

	// 外部系统直接把 HP 改到 0 时，也必须进入角色统一死亡流程。
	if (Attribute == EAttributeType::HP && NewValue <= 0.0f && !IsDead())
	{
		Die();
	}
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
	case EActionCharacterState::Skill:
		RemoveActionTag(ActionGameplayTags::State_Action_Skill);
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
