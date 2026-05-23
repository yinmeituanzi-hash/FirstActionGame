#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_PickHatredTarget.generated.h"

/**
 * Sprint 4-C+ Day 6：把怪物 HatredMap 里仇恨最高的目标写回 BB.TargetActor。
 *
 * 设计意图：
 *  - **当前**：单玩家场景下 HatredMap 永远只有玩家一个条目，退化为"优先攻击者"，
 *    行为上与 VisionService 写的 Target 一致。
 *  - **未来**：加入同伴 / 召唤物 / 多人后，HatredMap 会有多个候选，
 *    BTService_PickHatredTarget 自动选打怪最多的玩家，BT 节点不需要改任何东西。
 *
 * 挂载位置：BT Combat 分支根 Sequence 上。每 0.5s Tick 一次（仇恨更新频率不需要太高）。
 *
 * 与 VisionService 的协作：
 *  - VisionService 先写 TargetActor（看到谁就追谁）
 *  - PickHatredTarget 后跑：如果 HatredMap 非空，用仇恨最高者覆盖 TargetActor；
 *    如果 HatredMap 为空（怪物还没被打过 / 已 ClearHatred），保留 VisionService 写的 Target
 *
 * 这样设计的好处：
 *  - 一直没被打的怪：靠视觉锁定（看到玩家就追）
 *  - 被打过的怪：优先反击攻击者（即使攻击者短暂跑出视野，仇恨仍在）
 *
 * Debug：控制台 `AI.HatredDebug 1` 会在怪头顶画 Top3 仇恨列表，
 * 帮助调试"为什么怪在攻击 A 而不是 B"。
 */
UCLASS()
class ACTIONGAME_API UBTService_PickHatredTarget : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_PickHatredTarget();

	/** 仇恨表为空时是否清空 TargetActor（默认 false：保留 VisionService 写的 Target）。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Hatred")
	bool bClearTargetWhenHatredEmpty = false;

	/**
	 * BB.TargetActor Key。默认绑 `ActionAIBlackboardKeys::TargetActor`。
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** 控制台 cvar `AI.HatredDebug` 控制是否画 Top3 仇恨列表。 */
	static int32 GDebugDrawHatred;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

private:
	/** 在怪头顶画 Top3 仇恨。由 GDebugDrawHatred 控制是否启用。 */
	void DrawHatredDebug(class AActionMonsterCharacter* Monster) const;
};
