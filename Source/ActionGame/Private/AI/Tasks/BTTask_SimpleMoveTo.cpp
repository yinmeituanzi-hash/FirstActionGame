#include "AI/Tasks/BTTask_SimpleMoveTo.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Char/ActionMonsterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTTask_SimpleMoveTo::UBTTask_SimpleMoveTo()
{
	NodeName = TEXT("Simple Move To");
	bNotifyTaskFinished = true;
	bCreateNodeInstance = true;

	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SimpleMoveTo, TargetActorKey), AActor::StaticClass());
}

void UBTTask_SimpleMoveTo::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UBTTask_SimpleMoveTo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ApplyMoveSettings(OwnerComp);

	const EBTNodeResult::Type Result = Super::ExecuteTask(OwnerComp, NodeMemory);
	if (Result != EBTNodeResult::InProgress)
	{
		ClearMoveSettings(OwnerComp);
	}

	return Result;
}

void UBTTask_SimpleMoveTo::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	ClearMoveSettings(OwnerComp);
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

EBTNodeResult::Type UBTTask_SimpleMoveTo::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ClearMoveSettings(OwnerComp);
	return Super::AbortTask(OwnerComp, NodeMemory);
}

FString UBTTask_SimpleMoveTo::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("%s\nMoveType: %s SpeedOverride: %.1f"),
		*Super::GetStaticDescription(),
		*StaticEnum<EAIMoveTypeState>()->GetNameStringByValue(static_cast<int64>(MoveTypeState)),
		MaxSpeedOverride);
}

void UBTTask_SimpleMoveTo::ApplyMoveSettings(UBehaviorTreeComponent& OwnerComp)
{
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner != nullptr ? Cast<AActionMonsterCharacter>(AIOwner->GetPawn()) : nullptr;
	if (Monster == nullptr)
	{
		return;
	}

	CachedMonster = Monster;
	bCachedUseControllerRotationYaw = Monster->bUseControllerRotationYaw;
	bHasCachedMovementSettings = true;

	if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
	{
		bCachedOrientRotationToMovement = Movement->bOrientRotationToMovement;
		Movement->bOrientRotationToMovement = bOrientRotationToMovement;
	}

	Monster->bUseControllerRotationYaw = bUseControllerRotationYaw;

	if (UAIMoveLogicComponent* MoveLogic = Monster->GetAIMoveLogicComponent())
	{
		MoveLogic->BeginBTMove(MoveTypeState, MaxSpeedOverride, MaxAccelerationOverride);
	}

	if (bFocusTargetActor && AIOwner != nullptr)
	{
		const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
		AActor* TargetActor = Blackboard != nullptr ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
		if (TargetActor != nullptr)
		{
			AIOwner->SetFocus(TargetActor, EAIFocusPriority::Gameplay);
		}
	}
}

void UBTTask_SimpleMoveTo::ClearMoveSettings(UBehaviorTreeComponent& OwnerComp)
{
	AActionMonsterCharacter* Monster = CachedMonster.Get();
	if (Monster == nullptr)
	{
		return;
	}

	if (UAIMoveLogicComponent* MoveLogic = Monster->GetAIMoveLogicComponent())
	{
		MoveLogic->EndBTMove();
	}

	if (bHasCachedMovementSettings)
	{
		Monster->bUseControllerRotationYaw = bCachedUseControllerRotationYaw;
		if (UCharacterMovementComponent* Movement = Monster->GetCharacterMovement())
		{
			Movement->bOrientRotationToMovement = bCachedOrientRotationToMovement;
		}
	}

	if (bFocusTargetActor)
	{
		if (AAIController* AIOwner = OwnerComp.GetAIOwner())
		{
			AIOwner->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}

	CachedMonster.Reset();
	bHasCachedMovementSettings = false;
}
