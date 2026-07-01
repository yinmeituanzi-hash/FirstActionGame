#pragma once

#include "CoreMinimal.h"
#include "AI/Movement/AIMoveLogicComponent.h"
#include "AI/Tasks/BTTask_MoveToTarget.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_SimpleMoveTo.generated.h"

class AActionMonsterCharacter;

/**
 * 带移动意图配置的 MoveToTarget。
 *
 * 010 的 SimpleMoveTo / TeamMoveTo 会在移动 Task 内设置速度、加速度、
 * 转向策略和移动状态；这里保留同样分层，但复用现有 MoveToTarget 的技能距离逻辑。
 */
UCLASS()
class ACTIONGAME_API UBTTask_SimpleMoveTo : public UBTTask_MoveToTarget
{
	GENERATED_BODY()

public:
	UBTTask_SimpleMoveTo();

	UPROPERTY(EditAnywhere, Category = "Action|AI|Movement")
	EAIMoveTypeState MoveTypeState = EAIMoveTypeState::Run;

	/** >0 时覆盖 UAIMoveLogicComponent 根据 MoveTypeState 推导出的速度。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Movement", meta = (ClampMin = "-1.0"))
	float MaxSpeedOverride = -1.0f;

	/** >0 时覆盖 UAIMoveLogicComponent 根据 MoveTypeState 推导出的加速度。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Movement", meta = (ClampMin = "-1.0"))
	float MaxAccelerationOverride = -1.0f;

	/** 是否让 Controller Yaw 驱动角色朝向。Combat Strafe 通常打开，并由 Focus/FaceTarget 控制朝向。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Rotation")
	bool bUseControllerRotationYaw = false;

	/** 是否让移动方向驱动角色朝向。普通追击通常打开，Combat Strafe 通常关闭。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Rotation")
	bool bOrientRotationToMovement = true;

	/** 进入 Task 时是否把 AI Focus 设置为 TargetActor。Combat Strafe 时常用。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Rotation")
	bool bFocusTargetActor = false;

	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (EditCondition = "bFocusTargetActor"))
	FBlackboardKeySelector TargetActorKey;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

private:
	TWeakObjectPtr<AActionMonsterCharacter> CachedMonster;
	bool bCachedUseControllerRotationYaw = false;
	bool bCachedOrientRotationToMovement = true;
	bool bHasCachedMovementSettings = false;

	void ApplyMoveSettings(UBehaviorTreeComponent& OwnerComp);
	void ClearMoveSettings(UBehaviorTreeComponent& OwnerComp);
};
