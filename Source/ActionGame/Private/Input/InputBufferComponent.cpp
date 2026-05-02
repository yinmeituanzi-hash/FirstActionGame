#include "Input/InputBufferComponent.h"

#include "Engine/World.h"

UInputBufferComponent::UInputBufferComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInputBufferComponent::PushInput(FName InputName, float Lifetime)
{
	if (InputName.IsNone())
	{
		return;
	}

	ClearExpiredInputs();

	if (UWorld* World = GetWorld())
	{
		const float ExpireTime = World->GetTimeSeconds() + FMath::Max(Lifetime, 0.0f);
		BufferedInputExpireTimes.Add(InputName, ExpireTime);
	}
}

bool UInputBufferComponent::ConsumeInput(FName InputName)
{
	if (InputName.IsNone())
	{
		return false;
	}

	ClearExpiredInputs();

	const float* ExpireTime = BufferedInputExpireTimes.Find(InputName);
	if (ExpireTime == nullptr)
	{
		return false;
	}

	BufferedInputExpireTimes.Remove(InputName);
	return true;
}

bool UInputBufferComponent::HasValidInput(FName InputName) const
{
	if (InputName.IsNone())
	{
		return false;
	}

	if (const UWorld* World = GetWorld())
	{
		if (const float* ExpireTime = BufferedInputExpireTimes.Find(InputName))
		{
			return *ExpireTime >= World->GetTimeSeconds();
		}
	}

	return false;
}

void UInputBufferComponent::ClearExpiredInputs()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	for (TMap<FName, float>::TIterator It(BufferedInputExpireTimes); It; ++It)
	{
		if (It.Value() < CurrentTime)
		{
			It.RemoveCurrent();
		}
	}
}
