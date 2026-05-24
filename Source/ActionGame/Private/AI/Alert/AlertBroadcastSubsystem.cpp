#include "AI/Alert/AlertBroadcastSubsystem.h"

#include "AI/Alert/AlertComponent.h"
#include "Char/ActionMonsterCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlertBroadcast, Log, All);

int32 UAlertBroadcastSubsystem::GDebugDrawAlertBroadcast = 0;

static FAutoConsoleVariableRef CVarAIAlertBroadcastDebugDraw(
	TEXT("AI.AlertBroadcastDebug"),
	UAlertBroadcastSubsystem::GDebugDrawAlertBroadcast,
	TEXT("0=off, 1=draw alert broadcast radius and receiver lines."),
	ECVF_Cheat);

UAlertBroadcastSubsystem::UAlertBroadcastSubsystem()
{
}

UAlertBroadcastSubsystem* UAlertBroadcastSubsystem::Get(const UObject* WorldContext)
{
	if (WorldContext == nullptr)
	{
		return nullptr;
	}

	const UWorld* World = WorldContext->GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	return World->GetSubsystem<UAlertBroadcastSubsystem>();
}

void UAlertBroadcastSubsystem::RegisterAlertComponent(UAlertComponent* AlertComponent)
{
	if (AlertComponent != nullptr)
	{
		RegisteredAlertComponents.Add(AlertComponent);
	}
}

void UAlertBroadcastSubsystem::UnregisterAlertComponent(UAlertComponent* AlertComponent)
{
	if (AlertComponent != nullptr)
	{
		RegisteredAlertComponents.Remove(AlertComponent);
	}
}

int32 UAlertBroadcastSubsystem::BroadcastAlert(UAlertComponent* Source, AActor* Target, float Radius)
{
	if (Source == nullptr || Target == nullptr || Radius <= 0.0f)
	{
		return 0;
	}

	AActionMonsterCharacter* SourceMonster = Source->GetOwnerMonster();
	if (SourceMonster == nullptr)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}

	const FVector SourceLocation = SourceMonster->GetActorLocation();
	const float RadiusSq = FMath::Square(Radius);
	int32 ReceiverCount = 0;

	if (GDebugDrawAlertBroadcast > 0)
	{
		DrawDebugSphere(World, SourceLocation, Radius, 32, FColor::Cyan, false, 1.0f, 0, 2.0f);
	}

	for (auto It = RegisteredAlertComponents.CreateIterator(); It; ++It)
	{
		UAlertComponent* Candidate = It->Get();
		if (Candidate == nullptr || !IsValid(Candidate))
		{
			It.RemoveCurrent();
			continue;
		}

		if (Candidate == Source)
		{
			continue;
		}

		AActionMonsterCharacter* CandidateMonster = Candidate->GetOwnerMonster();
		if (CandidateMonster == nullptr || !IsValid(CandidateMonster) || CandidateMonster->IsDead())
		{
			continue;
		}

		if (FVector::DistSquared(CandidateMonster->GetActorLocation(), SourceLocation) > RadiusSq)
		{
			continue;
		}

		if (Candidate->ReceiveCombatAlert(Target, Source))
		{
			++ReceiverCount;
			if (GDebugDrawAlertBroadcast > 0)
			{
				DrawDebugLine(World, SourceLocation, CandidateMonster->GetActorLocation(), FColor::Cyan, false, 1.0f, 0, 1.5f);
			}
		}
	}

	UE_LOG(
		LogAlertBroadcast,
		Log,
		TEXT("AlertBroadcast: Source=%s Target=%s Radius=%.0f Receivers=%d"),
		*GetNameSafe(SourceMonster),
		*GetNameSafe(Target),
		Radius,
		ReceiverCount);

	return ReceiverCount;
}
