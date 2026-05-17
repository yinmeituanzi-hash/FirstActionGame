#include "AI/Tasks/BTTask_MoveToTarget.h"

#include "AIController.h"
#include "AI/ActionAIBlackboardKeys.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Char/ActionMonsterCharacter.h"

UBTTask_MoveToTarget::UBTTask_MoveToTarget()
{
	NodeName = TEXT("Move To Target");

	// 默认绑到我们约定的 BB Key。
	// UBTTask_MoveTo 父类的 BlackboardKey 已声明，是 Object/Vector 二选一的 Selector。
	// 默认绑 TargetActor，编辑器里改成 TargetLocation 也可以。
	BlackboardKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
}

EBTNodeResult::Type UBTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 在父类 ExecuteTask 跑前，按 AttackRange 动态调整 AcceptableRadius。
	// 注意：AcceptableRadius 是 UBTTask_MoveTo 的成员，UPROPERTY 可改。
	// 这里改的是节点实例的字段，影响本次寻路的 PathPoint Acceptance 半径。
	// 巡逻分支复用本节点时要关闭这个开关，否则会拿攻击距离当巡逻到点半径。
	if (bUseAttackRangeAsAcceptanceRadius)
	{
		if (AAIController* AIOwner = OwnerComp.GetAIOwner())
		{
			if (AActionMonsterCharacter* Monster = Cast<AActionMonsterCharacter>(AIOwner->GetCharacter()))
			{
				const float Range = Monster->GetMonsterAttackRange();
				const float NewRadius = FMath::Max(Range - AttackRangeBuffer, 50.0f);
				AcceptableRadius = NewRadius;
			}
		}
	}

	return Super::ExecuteTask(OwnerComp, NodeMemory);
}
