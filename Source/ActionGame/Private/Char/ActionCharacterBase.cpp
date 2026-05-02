#include "Char/ActionCharacterBase.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AActionCharacterBase::AActionCharacterBase()
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
}

bool AActionCharacterBase::CanAttack() const
{
	return !bIsDead;
}

void AActionCharacterBase::ApplyDamage(float InDamage)
{
	if (bIsDead)
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
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentHP = 0.0f;

	// Day 2 先只做最小死亡状态：
	// 停止移动并关闭碰撞，确保后续接入怪物死亡时不会继续参与交互。
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->DisableMovement();
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
