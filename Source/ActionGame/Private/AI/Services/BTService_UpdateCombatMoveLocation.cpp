#include "AI/Services/BTService_UpdateCombatMoveLocation.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AI/Alert/AlertComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "DrawDebugHelpers.h"
#include "Char/ActionMonsterCharacter.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
	const FName CombatMoveSpeedOverrideSource(TEXT("CombatMove"));
}

UBTService_UpdateCombatMoveLocation::UBTService_UpdateCombatMoveLocation()
{
	NodeName = TEXT("Update Combat Move Location");
	Interval = 0.2f;
	RandomDeviation = 0.03f;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;

	TargetActorKey.SelectedKeyName = ActionAIBlackboardKeys::TargetActor;
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCombatMoveLocation, TargetActorKey), AActor::StaticClass());

	SelectedSkillPreferredRangeKey.SelectedKeyName = ActionAIBlackboardKeys::SelectedSkillPreferredRange;
	SelectedSkillPreferredRangeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCombatMoveLocation, SelectedSkillPreferredRangeKey));

	CombatMoveLocationKey.SelectedKeyName = ActionAIBlackboardKeys::CombatMoveLocation;
	CombatMoveLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTService_UpdateCombatMoveLocation, CombatMoveLocationKey));
}

void UBTService_UpdateCombatMoveLocation::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*BBAsset);
		SelectedSkillPreferredRangeKey.ResolveSelectedKey(*BBAsset);
		CombatMoveLocationKey.ResolveSelectedKey(*BBAsset);
	}
}

void UBTService_UpdateCombatMoveLocation::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	FCombatMoveMemory* Memory = reinterpret_cast<FCombatMoveMemory*>(NodeMemory);
	Memory->TimeUntilNextUpdate = 0.0f;
	Memory->RemainingReverseTime = ReverseTime;
	Memory->FinalMoveRadius = -1.0f;
	Memory->PreviousMaxWalkSpeed = 0.0f;
	Memory->InitialLocation = FVector::ZeroVector;
	Memory->CurrentMoveLocation = FVector::ZeroVector;
	Memory->DirectionSign = FMath::RandBool() ? 1 : -1;
	Memory->bFirstUpdate = true;
	Memory->bHasSpeedOverride = false;
	Memory->bHasMoveLocation = false;

	ApplySpeedOverride(OwnerComp, *Memory);
}

void UBTService_UpdateCombatMoveLocation::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	RestoreSpeedOverride(OwnerComp, *reinterpret_cast<FCombatMoveMemory*>(NodeMemory));

	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

void UBTService_UpdateCombatMoveLocation::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	APawn* OwnerPawn = AIOwner != nullptr ? AIOwner->GetPawn() : nullptr;
	if (OwnerPawn == nullptr)
	{
		return;
	}

	FCombatMoveMemory* Memory = reinterpret_cast<FCombatMoveMemory*>(NodeMemory);
	if (Memory->bFirstUpdate)
	{
		Memory->bFirstUpdate = false;
		Memory->InitialLocation = OwnerPawn->GetActorLocation();
		InitializeMoveRadius(OwnerComp, *Memory);
		UpdateCombatMoveLocation(OwnerComp, *Memory);
		return;
	}

	if (ShouldWaitBeforeUpdating(*OwnerPawn, *Memory, DeltaSeconds))
	{
		return;
	}

	if (bBackAndForthMode && Memory->bHasMoveLocation)
	{
		const FVector PreviousLocation = Memory->CurrentMoveLocation;
		WriteMoveLocation(OwnerComp, *Memory, Memory->InitialLocation);
		Memory->InitialLocation = PreviousLocation;
		ResetUpdateInterval(*Memory);
		return;
	}

	if (ReverseTime > 0.0f)
	{
		Memory->RemainingReverseTime -= DeltaSeconds;
		if (Memory->RemainingReverseTime <= 0.0f)
		{
			Memory->RemainingReverseTime = ReverseTime;
			Memory->DirectionSign *= -1;
		}
	}

	if (UpdateCombatMoveLocation(OwnerComp, *Memory))
	{
		ResetUpdateInterval(*Memory);
	}
}

uint16 UBTService_UpdateCombatMoveLocation::GetInstanceMemorySize() const
{
	return sizeof(FCombatMoveMemory);
}

