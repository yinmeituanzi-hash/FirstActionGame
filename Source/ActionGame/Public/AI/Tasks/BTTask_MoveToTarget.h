#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BTTask_MoveToTarget.generated.h"

/**
 * 怪物移动到 Blackboard 目标位置 / Actor 的薄包装。
 *
 * 这里保留自定义 Task，是为了把项目约定的目标 Key、技能释放距离、追击停止距离集中在 C++ 里。
 * 后续如果接 EQS / 战斗走位，也能继续复用这个入口。
 */
UCLASS()
class ACTIONGAME_API UBTTask_MoveToTarget : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UBTTask_MoveToTarget();

	/** 优先使用 PickCombatSkill 写入的 SelectedSkillPreferredRange 作为停止距离。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI")
	bool bUseSelectedSkillPreferredRange = true;

	UPROPERTY(EditAnywhere, Category = "Blackboard", meta = (EditCondition = "bUseSelectedSkillPreferredRange"))
	FBlackboardKeySelector SelectedSkillPreferredRangeKey;

	/**
	 * 是否用 Character.MonsterAttackRange - Buffer 作为 AcceptableRadius。
	 * 注意：这是没有 SelectedSkillPreferredRange 时的过渡 fallback。
	 */
	UPROPERTY(EditAnywhere, Category = "Action|AI")
	bool bUseAttackRangeAsAcceptanceRadius = true;

	/** 停止距离余量，避免刚到技能边缘就停下，目标轻微位移后马上脱出释放范围。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI", meta = (EditCondition = "bUseAttackRangeAsAcceptanceRadius", ClampMin = "0.0"))
	float AttackRangeBuffer = 50.0f;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
