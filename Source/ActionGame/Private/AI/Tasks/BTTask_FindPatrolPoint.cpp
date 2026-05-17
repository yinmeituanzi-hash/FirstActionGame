#include "AI/Tasks/BTTask_FindPatrolPoint.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"

UBTTask_FindPatrolPoint::UBTTask_FindPatrolPoint()
{
	NodeName = TEXT("Find Patrol Point");

	HomeLocationKey.SelectedKeyName = ActionAIBlackboardKeys::HomeLocation;
	TargetLocationKey.SelectedKeyName = ActionAIBlackboardKeys::TargetLocation;

	// 限制编辑器下拉列表只显示 Vector Key，避免误选 Object/Bool Key 后运行时才失败。
	HomeLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindPatrolPoint, HomeLocationKey));
	TargetLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FindPatrolPoint, TargetLocationKey));
}

void UBTTask_FindPatrolPoint::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		// 和 Service 一样，KeySelector 需要绑定到当前 BT 使用的 Blackboard 资产。
		HomeLocationKey.ResolveSelectedKey(*BBAsset);
		TargetLocationKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_FindPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	APawn* Pawn = AIOwner != nullptr ? AIOwner->GetPawn() : nullptr;
	if (Blackboard == nullptr || Pawn == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	UWorld* World = Pawn->GetWorld();
	UNavigationSystemV1* NavSys = World != nullptr ? UNavigationSystemV1::GetCurrent(World) : nullptr;
	if (NavSys == nullptr || PatrolRadius <= 0.0f)
	{
		return EBTNodeResult::Failed;
	}

	const FVector HomeLocation = Blackboard->GetValueAsVector(HomeLocationKey.SelectedKeyName);
	const FVector CurrentLocation = Pawn->GetActorLocation();

	FNavLocation BestLocation;
	bool bHasAnyReachablePoint = false;

	// 多试几次是为了尽量避开"原地踏步"的小位移点。
	// 但如果半径内只有近点可达，也不能让 Patrol 整个失败，所以会保留第一个可达点做兜底。
	const int32 AttemptCount = FMath::Max(MaxAttempts, 1);
	for (int32 AttemptIndex = 0; AttemptIndex < AttemptCount; ++AttemptIndex)
	{
		FNavLocation Candidate;
		if (!NavSys->GetRandomReachablePointInRadius(HomeLocation, PatrolRadius, Candidate))
		{
			continue;
		}

		if (!bHasAnyReachablePoint)
		{
			BestLocation = Candidate;
			bHasAnyReachablePoint = true;
		}

		if (FVector::Dist2D(Candidate.Location, CurrentLocation) >= MinDistanceFromCurrent)
		{
			Blackboard->SetValueAsVector(TargetLocationKey.SelectedKeyName, Candidate.Location);
			return EBTNodeResult::Succeeded;
		}
	}

	if (bHasAnyReachablePoint)
	{
		Blackboard->SetValueAsVector(TargetLocationKey.SelectedKeyName, BestLocation.Location);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

FString UBTTask_FindPatrolPoint::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("FindPatrolPoint: %s -> %s, radius %.0f"),
		*HomeLocationKey.SelectedKeyName.ToString(),
		*TargetLocationKey.SelectedKeyName.ToString(),
		PatrolRadius);
}
