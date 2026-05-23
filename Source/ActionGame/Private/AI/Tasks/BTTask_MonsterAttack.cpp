#include "AI/Tasks/BTTask_MonsterAttack.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Char/ActionMonsterCharacter.h"

UBTTask_MonsterAttack::UBTTask_MonsterAttack()
{
	NodeName = TEXT("Monster Attack");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_MonsterAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTMonsterAttackMemory* Memory = reinterpret_cast<FBTMonsterAttackMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.0f;
	Memory->bAttackStarted = false;

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	if (AIOwner == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	AActionMonsterCharacter* Monster = Cast<AActionMonsterCharacter>(AIOwner->GetCharacter());
	if (Monster == nullptr || Monster->IsDead() || !Monster->CanAttack())
	{
		return EBTNodeResult::Failed;
	}

	// 攻击节奏冷却中：保持当前抽中的攻击分支，等冷却结束再真正开打。
	// 如果目标离开攻击范围，Attack Sequence 上的 IsInAttackRange Decorator 会 abort 本 Task。
	if (Monster->GetAttackCooldownRemaining() > 0.0f)
	{
		return EBTNodeResult::InProgress;
	}

	Monster->StartMonsterAttack();

	// 攻击调用后立刻应该是 IsAttacking==true。如果不是说明 StartMonsterAttack 内部
	// 拒绝了请求（比如 Montage 播放失败、内部状态不允许）→ 直接 Failed。
	if (!Monster->IsAttacking())
	{
		return EBTNodeResult::Failed;
	}
	Memory->bAttackStarted = true;

	// 攻击是异步动作：Task 不能立刻 Success，否则 BT 会在同一轮继续往下执行。
	// 返回 InProgress 后，TickTask 会一直等到 Character 端报告攻击结束。
	return EBTNodeResult::InProgress;
}

void UBTTask_MonsterAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTMonsterAttackMemory* Memory = reinterpret_cast<FBTMonsterAttackMemory*>(NodeMemory);
	Memory->ElapsedTime += DeltaSeconds;

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner ? Cast<AActionMonsterCharacter>(AIOwner->GetCharacter()) : nullptr;
	if (Monster == nullptr || Monster->IsDead() || !Monster->CanAttack())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	if (!Memory->bAttackStarted)
	{
		if (Monster->GetAttackCooldownRemaining() > 0.0f)
		{
			// 普通 return 不会结束 BTTask；节点仍保持 InProgress，下一帧继续 TickTask。
			// 只有 FinishLatentTask 才会把本节点结算为 Failed/Succeeded。
			if (Memory->ElapsedTime >= MaxExecutionTime)
			{
				FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			}
			return;
		}

		Monster->StartMonsterAttack();
		if (!Monster->IsAttacking())
		{
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
			return;
		}

		Memory->bAttackStarted = true;
		return;
	}

	// 攻击完成 → Success。BT 回到上层，下一轮再决定追/打/巡。
	if (!Monster->IsAttacking())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// 兜底超时：Character 端 IsAttacking 没正常被清的极端情况。
	if (Memory->ElapsedTime >= MaxExecutionTime)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}
}

FString UBTTask_MonsterAttack::GetStaticDescription() const
{
	return FString::Printf(TEXT("MonsterAttack (timeout %.1fs)"), MaxExecutionTime);
}
