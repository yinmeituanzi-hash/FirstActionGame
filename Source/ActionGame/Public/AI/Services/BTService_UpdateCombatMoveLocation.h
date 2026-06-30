#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_UpdateCombatMoveLocation.generated.h"

/**
 * Combat 状态下围绕目标更新走位点。
 *
 * 参考 010 的 BTService_AroundTargetUpdate：
 * - Service 只写 BB.CombatMoveLocation，不直接 MoveTo。
 * - 首次进入分支时确定绕目标半径，之后按固定角度步进找下一个点。
 * - 没到当前点前不刷新，避免移动途中频繁换目标。
 * - 支持定时反转绕行方向、径向随机偏移、来回点模式和分支内临时限速。
 */
UCLASS()
class ACTIONGAME_API UBTService_UpdateCombatMoveLocation : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateCombatMoveLocation();

	/**
	 * 到点后，允许刷新下一个走位点的随机间隔。
	 * 如果 BT Sequence 内还有 WaitAtPoint，这两个时间会共同决定停顿节奏。
	 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove", meta = (ClampMin = "0.0"))
	float MinUpdateInterval = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove", meta = (ClampMin = "0.0"))
	float MaxUpdateInterval = 1.5f;

	/** 优先使用 PickCombatSkill 写入的 SelectedSkillPreferredRange 作为绕目标半径。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance")
	bool bUseSelectedSkillPreferredRange = true;

	/** 使用固定绕行半径；关闭时若没有 SelectedSkillPreferredRange，则使用进入分支时与目标的距离。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance")
	bool bUseFixedDistance = false;

	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance", meta = (EditCondition = "bUseFixedDistance", ClampMin = "0.0"))
	float FixedDistance = 0.0f;

	/** 所有距离来源都无效时的兜底半径。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance", meta = (ClampMin = "0.0"))
	float FallbackMoveRadius = 250.0f;

	/** 径向随机偏移，避免每次都踩在完全相同的圆周上。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Distance", meta = (ClampMin = "0.0"))
	float RandomOffset = 80.0f;

	/** 每次尝试向左/右旋转的角度步长。010 里对应 StandardAngle。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Angle", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float StandardAngle = 45.0f;

	/** 多久反转一次绕行方向。<=0 表示不按时间反转。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Angle", meta = (ClampMin = "0.0"))
	float ReverseTime = 2.0f;

	/** 找点时最多尝试多少个角度步进。010 里固定尝试 5 次，这里暴露出来。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Angle", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxProjectionAttempts = 5;

	/** 距离当前点小于该值时，认为到点并允许进入下一次选点。010 里对应 DetectDis 的用法。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove", meta = (ClampMin = "0.0"))
	float DetectDistance = 100.0f;

	/** 投射到 NavMesh 的搜索范围。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Nav", meta = (ClampMin = "0.0"))
	float NavProjectExtent = 250.0f;

	/** 是否要求从当前位置到候选点有可行路径。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Nav")
	bool bRequirePathToCandidate = true;

	/** 是否在两个点之间来回，适合做更克制的前后试探。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove")
	bool bBackAndForthMode = false;

	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove")
	bool bDrawDebug = false;

	/** CombatMove 分支生效时临时限制移动速度，避免 CD 走位看起来像冲刺。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Speed")
	bool bOverrideMaxWalkSpeed = true;

	/** 默认接近 Idle / Patrol 速度。退出 CombatMove 分支时会恢复进入分支前的速度。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|CombatMove|Speed", meta = (EditCondition = "bOverrideMaxWalkSpeed", ClampMin = "0.0"))
	float CombatMoveMaxWalkSpeed = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector SelectedSkillPreferredRangeKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CombatMoveLocationKey;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

private:
	struct FCombatMoveMemory
	{
		float TimeUntilNextUpdate = 0.0f;
		float RemainingReverseTime = 0.0f;
		float FinalMoveRadius = -1.0f;
		float PreviousMaxWalkSpeed = 0.0f;
		FVector InitialLocation = FVector::ZeroVector;
		FVector CurrentMoveLocation = FVector::ZeroVector;
		int32 DirectionSign = 1;
		bool bFirstUpdate = true;
		bool bHasSpeedOverride = false;
		bool bHasMoveLocation = false;
	};

	void InitializeMoveRadius(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const;
	bool ShouldWaitBeforeUpdating(const APawn& OwnerPawn, FCombatMoveMemory& Memory, float DeltaSeconds) const;
	bool UpdateCombatMoveLocation(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const;
	bool IsCandidateReachable(const APawn& OwnerPawn, const FVector& CandidateLocation) const;
	void WriteMoveLocation(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory, const FVector& MoveLocation) const;
	void ResetUpdateInterval(FCombatMoveMemory& Memory) const;
	void ApplySpeedOverride(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const;
	void RestoreSpeedOverride(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const;
};
