#include "AI/Services/BTService_SetCombatMoveMode.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Char/ActionMonsterCharacter.h"

UBTService_SetCombatMoveMode::UBTService_SetCombatMoveMode()
{
	NodeName = TEXT("Set Combat Move Mode");
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
	Interval = 0.5f;
	RandomDeviation = 0.0f;
}

void UBTService_SetCombatMoveMode::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner != nullptr ? Cast<AActionMonsterCharacter>(AIOwner->GetPawn()) : nullptr;
	UAIMoveLogicComponent* MoveLogic = Monster != nullptr ? Monster->GetAIMoveLogicComponent() : nullptr;
	if (MoveLogic != nullptr)
	{
		MoveLogic->SetCombatMoveMode(CombatMoveMode);
	}
}

void UBTService_SetCombatMoveMode::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner != nullptr ? Cast<AActionMonsterCharacter>(AIOwner->GetPawn()) : nullptr;
	UAIMoveLogicComponent* MoveLogic = Monster != nullptr ? Monster->GetAIMoveLogicComponent() : nullptr;
	if (MoveLogic != nullptr)
	{
		MoveLogic->ClearCombatMoveMode(CombatMoveMode);
	}

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

FString UBTService_SetCombatMoveMode::GetStaticDescription() const
{
	return FString::Printf(TEXT("CombatMoveMode: %s"), *StaticEnum<EAICombatMoveMode>()->GetNameStringByValue(static_cast<int64>(CombatMoveMode)));
}
