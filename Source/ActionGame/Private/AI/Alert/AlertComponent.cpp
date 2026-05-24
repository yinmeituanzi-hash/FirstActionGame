#include "AI/Alert/AlertComponent.h"

#include "AI/ActionAIBlackboardKeys.h"
#include "AI/Alert/AlertBroadcastSubsystem.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Char/ActionCharacterBase.h"
#include "Char/ActionMonsterCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlertComponent, Log, All);

UAlertComponent::UAlertComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAlertComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UAlertBroadcastSubsystem* AlertSubsystem = UAlertBroadcastSubsystem::Get(this))
	{
		AlertSubsystem->RegisterAlertComponent(this);
	}

	ApplyAlertStateMovementSettings();
}

void UAlertComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAlertBroadcastSubsystem* AlertSubsystem = UAlertBroadcastSubsystem::Get(this))
	{
		AlertSubsystem->UnregisterAlertComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

AActionMonsterCharacter* UAlertComponent::GetOwnerMonster() const
{
	return Cast<AActionMonsterCharacter>(GetOwner());
}

void UAlertComponent::SetAlertState(EAIAlertState NewState)
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (Monster != nullptr && Monster->IsDead())
	{
		NewState = EAIAlertState::Idle;
	}

	if (AlertState == NewState)
	{
		ApplyAlertStateMovementSettings();
		return;
	}

	const bool bIsUpgrade = static_cast<uint8>(NewState) > static_cast<uint8>(AlertState);
	if (!bIsUpgrade && (Monster == nullptr || !Monster->IsDead()) && AlertChangeCooldown > 0.0f)
	{
		const UWorld* World = GetWorld();
		const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
		const float Elapsed = Now - LastAlertStateChangeTime;
		if (Elapsed < AlertChangeCooldown)
		{
			UE_LOG(
				LogAlertComponent,
				Verbose,
				TEXT("AlertComponent: downgrade %d -> %d suppressed (cooldown %.2fs remaining). Owner=%s"),
				static_cast<uint8>(AlertState),
				static_cast<uint8>(NewState),
				AlertChangeCooldown - Elapsed,
				*GetNameSafe(GetOwner()));
			return;
		}
	}

	const EAIAlertState OldState = AlertState;
	AlertState = NewState;
	ApplyAlertStateMovementSettings();

	if (const UWorld* World = GetWorld())
	{
		LastAlertStateChangeTime = World->GetTimeSeconds();
	}

	UE_LOG(
		LogAlertComponent,
		Log,
		TEXT("AlertComponent: AlertState changed %d -> %d. Owner=%s"),
		static_cast<uint8>(OldState),
		static_cast<uint8>(NewState),
		*GetNameSafe(GetOwner()));

	OnAlertStateChanged.Broadcast(OldState, NewState);
}

void UAlertComponent::SetLastNoiseLocation(const FVector& InLocation)
{
	LastNoiseLocation = InLocation;

	if (const UWorld* World = GetWorld())
	{
		LastNoiseTime = World->GetTimeSeconds();
	}
}

bool UAlertComponent::TryBroadcastCombatAlert(AActor* Target)
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (!bEnableAlertBroadcast || Monster == nullptr || Monster->IsDead() || Target == nullptr || AlertBroadcastRadius <= 0.0f)
	{
		return false;
	}

	if (const AActionCharacterBase* TargetChar = Cast<AActionCharacterBase>(Target))
	{
		if (TargetChar->IsDead())
		{
			return false;
		}
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (AlertBroadcastCooldown > 0.0f && Now - LastAlertBroadcastTime < AlertBroadcastCooldown)
	{
		UE_LOG(
			LogAlertComponent,
			Verbose,
			TEXT("AlertComponent: broadcast suppressed by cooldown %.2fs. Owner=%s"),
			AlertBroadcastCooldown - (Now - LastAlertBroadcastTime),
			*GetNameSafe(GetOwner()));
		return false;
	}

	UAlertBroadcastSubsystem* AlertSubsystem = UAlertBroadcastSubsystem::Get(this);
	if (AlertSubsystem == nullptr)
	{
		return false;
	}

	LastAlertBroadcastTime = Now;
	const int32 ReceiverCount = AlertSubsystem->BroadcastAlert(this, Target, AlertBroadcastRadius);
	return ReceiverCount > 0;
}

bool UAlertComponent::ReceiveCombatAlert(AActor* Target, UAlertComponent* Source)
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	AActionMonsterCharacter* SourceMonster = Source != nullptr ? Source->GetOwnerMonster() : nullptr;
	if (Target == nullptr || Source == nullptr || Source == this || Monster == nullptr || SourceMonster == nullptr || Monster->IsDead())
	{
		return false;
	}

	if (AlertState == EAIAlertState::Combat)
	{
		return false;
	}

	if (const AActionCharacterBase* TargetChar = Cast<AActionCharacterBase>(Target))
	{
		if (TargetChar->IsDead())
		{
			return false;
		}
	}

	AAIController* AIController = Cast<AAIController>(Monster->GetController());
	UBlackboardComponent* Blackboard = AIController != nullptr ? AIController->GetBlackboardComponent() : nullptr;
	if (Blackboard != nullptr)
	{
		Blackboard->SetValueAsObject(ActionAIBlackboardKeys::TargetActor, Target);
		Blackboard->SetValueAsVector(ActionAIBlackboardKeys::TargetLocation, Target->GetActorLocation());
		Blackboard->SetValueAsBool(ActionAIBlackboardKeys::IsInAttackRange, Monster->IsTargetInAttackRange(Target));
	}

	// Broadcast is a confirmed enemy callout, not just a suspicious sound.
	// Give it a tiny hatred entry so VisionUpdate will not immediately clear
	// TargetActor on receivers that have not personally seen the player yet.
	Monster->AddHatred(Target, 1.0f);
	SetAlertState(EAIAlertState::Combat);

	UE_LOG(
		LogAlertComponent,
		Log,
		TEXT("AlertComponent: received combat alert. Owner=%s Source=%s Target=%s"),
		*GetNameSafe(Monster),
		*GetNameSafe(SourceMonster),
		*GetNameSafe(Target));

	return true;
}

void UAlertComponent::ApplyAlertStateMovementSettings()
{
	AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (Monster == nullptr || Monster->IsDead())
	{
		return;
	}

	UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
	if (Movement == nullptr)
	{
		return;
	}

	float TargetSpeed = IdleMaxWalkSpeed;
	switch (AlertState)
	{
	case EAIAlertState::Alert:
		TargetSpeed = AlertMaxWalkSpeed;
		break;
	case EAIAlertState::Combat:
		TargetSpeed = CombatMaxWalkSpeed;
		break;
	case EAIAlertState::Idle:
	default:
		TargetSpeed = IdleMaxWalkSpeed;
		break;
	}

	Movement->MaxWalkSpeed = TargetSpeed;
}
