#include "AI/Services/BTService_AlertStateTick.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AI/ActionAITypes.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "Common/ActionGameplayTags.h"

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
	IsBlockedKey.SelectedKeyName = ActionAIBlackboardKeys::IsBlocked;
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
		IsBlockedKey.ResolveSelectedKey(*BBAsset);
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

	// Day 6: Tag → BB.IsBlocked 单向同步。Ragdoll/受击期间 HitPhysicsComponent
	// 挂 Block.AIControl Tag，这里同步到 BB；BT 根 Decorator 在 IsBlocked=true 时整树跳过。
	// 死亡走 BrainComponent->StopLogic 永久停，不走 Tag 路径。
	const bool bBlocked = Monster->HasActionTag(ActionGameplayTags::Block_AIControl);
	Blackboard->SetValueAsBool(IsBlockedKey.SelectedKeyName, bBlocked);
	if (bBlocked)
	{
		// 阻塞期间不更新 AlertState/TargetActor 等业务字段，避免 Ragdoll 中飞行轨迹被误判为"看到玩家"。
		return;
	}

	// 听觉事件发生在 BT 外部：NoiseListener 会先写到 Monster 的运行时字段。
	// 这里集中同步到 Blackboard，让 Alert 分支的 MoveTo(LastNoiseLocation) 有真实目标点。
	if (Monster->GetLastNoiseTime() > -999.0f)
	{
		Blackboard->SetValueAsVector(LastNoiseLocationKey.SelectedKeyName, Monster->GetLastNoiseLocation());
	}

	const EAIAlertState PreviousState = Monster->GetAlertState();
	if (Memory->LastObservedState != static_cast<uint8>(PreviousState))
	{
		Memory->AlertElapsedTime = 0.0f;
		Memory->LastObservedState = static_cast<uint8>(PreviousState);
	}

	UObject* TargetObject = Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName);
	AActor* TargetActor = Cast<AActor>(TargetObject);
	if (TargetActor != nullptr && !TargetActor->IsPendingKillPending())
	{
		const bool bEnteredCombat = PreviousState != EAIAlertState::Combat;
		Monster->SetAlertState(EAIAlertState::Combat);
		if (bEnteredCombat)
		{
			Monster->TryBroadcastCombatAlert(TargetActor);
		}
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
