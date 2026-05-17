#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_WaitAtPoint.generated.h"

struct FBTWaitAtPointMemory
{
	float RemainingTime = 0.0f;
};

/**
 * 巡逻点停顿节点。
 *
 * UE 自带 Wait 只能填固定时间；这里保留 Min/Max 随机区间，方便让巡逻节奏不那么机械。
 */
UCLASS()
class ACTIONGAME_API UBTTask_WaitAtPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_WaitAtPoint();

	UPROPERTY(EditAnywhere, Category = "Action|AI|Patrol", meta = (ClampMin = "0.0"))
	float MinWaitTime = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Action|AI|Patrol", meta = (ClampMin = "0.0"))
	float MaxWaitTime = 2.5f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTWaitAtPointMemory); }
	virtual FString GetStaticDescription() const override;
};