FString UBTService_UpdateCombatMoveLocation::GetStaticDescription() const
{
	return FString::Printf(
		TEXT("AroundTarget: angle %.0f, reverse %.1fs, speed %.0f"),
		StandardAngle,
		ReverseTime,
		bOverrideMaxWalkSpeed ? CombatMoveMaxWalkSpeed : -1.0f);
}

void UBTService_UpdateCombatMoveLocation::InitializeMoveRadius(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const
{
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AAIController* AIOwner = OwnerComp.GetAIOwner();
	const APawn* OwnerPawn = AIOwner != nullptr ? AIOwner->GetPawn() : nullptr;
	const AActor* TargetActor = Blackboard != nullptr ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;

	float Radius = 0.0f;
	if (bUseSelectedSkillPreferredRange && Blackboard != nullptr)
	{
		Radius = Blackboard->GetValueAsFloat(SelectedSkillPreferredRangeKey.SelectedKeyName);
	}

	if (Radius <= 0.0f && bUseFixedDistance)
	{
		Radius = FixedDistance;
	}

	if (Radius <= 0.0f && OwnerPawn != nullptr && TargetActor != nullptr)
	{
		Radius = FVector::Dist2D(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());
	}

	Memory.FinalMoveRadius = FMath::Max(Radius, FallbackMoveRadius);
}

bool UBTService_UpdateCombatMoveLocation::ShouldWaitBeforeUpdating(const APawn& OwnerPawn, FCombatMoveMemory& Memory, float DeltaSeconds) const
{
	if (Memory.bHasMoveLocation)
	{
		const float DistanceToCurrentPoint = FVector::Dist2D(OwnerPawn.GetActorLocation(), Memory.CurrentMoveLocation);
		if (DistanceToCurrentPoint > DetectDistance)
		{
			return true;
		}
	}

	Memory.TimeUntilNextUpdate -= DeltaSeconds;
	return Memory.TimeUntilNextUpdate > 0.0f;
}

bool UBTService_UpdateCombatMoveLocation::UpdateCombatMoveLocation(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const
{
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const AAIController* AIOwner = OwnerComp.GetAIOwner();
	const APawn* OwnerPawn = AIOwner != nullptr ? AIOwner->GetPawn() : nullptr;
	const AActor* TargetActor = Blackboard != nullptr ? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
	if (Blackboard == nullptr || OwnerPawn == nullptr || TargetActor == nullptr)
	{
		return false;
	}

	const FVector OwnerLocation = OwnerPawn->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	FVector ToCircleForward = (OwnerLocation - TargetLocation).GetSafeNormal2D();
	if (ToCircleForward.IsNearlyZero())
	{
		ToCircleForward = -TargetActor->GetActorForwardVector().GetSafeNormal2D();
	}
	if (ToCircleForward.IsNearlyZero())
	{
		ToCircleForward = FVector::ForwardVector;
	}

	UWorld* World = OwnerPawn->GetWorld();
	UNavigationSystemV1* NavSystem = World != nullptr ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (NavSystem == nullptr)
	{
		return false;
	}

	const int32 AttemptCount = FMath::Clamp(MaxProjectionAttempts, 1, 16);
	const float Radius = FMath::Max(Memory.FinalMoveRadius, FallbackMoveRadius);
	const float AngleStep = FMath::Clamp(StandardAngle, 0.0f, 180.0f);
	const FVector ProjectExtent(NavProjectExtent, NavProjectExtent, NavProjectExtent);

	for (int32 AttemptIndex = 0; AttemptIndex < AttemptCount; ++AttemptIndex)
	{
		const float Angle = AngleStep * static_cast<float>(Memory.DirectionSign) * static_cast<float>(AttemptIndex + 1);
		const FVector CandidateDirection = ToCircleForward.RotateAngleAxis(Angle, FVector::UpVector).GetSafeNormal2D();
		const float CandidateRadius = FMath::Max(80.0f, Radius + FMath::FRandRange(-RandomOffset, RandomOffset));
		const FVector CandidateLocation = TargetLocation + CandidateDirection * CandidateRadius;

		FNavLocation ProjectedLocation;
		const bool bProjected = NavSystem->ProjectPointToNavigation(CandidateLocation, ProjectedLocation, ProjectExtent);
		const FVector DebugLocation = bProjected ? ProjectedLocation.Location : CandidateLocation;

		if (bProjected && IsCandidateReachable(*OwnerPawn, ProjectedLocation.Location))
		{
			WriteMoveLocation(OwnerComp, Memory, ProjectedLocation.Location);
			if (bDrawDebug && World != nullptr)
			{
				DrawDebugSphere(World, ProjectedLocation.Location, 24.0f, 12, FColor::Green, false, 3.0f, 0, 1.5f);
				DrawDebugLine(World, OwnerLocation, ProjectedLocation.Location, FColor::Green, false, 3.0f, 0, 1.5f);
			}
			return true;
		}

		if (bDrawDebug && World != nullptr)
		{
			DrawDebugSphere(World, DebugLocation, 18.0f, 8, FColor::Red, false, 3.0f, 0, 1.0f);
		}
	}

	return false;
}

bool UBTService_UpdateCombatMoveLocation::IsCandidateReachable(const APawn& OwnerPawn, const FVector& CandidateLocation) const
{
	if (!bRequirePathToCandidate)
	{
		return true;
	}

	UWorld* World = OwnerPawn.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		World,
		OwnerPawn.GetActorLocation(),
		CandidateLocation,
		const_cast<APawn*>(&OwnerPawn));
	return Path != nullptr && Path->IsValid() && !Path->IsPartial();
}

void UBTService_UpdateCombatMoveLocation::WriteMoveLocation(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory, const FVector& MoveLocation) const
{
	if (UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent())
	{
		Blackboard->SetValueAsVector(CombatMoveLocationKey.SelectedKeyName, MoveLocation);
	}

	Memory.CurrentMoveLocation = MoveLocation;
	Memory.bHasMoveLocation = true;
}

void UBTService_UpdateCombatMoveLocation::ResetUpdateInterval(FCombatMoveMemory& Memory) const
{
	const float ActualMin = FMath::Max(0.0f, MinUpdateInterval);
	const float ActualMax = FMath::Max(ActualMin, MaxUpdateInterval);
	Memory.TimeUntilNextUpdate = FMath::FRandRange(ActualMin, ActualMax);
}

void UBTService_UpdateCombatMoveLocation::ApplySpeedOverride(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const
{
	if (!bOverrideMaxWalkSpeed || CombatMoveMaxWalkSpeed <= 0.0f)
	{
		return;
	}

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	ACharacter* OwnerCharacter = AIOwner != nullptr ? AIOwner->GetCharacter() : nullptr;
	if (AActionMonsterCharacter* Monster = Cast<AActionMonsterCharacter>(OwnerCharacter))
	{
		if (UAlertComponent* AlertComponent = Monster->GetAlertComponent())
		{
			AlertComponent->SetMovementSpeedOverride(CombatMoveSpeedOverrideSource, CombatMoveMaxWalkSpeed);
			Memory.bHasSpeedOverride = true;
			return;
		}
	}

	UCharacterMovementComponent* Movement = OwnerCharacter != nullptr ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (Movement == nullptr)
	{
		return;
	}

	Memory.PreviousMaxWalkSpeed = Movement->MaxWalkSpeed;
	Memory.bHasSpeedOverride = true;
	Movement->MaxWalkSpeed = CombatMoveMaxWalkSpeed;
}

void UBTService_UpdateCombatMoveLocation::RestoreSpeedOverride(UBehaviorTreeComponent& OwnerComp, FCombatMoveMemory& Memory) const
{
	if (!Memory.bHasSpeedOverride)
	{
		return;
	}

	AAIController* AIOwner = OwnerComp.GetAIOwner();
	ACharacter* OwnerCharacter = AIOwner != nullptr ? AIOwner->GetCharacter() : nullptr;
	if (AActionMonsterCharacter* Monster = Cast<AActionMonsterCharacter>(OwnerCharacter))
	{
		if (UAlertComponent* AlertComponent = Monster->GetAlertComponent())
		{
			AlertComponent->ClearMovementSpeedOverride(CombatMoveSpeedOverrideSource);
			Memory.bHasSpeedOverride = false;
			return;
		}
	}

	UCharacterMovementComponent* Movement = OwnerCharacter != nullptr ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (Movement != nullptr)
	{
		Movement->MaxWalkSpeed = Memory.PreviousMaxWalkSpeed;
	}

	Memory.bHasSpeedOverride = false;
}
