#include "Combat/Attributes/SPRecoverComponent.h"

#include "Char/ActionCharacterBase.h"
#include "Combat/Attributes/AttributeComponent.h"
#include "TimerManager.h"

USPRecoverComponent::USPRecoverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPRecoverComponent::BeginPlay()
{
	Super::BeginPlay();

	if (SPRegenPerSecond > 0.0f && GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().SetTimer(
			SPRegenTimerHandle,
			this,
			&USPRecoverComponent::TickSPRegen,
			SPRegenInterval,
			true);
	}
}

void USPRecoverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().ClearTimer(SPRegenTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void USPRecoverComponent::TickSPRegen()
{
	if (SPRegenPerSecond <= 0.0f)
	{
		return;
	}

	const AActionCharacterBase* OwnerChar = Cast<AActionCharacterBase>(GetOwner());
	if (OwnerChar != nullptr && OwnerChar->IsDead())
	{
		return;
	}

	UAttributeComponent* AttrComp = OwnerChar != nullptr
		? OwnerChar->FindComponentByClass<UAttributeComponent>()
		: nullptr;
	if (AttrComp == nullptr)
	{
		return;
	}

	const float MaxSP = AttrComp->GetAttribute(EAttributeType::MaxSP);
	if (MaxSP <= 0.0f)
	{
		return;
	}

	const float CurrentSP = AttrComp->GetAttribute(EAttributeType::SP);
	if (CurrentSP >= MaxSP)
	{
		return;
	}

	AttrComp->ModifyAttribute(EAttributeType::SP, SPRegenPerSecond * SPRegenInterval);
}
