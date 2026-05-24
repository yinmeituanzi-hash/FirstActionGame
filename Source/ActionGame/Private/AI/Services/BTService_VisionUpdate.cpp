#include "AI/Services/BTService_VisionUpdate.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UBTService_VisionUpdate::UBTService_VisionUpdate()
{
	NodeName = TEXT("Vision Update");

	// Service 会挂在根 Selector 上，所有怪如果同一帧感知会造成尖峰。
	// RandomDeviation 让不同 AI 的感知 Tick 自然错开一点。
	Interval = 0.2f;
	RandomDeviation = 0.05f;
	bNotifyBecomeRelevant = true;

	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
	TargetLocationKey.SelectedKeyName = ActionAIBlackboardKeys::TargetLocation;
	IsInAttackRangeKey.SelectedKeyName = ActionAIBlackboardKeys::IsInAttackRange;
}

void UBTService_VisionUpdate::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		// FBlackboardKeySelector 在编辑器里显示的是名字，但运行时访问靠解析后的 KeyID。
		// 自定义 BT 节点只要自己持有 KeySelector，就需要在 InitializeFromAsset 里 Resolve。
		TargetActorKey.ResolveSelectedKey(*BBAsset);
		TargetLocationKey.ResolveSelectedKey(*BBAsset);
		IsInAttackRangeKey.ResolveSelectedKey(*BBAsset);
	}
}

void UBTService_VisionUpdate::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	FVisionUpdateMemory* Memory = reinterpret_cast<FVisionUpdateMemory*>(NodeMemory);

	// 第一次进入树时先视为"已经丢失足够久"。
	// 这样如果玩家一开始不在视野内，TargetActor 会立即保持为空，BT 直接走 Patrol。
	Memory->TimeSinceLastSeen = LostTargetGraceTime;
}

void UBTService_VisionUpdate::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	FVisionUpdateMemory* Memory = reinterpret_cast<FVisionUpdateMemory*>(NodeMemory);
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner != nullptr ? Cast<AActionMonsterCharacter>(AIOwner->GetCharacter()) : nullptr;
	if (Blackboard == nullptr || Monster == nullptr || Monster->IsDead())
	{
		return;
	}

	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(Monster, 0);
	if (Player != nullptr && !Player->IsPendingKillPending() && CanSeeTarget(*Monster, *Player))
	{
		Memory->TimeSinceLastSeen = 0.0f;
		Blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, Player);
		Blackboard->SetValueAsVector(TargetLocationKey.SelectedKeyName, Player->GetActorLocation());
		Blackboard->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, Monster->IsTargetInAttackRange(Player));
		return;
	}

	Memory->TimeSinceLastSeen += DeltaSeconds;
	if (Memory->TimeSinceLastSeen >= LostTargetGraceTime)
	{
		AActor* HatredTarget = Monster->GetHighestHatredTarget();
		if (HatredTarget != nullptr)
		{
			Blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, HatredTarget);
			Blackboard->SetValueAsVector(TargetLocationKey.SelectedKeyName, HatredTarget->GetActorLocation());
			Blackboard->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, Monster->IsTargetInAttackRange(HatredTarget));
			return;
		}

		ClearTarget(*Blackboard);
	}
	else
	{
		// 宽限时间内保留旧 TargetActor，避免玩家短暂穿过柱子/墙角时 AI 立刻退出 Combat。
		// 但攻击距离需要继续刷新，否则可能在遮挡宽限内错误进入 Attack 分支。
		UObject* CurrentTarget = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
		Blackboard->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, Monster->IsTargetInAttackRange(Cast<AActor>(CurrentTarget)));
	}
}

uint16 UBTService_VisionUpdate::GetInstanceMemorySize() const
{
	return sizeof(FVisionUpdateMemory);
}

FString UBTService_VisionUpdate::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("VisionUpdate: radius %.0f, half angle %.0f, LOS %s"),
		SightRadius,
		SightHalfAngle,
		bRequireLineOfSight ? TEXT("on") : TEXT("off"));
}

bool UBTService_VisionUpdate::CanSeeTarget(const AActionMonsterCharacter& Monster, const AActor& Target) const
{
	const FVector MonsterLocation = Monster.GetActorLocation();
	const FVector TargetLocation = Target.GetActorLocation();
	const FVector ToTarget = TargetLocation - MonsterLocation;
	const float Distance2D = ToTarget.Size2D();
	if (Distance2D > SightRadius)
	{
		return false;
	}

	const FVector Direction2D = ToTarget.GetSafeNormal2D();
	if (Direction2D.IsNearlyZero())
	{
		return true;
	}

	const float Dot = FVector::DotProduct(Monster.GetActorForwardVector().GetSafeNormal2D(), Direction2D);
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));
	// Dot 小于 cos(半角) 表示目标落在视锥外；这里只看 2D，避免上下坡影响水平视野。
	if (Dot < CosHalfAngle)
	{
		return false;
	}

	if (!bRequireLineOfSight)
	{
		return true;
	}

	UWorld* World = Monster.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ActionAIVisionTrace), false, &Monster);
	// 这里忽略 Target：我们只关心 Monster 和 Target 之间有没有墙/地形挡住。
	// 如果不忽略 Target，射线打到玩家 Capsule 反而会被当成"被阻挡"。
	QueryParams.AddIgnoredActor(&Target);

	const FVector TraceStart = MonsterLocation + FVector(0.0f, 0.0f, 50.0f);
	const FVector TraceEnd = TargetLocation + FVector(0.0f, 0.0f, 50.0f);
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, VisionTraceChannel, QueryParams);
	return !bBlocked;
}

void UBTService_VisionUpdate::ClearTarget(UBlackboardComponent& Blackboard) const
{
	Blackboard.ClearValue(TargetActorKey.SelectedKeyName);
	Blackboard.SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, false);
}
