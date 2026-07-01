#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_UpdateCombatMoveLocation.generated.h"

/**
 * Combat 状态下围绕目标更新走位点。
 *
 * 对齐 010 的职责边界：本 Service 只计算并写入 BB.CombatMoveLocation，
 * 不直接 MoveTo、不直接改速度。移动速度和 Strafe 状态由 UAIMoveLogicComponent
 * 以及对应 BTTask / BTService 管理。
 */
UCLASS()
class ACTIONGAME_API UBTService_UpdateCombatMoveLocation : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateCombatMoveLocation();

	/** 到点后允许刷新下一次走位点的随机间隔。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove", meta = (ClampMin = "0.0"))
	float MinUpdateInterval = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove", meta = (ClampMin = "0.0"))
	float MaxUpdateInterval = 1.5f;

	/** 优先使用 PickCombatSkill 写入的 SelectedSkillPreferredRange 作为绕目标半径。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance")
	bool bUseSelectedSkillPreferredRange = true;

	/** 使用固定绕行半径；关闭时若没有技能距离，则使用进入分支时与目标的距离。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance")
	bool bUseFixedDistance = false;

	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance", meta = (EditCondition = "bUseFixedDistance", ClampMin = "0.0"))
	float FixedDistance = 0.0f;

	/** 所有距离来源都无效时的兜底半径。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance", meta = (ClampMin = "0.0"))
	float FallbackMoveRadius = 250.0f;

	/** 径向随机偏移，避免每次踩在完全相同的圆周上。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance", meta = (ClampMin = "0.0"))
	float RandomOffset = 80.0f;

	/** 每次尝试向左/右旋转的角度步长。010 中对应 StandardAngle。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Angle", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float StandardAngle = 45.0f;

	/** 多久反转一次绕行方向。=0 表示不按时间反转。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Angle", meta = (ClampMin = "0.0"))
	float ReverseTime = 2.0f;

	/** 找点时最多尝试多少个角度步进。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Angle", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxProjectionAttempts = 5;

	/** 距离当前点小于该值时，认为到点并允许进入下一次选点。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove", meta = (ClampMin = "0.0"))
	float DetectDistance = 15.0f;

	/** 投射到 NavMesh 的搜索范围。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Nav", meta = (ClampMin = "0.0"))
	float NavProjectExtent = 250.0f;

	/** 是否要求从当前位置到候选点有可行路径。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Nav")
	bool bRequirePathToCandidate = true;

	/** 是否在两个点之间来回，适合更克制的前后试探。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove")
	bool bBackAndForthMode = false;

	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedSkillPreferredRangeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CombatMoveLocationKey;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

private:
	struct FCombatMoveMemory
	{
		float TimeUntilNextUpdate = 0.0f;
		float RemainingReverseTime = 0.0f;
		float FinalMoveRadius = -1.0f;
		FVector InitialLocation = FVector::ZeroVector;
		FVector CurrentMoveLocation = FVector::ZeroVector;
		int32 DirectionSign = 1;
		bool bFirstUpdate = true;
		bool bHasMoveLocation = false;
	};

	void InitializeMoveRadius(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const;
	bool ShouldWaitBeforeUpdating(const APawn& OwnerPawn, FCombatMoveMemory& Memory, float DeltaSeconds) const;
	bool UpdateCombatMoveLocation(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const;
	bool IsCandidateReachable(const APawn& OwnerPawn, const FVector& CandidateLocation) const;
	void WriteMoveLocation(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory, const FVector& MoveLocation) const;
	void ResetUpdateInterval(FCombatMoveMemory& Memory) const;
};
