#include "AI/Budget/AIBudgetComponent.h"

#include "AI/Budget/AIBudgetSubsystem.h"
#include "AI/Significance/AISignificanceComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "Common/ActionGameplayTags.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAIBudget, Log, All);

UAIBudgetComponent::UAIBudgetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAIBudgetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UAIBudgetSubsystem* Subsystem = UAIBudgetSubsystem::Get(this))
	{
		Subsystem->RegisterComponent(this);
	}
}

void UAIBudgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAIBudgetSubsystem* Subsystem = UAIBudgetSubsystem::Get(this))
	{
		Subsystem->UnregisterComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

AActionMonsterCharacter* UAIBudgetComponent::GetOwnerMonster() const
{
	return Cast<AActionMonsterCharacter>(GetOwner());
}

bool UAIBudgetComponent::CanReduceWork() const
{
	return GetProtectionReason().IsEmpty();
}

void UAIBudgetComponent::ApplyBudgetEnabled(bool bEnabled, EAIBudgetAllocationReason Reason, bool bForce)
{
	if (!bEnabled && !CanReduceWork())
	{
		bEnabled = true;
		Reason = EAIBudgetAllocationReason::Protected;
	}

	if (bBudgetEnabled == bEnabled)
	{
		AllocationReason = Reason;
		return;
	}

	const UWorld* World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (!bForce && !bEnabled && DisableThrottleSeconds > 0.0f && Now - LastStateChangeTime < DisableThrottleSeconds)
	{
		AllocationReason = EAIBudgetAllocationReason::Throttled;
		return;
	}

	bBudgetEnabled = bEnabled;
	AllocationReason = Reason;
	LastStateChangeTime = Now;

	if (UAISignificanceComponent* Significance = GetSignificanceComponent())
	{
		Significance->ApplyBudgetEnabled(bBudgetEnabled);
	}

	UE_LOG(
		LogAIBudget,
		Verbose,
		TEXT("AIBudget: Owner=%s Enabled=%d Reason=%d"),
		*GetNameSafe(GetOwner()),
		bBudgetEnabled ? 1 : 0,
		static_cast<uint8>(AllocationReason));
}

FString UAIBudgetComponent::GetProtectionReason() const
{
	const AActionMonsterCharacter* Monster = GetOwnerMonster();
	if (Monster == nullptr)
	{
		return TEXT("Invalid");
	}

	if (Monster->IsDead())
	{
		return TEXT("Dead");
	}

	if (Monster->IsAttacking())
	{
		return TEXT("Attack");
	}

	if (Monster->IsBeingLockedOn())
	{
		return TEXT("LockOn");
	}

	if (Monster->HasActionTag(ActionGameplayTags::Block_AIControl))
	{
		return TEXT("AIBlock");
	}

	if (Monster->HasActionTag(ActionGameplayTags::Block_HitReact))
	{
		return TEXT("HitReact");
	}

	if (Monster->HasActionTag(ActionGameplayTags::State_Ragdoll))
	{
		return TEXT("Ragdoll");
	}

	return FString();
}

UAISignificanceComponent* UAIBudgetComponent::GetSignificanceComponent() const
{
	const AActionMonsterCharacter* Monster = GetOwnerMonster();
	return Monster != nullptr ? Monster->GetSignificanceComponent() : nullptr;
}
