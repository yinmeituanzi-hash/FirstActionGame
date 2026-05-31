#include "AI/Significance/AISignificanceSubsystem.h"

#include "AI/Significance/AISignificanceComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UAISignificanceSubsystem* UAISignificanceSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UAISignificanceSubsystem>() : nullptr;
}

void UAISignificanceSubsystem::Tick(float DeltaTime)
{
	TimeSinceLastUpdate += DeltaTime;
	if (TimeSinceLastUpdate < UpdateInterval)
	{
		return;
	}

	TimeSinceLastUpdate = 0.0f;
	UpdateSignificance();
}

TStatId UAISignificanceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAISignificanceSubsystem, STATGROUP_Tickables);
}

bool UAISignificanceSubsystem::IsTickable() const
{
	return !IsTemplate()
		&& GetWorld() != nullptr
		&& !GetWorld()->IsNetMode(NM_Client);
}

void UAISignificanceSubsystem::RegisterComponent(UAISignificanceComponent* Component)
{
	if (Component != nullptr)
	{
		RegisteredComponents.AddUnique(Component);
	}
}

void UAISignificanceSubsystem::UnregisterComponent(UAISignificanceComponent* Component)
{
	RegisteredComponents.Remove(Component);
}

void UAISignificanceSubsystem::UpdateSignificance()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);

	for (int32 Index = RegisteredComponents.Num() - 1; Index >= 0; --Index)
	{
		UAISignificanceComponent* Component = RegisteredComponents[Index].Get();
		if (Component == nullptr)
		{
			RegisteredComponents.RemoveAtSwap(Index);
			continue;
		}

		Component->EvaluateAndApply(PlayerPawn);
	}
}
