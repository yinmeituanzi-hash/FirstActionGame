#pragma once

#include "CoreMinimal.h"
#include "AI/ActionAITypes.h"
#include "Components/ActorComponent.h"
#include "AIMoveLogicComponent.generated.h"

class AActionMonsterCharacter;

UENUM(BlueprintType)
enum class EAIMoveTypeState : uint8
{
	Walk UMETA(DisplayName = "Walk"),
	Run  UMETA(DisplayName = "Run")
};

UENUM(BlueprintType)
enum class EAICombatMoveMode : uint8
{
	None         UMETA(DisplayName = "None"),
	Approach     UMETA(DisplayName = "Approach"),
	CombatStrafe UMETA(DisplayName = "Combat Strafe")
};

/**
 * 怪物 AI 移动逻辑组件。
 *
 * 参考 010 的 MonMoveLogicComponent 分层：BTService / BTTask 只声明当前移动意图，
 * 速度、加速度、Strafe 状态等统一由本组件落到 CharacterMovement 和动画蓝图可读状态。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UAIMoveLogicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAIMoveLogicComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Action|AI|Move")
	AActionMonsterCharacter* GetOwnerMonster() const;

	UFUNCTION(BlueprintCallable, Category = "Action|AI|Move")
	void SetAlertState(EAIAlertState NewAlertState);

	UFUNCTION(BlueprintCallable, Category = "Action|AI|Move")
	void SetCombatMoveMode(EAICombatMoveMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Action|AI|Move")
	void ClearCombatMoveMode(EAICombatMoveMode ExpectedMode);

	UFUNCTION(BlueprintCallable, Category = "Action|AI|Move")
	void BeginBTMove(EAIMoveTypeState InMoveType, float MaxSpeedOverride = -1.0f, float MaxAccelerationOverride = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Action|AI|Move")
	void EndBTMove();

	UFUNCTION(BlueprintPure, Category = "Action|AI|Move")
	EAICombatMoveMode GetCombatMoveMode() const { return CombatMoveMode; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Move")
	EAIMoveTypeState GetMoveTypeState() const { return MoveTypeState; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Move")
	bool IsInMoveState() const { return bIsInMoveState; }

	UFUNCTION(BlueprintPure, Category = "Action|AI|Move")
	bool IsStrafing() const { return CombatMoveMode == EAICombatMoveMode::CombatStrafe; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|Speed", meta = (ClampMin = "0.0"))
	float IdleMaxWalkSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|Speed", meta = (ClampMin = "0.0"))
	float AlertMaxWalkSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|Speed", meta = (ClampMin = "0.0"))
	float CombatMaxWalkSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|Speed", meta = (ClampMin = "0.0"))
	float CombatStrafeMaxWalkSpeed = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|BT", meta = (ClampMin = "0.0"))
	float BTMaxSpeedWalk = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|BT", meta = (ClampMin = "0.0"))
	float BTMaxSpeedRun = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|BT", meta = (ClampMin = "0.0"))
	float BTMaxAccelerationWalk = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Move|BT", meta = (ClampMin = "0.0"))
	float BTMaxAccelerationRun = 3000.0f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Move", meta = (AllowPrivateAccess = "true"))
	EAIAlertState AlertState = EAIAlertState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Move", meta = (AllowPrivateAccess = "true"))
	EAIMoveTypeState MoveTypeState = EAIMoveTypeState::Walk;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Move", meta = (AllowPrivateAccess = "true"))
	EAICombatMoveMode CombatMoveMode = EAICombatMoveMode::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|AI|Move", meta = (AllowPrivateAccess = "true"))
	bool bIsInMoveState = false;

	float ActiveMaxSpeedOverride = -1.0f;
	float ActiveMaxAccelerationOverride = -1.0f;

	float ResolveMaxWalkSpeed() const;
	float ResolveMaxAcceleration() const;
	void ApplyMovementSettings();
};
