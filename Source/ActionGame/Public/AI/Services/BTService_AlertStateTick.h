#pragma once

#include "CoreMinimal.h"
#include "AI/ActionAITypes.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_AlertStateTick.generated.h"

/**
 * Sprint 4 Day 4：把视觉/听觉等感知结果整理成 Idle / Alert / Combat 三状态。
 *
 * Day 4 先只接视觉：
 * - TargetActor 有效：Combat
 * - Combat 丢失 TargetActor：Alert，并把最后 TargetLocation 作为可疑点
 * - Alert 超时仍没重新看到玩家：Idle
 *
 * Day 5 听觉系统接入后，会从外部写 LastNoiseLocation 并 SetAlertState(Alert)。
 */
UCLASS()
class ACTIONGAME_API UBTService_AlertStateTick : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_AlertStateTick();

	/** Alert 状态持续多久后回到 Idle。 */
	UPROPERTY(EditAnywhere, Category = "Action|AI|Alert", meta = (ClampMin = "0.0"))
	float AlertTimeout = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector AlertStateKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector LastNoiseLocationKey;

	/**
	 * 用于把 Owner 的 Block.AIControl Tag 同步到 BB（Day 6 起启用）。
	 * BT 根 Selector 上挂 Decorator: "IsBlocked Is NOT Set, Observer Aborts: Self"。
	 */
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsBlockedKey;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

private:
	struct FAlertStateTickMemory
	{
		float AlertElapsedTime = 0.0f;
		uint8 LastObservedState = 255;
	};

	void SyncBlackboardState(UBlackboardComponent& Blackboard, EAIAlertState State) const;
};
