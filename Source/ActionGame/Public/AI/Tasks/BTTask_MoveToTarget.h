#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BTTask_MoveToTarget.generated.h"

/**
 * 怪物移动到 Blackboard 目标位置/Actor 的薄包装。
 *
 * 为什么不直接在 BT 里用 `MoveTo`：
 *  1. 默认绑到我们约定的 BB Key 名（`TargetActor`），编辑器配置时不用每次手动选。
 *  2. 可选 `bUseAttackRangeAsAcceptanceRadius`：自动把"攻击距离 - Buffer"作为停止距离。
 *     这样 Character 端调整 MonsterAttackRange 时 BT 不需要跟着改。
 *  3. 后续 Sprint 4-B+ / 4-C+ 可以扩"追击超时"、"目标过远改用 EQS 选位"等逻辑。
 */
UCLASS()
class ACTIONGAME_API UBTTask_MoveToTarget : public UBTTask_MoveTo
{
	GENERATED_BODY()

public:
	UBTTask_MoveToTarget();

	/**
	 * 是否用 Character.MonsterAttackRange - Buffer 作为 AcceptableRadius。
	 * - true（默认）：保证停在攻击距离内一点点，让后续 BTTask_MonsterAttack 一进来就在范围内。
	 * - false：使用 BT 节点上配置的 AcceptableRadius。
	 */
	UPROPERTY(EditAnywhere, Category = "Action|AI")
	bool bUseAttackRangeAsAcceptanceRadius = true;

	/**
	 * 用攻击距离作为停止距离时的余量（cm）。
	 * 例：AttackRange=400, Buffer=50 → AcceptanceRadius=350。
	 * 留 Buffer 是为了"刚到边缘就停"，避免轻微目标位移导致马上又脱出。
	 */
	UPROPERTY(EditAnywhere, Category = "Action|AI", meta = (EditCondition = "bUseAttackRangeAsAcceptanceRadius", ClampMin = "0.0"))
	float AttackRangeBuffer = 50.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
