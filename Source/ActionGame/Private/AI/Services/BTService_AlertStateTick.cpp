#include "AI/Services/BTService_AlertStateTick.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AI/ActionAITypes.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Char/ActionMonsterCharacter.h"

UBTService_AlertStateTick::UBTService_AlertStateTick()
{
	NodeName = TEXT("Alert State Tick");
	Interval = 0.2f;
	RandomDeviation = 0.05f;
	bNotifyBecomeRelevant = true;

	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
	TargetLocationKey.SelectedKeyName = ActionAIBlackboardKeys::TargetLocation;
	AlertStateKey.SelectedKeyName = ActionAIBlackboardKeys::AlertState;
	LastNoiseLocationKey.SelectedKeyName = ActionAIBlackboardKeys::LastNoiseLocation;
}

void UBTService_AlertStateTick::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
		TargetLocationKey.ResolveSelectedKey(*BBAsset);
		AlertStateKey.ResolveSelectedKey(*BBAsset);
		LastNoiseLocationKey.ResolveSelectedKey(*BBAsset);
	}
}

void UBTService_AlertStateTick::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	FAlertStateTickMemory* Memory = reinterpret_cast<FAlertStateTickMemory*>(NodeMemory);
	Memory->AlertElapsedTime = 0.0f;
	Memory->LastObservedState = 255;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner != nullptr ? Cast<AActionMonsterCharacter>(AIOwner->GetCharacter()) : nullptr;
	if (Blackboard != nullptr && Monster != nullptr)
	{
		SyncBlackboardState(*Blackboard, Monster->GetAlertState());
	}
}

void UBTService_AlertStateTick::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	FAlertStateTickMemory* Memory = reinterpret_cast<FAlertStateTickMemory*>(NodeMemory);
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	AAIController* AIOwner = OwnerComp.GetAIOwner();
	AActionMonsterCharacter* Monster = AIOwner != nullptr ? Cast<AActionMonsterCharacter>(AIOwner->GetCharacter()) : nullptr;
	if (Blackboard == nullptr || Monster == nullptr || Monster->IsDead())
	{
		return;
	}

	const EAIAlertState PreviousState = Monster->GetAlertState();
	if (Memory->LastObservedState != static_cast<uint8>(PreviousState))
	{
		Memory->AlertElapsedTime = 0.0f;
		Memory->LastObservedState = static_cast<uint8>(PreviousState);
	}

	const UObject* TargetObject = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	const AActor* TargetActor = Cast<AActor>(TargetObject);
	if (TargetActor != nullptr && !TargetActor->IsPendingKillPending())
	{
		Monster->SetAlertState(EAIAlertState::Combat);
		Memory->AlertElapsedTime = 0.0f;
		Memory->LastObservedState = static_cast<uint8>(EAIAlertState::Combat);
		SyncBlackboardState(*Blackboard, EAIAlertState::Combat);
		return;
	}

	if (PreviousState == EAIAlertState::Combat)
	{
		const FVector LastKnownTargetLocation = Blackboard->GetValueAsVector(TargetLocationKey.SelectedKeyName);
		const FVector SuspiciousLocation = LastKnownTargetLocation.IsNearlyZero()
			? Monster->GetActorLocation()
			: LastKnownTargetLocation;

		Monster->SetLastNoiseLocation(SuspiciousLocation);
		Blackboard->SetValueAsVector(LastNoiseLocationKey.SelectedKeyName, SuspiciousLocation);
		Monster->SetAlertState(EAIAlertState::Alert);
		Memory->AlertElapsedTime = 0.0f;
		Memory->LastObservedState = static_cast<uint8>(EAIAlertState::Alert);
		SyncBlackboardState(*Blackboard, EAIAlertState::Alert);
		return;
	}

	if (PreviousState == EAIAlertState::Alert)
	{
		Memory->AlertElapsedTime += DeltaSeconds;
		if (Memory->AlertElapsedTime >= AlertTimeout)
		{
			Monster->SetAlertState(EAIAlertState::Idle);
			Memory->AlertElapsedTime = 0.0f;
			Memory->LastObservedState = static_cast<uint8>(EAIAlertState::Idle);
		}
	}
	else
	{
		Memory->AlertElapsedTime = 0.0f;
	}

	SyncBlackboardState(*Blackboard, Monster->GetAlertState());
}

uint16 UBTService_AlertStateTick::GetInstanceMemorySize() const
{
	return sizeof(FAlertStateTickMemory);
}

FString UBTService_AlertStateTick::GetStaticDescription() const
{
	return FString::Printf(TEXT("AlertStateTick: Alert timeout %.1fs"), AlertTimeout);
}

void UBTService_AlertStateTick::SyncBlackboardState(UBlackboardComponent& Blackboard, EAIAlertState State) const
{
	Blackboard.SetValueAsEnum(AlertStateKey.SelectedKeyName, static_cast<uint8>(State));
}
