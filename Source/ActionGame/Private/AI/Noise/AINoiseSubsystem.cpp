#include "AI/Noise/AINoiseSubsystem.h"

#include "AI/Noise/NoiseListenerComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAINoiseSubsystem, Log, All);

int32 UAINoiseSubsystem::GDebugDrawNoise = 0;

static FAutoConsoleVariableRef CVarAINoiseDebugDraw(
	TEXT("AI.NoiseDebug"),
	UAINoiseSubsystem::GDebugDrawNoise,
	TEXT("0=off, 1=draw a debug sphere at every ReportNoise call (red=Footstep, orange=Combat, yellow=Generic)."),
	ECVF_Cheat);

UAINoiseSubsystem::UAINoiseSubsystem()
{
}

UAINoiseSubsystem* UAINoiseSubsystem::Get(const UObject* WorldContext)
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
	return World->GetSubsystem<UAINoiseSubsystem>();
}

void UAINoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogAINoiseSubsystem, Log, TEXT("AINoiseSubsystem initialized."));
}

void UAINoiseSubsystem::Deinitialize()
{
	Listeners.Reset();
	Super::Deinitialize();
}

void UAINoiseSubsystem::RegisterListener(UNoiseListenerComponent* Listener)
{
	if (Listener == nullptr)
	{
		return;
	}
	Listeners.Add(Listener);
}

void UAINoiseSubsystem::UnregisterListener(UNoiseListenerComponent* Listener)
{
	if (Listener == nullptr)
	{
		return;
	}
	Listeners.Remove(Listener);
}

void UAINoiseSubsystem::ReportNoise(FVector Location, float Loudness, AActor* Instigator, EAINoiseCategory Category)
{
	if (Loudness <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Debug 可视化：按 Category 上色，便于现场分辨噪音来源。
	if (GDebugDrawNoise > 0)
	{
		FColor DebugColor = FColor::Yellow;
		switch (Category)
		{
		case EAINoiseCategory::Footstep: DebugColor = FColor::Red; break;
		case EAINoiseCategory::Combat:   DebugColor = FColor::Orange; break;
		case EAINoiseCategory::Generic:
		default:                          DebugColor = FColor::Yellow; break;
		}
		DrawDebugSphere(World, Location, Loudness, 24, DebugColor, false, 0.5f, 0, 1.5f);
	}

	FActionAINoiseEvent Event;
	Event.Location = Location;
	Event.Loudness = Loudness;
	Event.Category = Category;
	Event.Instigator = Instigator;

	// 遍历时顺手剔除已失效的 Listener（一般 EndPlay 会主动 Unregister，这里是兜底）。
	for (auto It = Listeners.CreateIterator(); It; ++It)
	{
		UNoiseListenerComponent* Listener = It->Get();
		if (Listener == nullptr || !IsValid(Listener))
		{
			It.RemoveCurrent();
			continue;
		}
		Listener->HandleNoiseEvent(Event);
	}
}
