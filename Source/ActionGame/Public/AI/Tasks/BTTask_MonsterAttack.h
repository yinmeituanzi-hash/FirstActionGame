#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MonsterAttack.generated.h"

/**
 * 命令怪物执行一次攻击。
 *
 * 设计要点：
 *  - 如果攻击节奏冷却已好，ExecuteTask 调 Character->StartMonsterAttack()，返回 InProgress。
 *  - 如果还在攻击节奏冷却中，Task 保持 InProgress 等冷却结束，再发起攻击。
 *  - TickTask 在攻击启动前等 Cooldown；攻击启动后等 Character->IsAttacking() 变 false。
 *  - 这种"启动 + 等待完成"的异步模式是 010 BTTask_BaseUseSkill 的核心套路。
 *
 * 失败路径：
 *  - 没有 AIController / Pawn / Character → Failed（BT 数据问题）
 *  - Character->IsDead 或 !CanAttack → Failed（让 BT 跳到下一选项）
 *  - StartMonsterAttack 调用后 IsAttacking 仍为 false → Failed（攻击没起来，比如 Montage 未配置）
 *  - 超时（MaxExecutionTime）→ Succeeded（兜底，避免 BT 卡死）
 *
 * 命中机制：
 *  - 由 Character 端的 AttackHitCheck Notify 触发球形判定（玩家同款 Library 路径）。
 *  - 玩家在挥刀过程中跑出球外即可躲避，不需要 BTTask 二次检查。
 */

USTRUCT()
struct FBTMonsterAttackMemory
{
	GENERATED_BODY()

	float ElapsedTime = 0.0f;
	bool bAttackStarted = false;
};

UCLASS()
class ACTIONGAME_API UBTTask_MonsterAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MonsterAttack();

	/** 单次攻击 BT 节点最长等待时间（秒）。超过则 Success（避免 IsAttacking 永真锁死）。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI", meta = (ClampMin = "0.5"))
	float MaxExecutionTime = 4.0f;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTMonsterAttackMemory); }
	virtual FString GetStaticDescription() const override;
};
