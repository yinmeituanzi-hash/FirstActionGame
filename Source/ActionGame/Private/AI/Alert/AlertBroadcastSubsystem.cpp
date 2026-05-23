#include "AI/Alert/AlertBroadcastSubsystem.h"

#include "Char/ActionMonsterCharacter.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
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

int32 UAlertBroadcastSubsystem::BroadcastAlert(AActionMonsterCharacter* Source, AActor* Target, float Radius)
{
	if (Source == nullptr || Target == nullptr || Radius <= 0.0f)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}

	const FVector SourceLocation = Source->GetActorLocation();
	const float RadiusSq = FMath::Square(Radius);
	int32 ReceiverCount = 0;

	if (GDebugDrawAlertBroadcast > 0)
	{
		DrawDebugSphere(World, SourceLocation, Radius, 32, FColor::Cyan, false, 1.0f, 0, 2.0f);
	}

	for (TActorIterator<AActionMonsterCharacter> It(World); It; ++It)
	{
		AActionMonsterCharacter* Candidate = *It;
		if (Candidate == nullptr || Candidate == Source || !IsValid(Candidate) || Candidate->IsDead())
		{
			continue;
		}

		if (FVector::DistSquared(Candidate->GetActorLocation(), SourceLocation) > RadiusSq)
		{
			continue;
		}

		if (Candidate->ReceiveCombatAlert(Target, Source))
		{
			++ReceiverCount;
			if (GDebugDrawAlertBroadcast > 0)
			{
				DrawDebugLine(World, SourceLocation, Candidate->GetActorLocation(), FColor::Cyan, false, 1.0f, 0, 1.5f);
			}
		}
	}

	UE_LOG(
		LogAlertBroadcast,
		Log,
		TEXT("AlertBroadcast: Source=%s Target=%s Radius=%.0f Receivers=%d"),
		*GetNameSafe(Source),
		*GetNameSafe(Target),
		Radius,
		ReceiverCount);

	return ReceiverCount;
}
