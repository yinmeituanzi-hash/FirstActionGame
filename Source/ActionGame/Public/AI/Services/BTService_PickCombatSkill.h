#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_PickCombatSkill.generated.h"

USTRUCT(BlueprintType)
struct FActionAICombatSkillCandidate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI")
	FName SkillId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI", meta = (ClampMin = "0"))
	int32 Weight = 1;
};

/**
 * Combat 分支选招服务。
 *
 * 只负责把当前最适合尝试的 SkillId 和释放距离写入 Blackboard。
 * 真正释放仍由 BTTask_UseSkill 执行，让“选招”和“执行技能”保持解耦。
 */
UCLASS()
class ACTIONGAME_API UBTService_PickCombatSkill : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_PickCombatSkill();

	/** 候选技能池。权重只在多个技能都可释放时参与加权随机。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Skill")
	TArray<FActionAICombatSkillCandidate> CandidateSkills;

	/** 技能没有配置 ReleaseRange 时使用的临时默认释放距离。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Skill", meta = (ClampMin = "0.0"))
	float DefaultMinReleaseRange = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Action|AI|Skill", meta = (ClampMin = "0.0"))
	float DefaultMaxReleaseRange = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Action|AI|Skill", meta = (ClampMin = "0.0"))
	float DefaultPreferredReleaseRange = 160.0f;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedSkillIdKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsSelectedSkillInRangeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector ShouldApproachSelectedSkillKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedSkillMinRangeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedSkillMaxRangeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedSkillPreferredRangeKey;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

private:
	struct FRuntimeCandidate
	{
		FName SkillId = NAME_None;
		int32 Weight = 0;
		float MinRange = 0.0f;
		float MaxRange = 0.0f;
		float PreferredRange = 0.0f;
		float CooldownRemaining = 0.0f;
		bool bCanUseNow = false;
		bool bInRange = false;
	};

	void ClearSelection(UBlackboardComponent& Blackboard) const;
	void WriteSelection(UBlackboardComponent& Blackboard, const FRuntimeCandidate& Candidate, float DistanceToTarget) const;
	static const FRuntimeCandidate* PickWeighted(const TArray<FRuntimeCandidate>& Candidates);
};
