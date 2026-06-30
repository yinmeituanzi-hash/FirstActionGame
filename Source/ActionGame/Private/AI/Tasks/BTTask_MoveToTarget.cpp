#include "AI/Tasks/BTTask_MoveToTarget.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "Char/ActionMonsterCharacter.h"

UBTTask_MoveToTarget::UBTTask_MoveToTarget()
{
	NodeName = TEXT("Move To Target");

	// 默认绑定到项目约定的 TargetActor；编辑器里也可以改成 TargetLocation。
	BlackboardKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;

	SelectedSkillPreferredRangeKey.SelectedKeyName = ActionAIBlackboardKeys::SelectedSkillPreferredRange;
	SelectedSkillPreferredRangeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MoveToTarget, SelectedSkillPreferredRangeKey));
}

void UBTTask_MoveToTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		SelectedSkillPreferredRangeKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_MoveToTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// 在父类 ExecuteTask 前，按当前选中技能动态调整 AcceptableRadius。
	// 巡逻分支复用本节点时要关闭这个开关，否则会拿攻击距离当巡逻到点半径。
	if (bUseSelectedSkillPreferredRange)
	{
		if (const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
		{
			const float PreferredRange = Blackboard->GetValueAsFloat(SelectedSkillPreferredRangeKey.SelectedKeyName);
			if (PreferredRange > 0.0f)
			{
				AcceptableRadius = FMath::Max(PreferredRange - AttackRangeBuffer, 50.0f);
				return Super::ExecuteTask(OwnerComp, NodeMemory);
			}
		}
	}

	if (bUseAttackRangeAsAcceptanceRadius)
	{
		if (AAIController* AIOwner = OwnerComp.GetAIOwner())
		{
			if (AActionMonsterCharacter* Monster = Cast<AActionMonsterCharacter>(AIOwner->GetCharacter()))
			{
				const float Range = Monster->GetMonsterAttackRange();
				AcceptableRadius = FMath::Max(Range - AttackRangeBuffer, 50.0f);
			}
		}
	}

	return Super::ExecuteTask(OwnerComp, NodeMemory);
}
