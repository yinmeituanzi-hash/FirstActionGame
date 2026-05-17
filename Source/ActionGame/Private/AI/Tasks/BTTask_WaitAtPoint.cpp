#include "AI/Tasks/BTTask_WaitAtPoint.h"

#include "BehaviorTree/BehaviorTreeComponent.h"

UBTTask_WaitAtPoint::UBTTask_WaitAtPoint()
{
	NodeName = TEXT("Wait At Point");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_WaitAtPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTWaitAtPointMemory* Memory = reinterpret_cast<FBTWaitAtPointMemory*>(NodeMemory);

	// BTTask 默认不是每个 AI 一个 UObject 实例，运行时状态要放进 NodeMemory。
	// RemainingTime 如果写成成员变量，多只怪共用同一个节点资产时会互相覆盖。
	const float ActualMin = FMath::Max(MinWaitTime, 0.0f);
	const float ActualMax = FMath::Max(MaxWaitTime, ActualMin);
	Memory->RemainingTime = FMath::FRandRange(ActualMin, ActualMax);

	return Memory->RemainingTime <= 0.0f ? EBTNodeResult::Succeeded : EBTNodeResult::InProgress;
}

void UBTTask_WaitAtPoint::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTWaitAtPointMemory* Memory = reinterpret_cast<FBTWaitAtPointMemory*>(NodeMemory);
	Memory->RemainingTime -= DeltaSeconds;

	if (Memory->RemainingTime <= 0.0f)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

FString UBTTask_WaitAtPoint::GetStaticDescription() const
{
	return FString::Printf(TEXT("WaitAtPoint: %.1f - %.1fs"), MinWaitTime, MaxWaitTime);
}
