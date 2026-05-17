#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_FindPatrolPoint.generated.h"

/**
 * 从 Blackboard.HomeLocation 周围随机选一个可到达的 NavMesh 点，写入 Blackboard.TargetLocation。
 *
 * 这个节点只负责"选点"，真正移动继续复用 UBTTask_MoveToTarget：
 * 在 BT 里把 Move To Target 的 BlackboardKey 改成 TargetLocation，并关闭攻击距离停止半径即可。
 */
UCLASS()
class ACTIONGAME_API UBTTask_FindPatrolPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindPatrolPoint();

	/** 巡逻围绕的中心点，默认是 AIController OnPossess 写入的 HomeLocation。 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector HomeLocationKey;

	/** 输出巡逻点，默认写入 TargetLocation。 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetLocationKey;

	/** 从 HomeLocation 向外随机找点的半径（cm）。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Patrol", meta = (ClampMin = "0.0"))
	float PatrolRadius = 800.0f;

	/** 尽量避免选到离当前 Pawn 太近的点；找不到时仍会接受一个可达点。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Patrol", meta = (ClampMin = "0.0"))
	float MinDistanceFromCurrent = 150.0f;

	/** 随机尝试次数。次数越高越不容易选到过近点，但代价也更高。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Patrol", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxAttempts = 5;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual FString GetStaticDescription() const override;
};
