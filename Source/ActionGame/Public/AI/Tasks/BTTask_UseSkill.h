#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_UseSkill.generated.h"

USTRUCT()
struct FBTUseSkillMemory
{
	GENERATED_BODY()

	float ElapsedTime = 0.0f;
	bool bSkillStarted = false;
};

/**
 * 行为树技能节点。
 *
 * BT 只负责选择 SkillId、传入目标并等待技能结束；具体 Montage、命中、特效、冷却都归 SkillSystem。
 */
UCLASS()
class ACTIONGAME_API UBTTask_UseSkill : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_UseSkill();

	/** 要释放的技能 Id，对应 DT_ActionSkills 的 RowName / SkillId。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI")
	FName SkillId = NAME_None;

	/** 从 Blackboard 读取目标并作为 OptionalTarget 传给 SkillComponent。 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	/** 等待技能启动/结束的最大时间。超时会让 BT 继续，避免行为树卡死。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI", meta = (ClampMin = "0.5"))
	float MaxExecutionTime = 4.0f;

	/** Task 被外部 Abort 时是否强制停止当前技能。默认 false：攻击挥出后即使目标离开范围也让动作自然收完。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI")
	bool bStopSkillOnAbort = false;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTUseSkillMemory); }
	virtual FString GetStaticDescription() const override;

private:
	AActor* GetBlackboardTarget(const UBehaviorTreeComponent& OwnerComp) const;
	bool TryStartSkill(UBehaviorTreeComponent& OwnerComp, FBTUseSkillMemory& Memory) const;
};
