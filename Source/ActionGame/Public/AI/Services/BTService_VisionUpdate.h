#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BTService_VisionUpdate.generated.h"

/**
 * Lightweight vision service for Sprint 4 Day 3.
 *
 * Writes:
 * - 目标角色：可见时为玩家，否则在丢失目标宽限时间后清除。
 * - 目标位置：最新可见玩家位置。
 * - IsInAttackRange: Monster->IsTargetInAttackRange(TargetActor).
 */
UCLASS()
class ACTIONGAME_API UBTService_VisionUpdate : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_VisionUpdate();

	UPROPERTY(EditAnywhere, Category = "Action|AI|Vision", meta = (ClampMin = "0.0"))
	float SightRadius = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Action|AI|Vision", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SightHalfAngle = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Action|AI|Vision")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, Category = "Action|AI|Vision")
	TEnumAsByte<ECollisionChannel> VisionTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Action|AI|Vision", meta = (ClampMin = "0.0"))
	float LostTargetGraceTime = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetLocationKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsInAttackRangeKey;

protected:
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;

private:
	struct FVisionUpdateMemory
	{
		float TimeSinceLastSeen = 0.0f;
	};

	bool CanSeeTarget(const class AActionMonsterCharacter& Monster, const AActor& Target) const;
	void ClearTarget(class UBlackboardComponent& Blackboard) const;
};
