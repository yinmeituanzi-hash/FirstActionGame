#include "AI/Tasks/BTTask_FaceTarget.h"

#include "AIController.h"
#include "AI/ActionAIBlackboardKeys.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Pawn.h"

UBTTask_FaceTarget::UBTTask_FaceTarget()
{
	NodeName = TEXT("Face Target");

	// bNotifyTick = true 才会调 TickTask；ExecuteTask 返回 InProgress 时也需要这个。
	bNotifyTick = true;

	// BB Key 默认绑到我们约定的 Key 名上。编辑器里仍可改。
	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
	TargetLocationKey.SelectedKeyName = ActionAIBlackboardKeys::TargetLocation;
}

void UBTTask_FaceTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	// 把 KeySelector 解析到对应 Blackboard Key 的运行时句柄。
	// 没这一步的话 NodeName 解析为 NAME_None，运行时取不到值。
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
		TargetLocationKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_FaceTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	FBTFaceTargetMemory* Memory = reinterpret_cast<FBTFaceTargetMemory*>(NodeMemory);
	Memory->ElapsedTime = 0.0f;

	// 没有有效目标 → 直接 Success（让 BT 流程继续，不挂死在这里）。
	FVector TargetLoc;
	if (!TryGetTargetLocation(OwnerComp, TargetLoc))
	{
		return EBTNodeResult::Succeeded;
	}

	// 已经对准了 → 立刻 Success，省一帧 Tick。
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	APawn* Pawn = AIOwner ? AIOwner->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return EBTNodeResult::Failed;
	}

	const FVector Direction = (TargetLoc - Pawn->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return EBTNodeResult::Succeeded;
	}

	const float DesiredYaw = Direction.Rotation().Yaw;
	const float CurrentYaw = Pawn->GetActorRotation().Yaw;
	if (FMath::Abs(FRotator::NormalizeAxis(DesiredYaw - CurrentYaw)) <= AcceptanceAngle)
	{
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_FaceTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	FBTFaceTargetMemory* Memory = reinterpret_cast<FBTFaceTargetMemory*>(NodeMemory);
	Memory->ElapsedTime += DeltaSeconds;

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	APawn* Pawn = AIOwner ? AIOwner->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 超时兜底：避免目标一直跑、Pawn 永远追不上的情况下卡住整棵树。
	if (Memory->ElapsedTime >= TimeoutSeconds)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	FVector TargetLoc;
	if (!TryGetTargetLocation(OwnerComp, TargetLoc))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	const FVector Direction = (TargetLoc - Pawn->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	const float DesiredYaw = Direction.Rotation().Yaw;
	const FRotator CurrentRotation = Pawn->GetActorRotation();
	const float DeltaYaw = FRotator::NormalizeAxis(DesiredYaw - CurrentRotation.Yaw);

	if (FMath::Abs(DeltaYaw) <= AcceptanceAngle)
	{
		// 最后一步对准，免得停在 AcceptanceAngle 边缘抖
		Pawn->SetActorRotation(FRotator(CurrentRotation.Pitch, DesiredYaw, CurrentRotation.Roll));
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	const float Step = FMath::Sign(DeltaYaw) * FMath::Min(RotationSpeed * DeltaSeconds, FMath::Abs(DeltaYaw));
	const FRotator NewRotation(CurrentRotation.Pitch, CurrentRotation.Yaw + Step, CurrentRotation.Roll);
	Pawn->SetActorRotation(NewRotation);
}

bool UBTTask_FaceTarget::TryGetTargetLocation(const UBehaviorTreeComponent& OwnerComp, FVector& OutLocation) const
{
	const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (BB == nullptr)
	{
		return false;
	}

	// 优先 Actor 是为了 Combat 中持续追踪移动目标；Location 是给巡逻点/噪音点这类静态目标用的兜底。
	if (TargetActorKey.SelectedKeyName != NAME_None)
	{
		if (UObject* Obj = BB->GetValueAsObject(TargetActorKey.SelectedKeyName))
		{
			if (AActor* Actor = Cast<AActor>(Obj))
			{
				OutLocation = Actor->GetActorLocation();
				return true;
			}
		}
	}

	if (TargetLocationKey.SelectedKeyName != NAME_None)
	{
		const FVector Loc = BB->GetValueAsVector(TargetLocationKey.SelectedKeyName);
		if (!Loc.IsNearlyZero())
		{
			OutLocation = Loc;
			return true;
		}
	}

	return false;
}

FString UBTTask_FaceTarget::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("FaceTarget: %s (%.0f deg/s, ±%.1f deg, timeout %.1fs)"),
		*TargetActorKey.SelectedKeyName.ToString(),
		RotationSpeed,
		AcceptanceAngle,
		TimeoutSeconds);
}
