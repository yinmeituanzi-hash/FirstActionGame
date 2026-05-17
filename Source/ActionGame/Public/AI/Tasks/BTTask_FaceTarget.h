#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTTask_FaceTarget.generated.h"

/**
 * 让控制的 Pawn 朝 Blackboard.TargetActor（或 TargetLocation）方向旋转。
 *
 * 行为：
 *  - 默认从 Blackboard 取 TargetActor，取 ActorLocation 做朝向。
 *  - 用 Tick 增量插值（每帧旋转 RotationSpeed 度），不是瞬切。
 *  - 当 Pawn 的 Yaw 与目标 Yaw 误差小于 AcceptanceAngle 时返回 Success。
 *  - 超时（TimeoutSeconds）也返回 Success（避免被卡住，让 BT 继续走）。
 *
 * 设计取舍：
 *  - 不直接 SetActorRotation 一帧到位，因为视觉上太突兀。
 *  - 不依赖 AIController 的 ControlRotation 平滑（那个是控制视角朝向的，
 *    会让 Pawn 跟着转，但有些场景我们想让"身体"先转、视角后跟）。
 *  - InstanceMemory 存"已用时间"，跨 Tick 累计。
 */

USTRUCT()
struct FBTFaceTargetMemory
{
	GENERATED_BODY()

	float ElapsedTime = 0.0f;
};

UCLASS()
class ACTIONGAME_API UBTTask_FaceTarget : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FaceTarget();

	/** 旋转速度（度/秒）。值越大转得越快。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI", meta = (ClampMin = "1.0"))
	float RotationSpeed = 340.0f;

	/** 角度误差小于这个值认为对准（度）。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI", meta = (ClampMin = "0.1", ClampMax = "45.0"))
	float AcceptanceAngle = 5.0f;

	/** 最大执行时间（秒）。超时无论是否对准都返回 Success，避免卡住。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI", meta = (ClampMin = "0.1"))
	float TimeoutSeconds = 1.5f;

	/**
	 * 优先 BB.TargetActor 的位置；Actor 为空时尝试 BB.TargetLocation。
	 * 这种"先 Actor 再 Location"的回退是 010 BT 节点常见模式。
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	struct FBlackboardKeySelector TargetLocationKey;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTFaceTargetMemory); }
	virtual FString GetStaticDescription() const override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

private:
	/** 从 BB 取目标位置。返回 false 表示 BB 数据无效，本帧无法决定朝向。 */
	bool TryGetTargetLocation(const UBehaviorTreeComponent& OwnerComp, FVector& OutLocation) const;
};
