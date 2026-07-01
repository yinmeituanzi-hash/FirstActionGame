#include "AI/Movement/AIMoveLogicComponent.h"

#include "Char/ActionMonsterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UAIMoveLogicComponent::UAIMoveLogicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAIMoveLogicComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyMovementSettings();
}

AActionMonsterCharacter* UAIMoveLogicComponent::GetOwnerMonster() const
{
	return Cast<AActionMonsterCharacter>(GetOwner());
}

void UAIMoveLogicComponent::SetAlertState(EAIAlertState NewAlertState)
{
	AlertState = NewAlertState;
	ApplyMovementSettings();
}

void UAIMoveLogicComponent::SetCombatMoveMode(EAICombatMoveMode NewMode)
{
	CombatMoveMode = NewMode;
	ApplyMovementSettings();
}

void UAIMoveLogicComponent::ClearCombatMoveMode(EAICombatMoveMode ExpectedMode)
{
	if (ExpectedMode != EAICombatMoveMode::None && CombatMoveMode != ExpectedMode)
	{
		return;
	}

	CombatMoveMode = EAICombatMoveMode::None;
	ApplyMovementSettings();
}

void UAIMoveLogicComponent::BeginBTMove(EAIMoveTypeState InMoveType, float MaxSpeedOverride, float MaxAccelerationOverride)
{
	bIsInMoveState = true;
	MoveTypeState = InMoveType;
	ActiveMaxSpeedOverride = MaxSpeedOverride;
	ActiveMaxAccelerationOverride = MaxAccelerationOverride;
	ApplyMovementSettings();
}

void UAIMoveLogicComponent::EndBTMove()
{
	bIsInMoveState = false;
	ActiveMaxSpeedOverride = -1.0f;
	ActiveMaxAccelerationOverride = -1.0f;
	ApplyMovementSettings();
}

float UAIMoveLogicComponent::ResolveMaxWalkSpeed() const
{
	if (ActiveMaxSpeedOverride > 0.0f)
	{
		return ActiveMaxSpeedOverride;
	}

	if (CombatMoveMode == EAICombatMoveMode::CombatStrafe)
	{
		return CombatStrafeMaxWalkSpeed;
	}

	if (bIsInMoveState)
	{
		return MoveTypeState == EAIMoveTypeState::Walk ? BTMaxSpeedWalk : BTMaxSpeedRun;
	}

	switch (AlertState)
	{
	case EAIAlertState::Alert:
		return AlertMaxWalkSpeed;
	case EAIAlertState::Combat:
		return CombatMaxWalkSpeed;
	case EAIAlertState::Idle:
	default:
		return IdleMaxWalkSpeed;
	}
}

float UAIMoveLogicComponent::ResolveMaxAcceleration() const
{
	if (ActiveMaxAccelerationOverride > 0.0f)
	{
		return ActiveMaxAccelerationOverride;
	}

	return MoveTypeState == EAIMoveTypeState::Walk ? BTMaxAccelerationWalk : BTMaxAccelerationRun;
}

void UAIMoveLogicComponent::ApplyMovementSettings()
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (Monster == nullptr || Monster->IsDead())
	{
		return;
	}

	UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}

	Movement->MaxWalkSpeed = ResolveMaxWalkSpeed();
	Movement->MaxAcceleration = ResolveMaxAcceleration();
}
