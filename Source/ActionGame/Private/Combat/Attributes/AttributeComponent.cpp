#include "Combat/Attributes/AttributeComponent.h"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	DefaultAttributeValues.Add(EAttributeType::MaxHP, 100.0f);
	DefaultAttributeValues.Add(EAttributeType::HP, 100.0f);
	DefaultAttributeValues.Add(EAttributeType::AttackPower, 10.0f);
}

void UAttributeComponent::InitializeAttributesFromDefaults()
{
	EnsureDefaultAttributes();

	AttributeValues.Reset();

	const float DefaultMaxHP = DefaultAttributeValues.FindRef(EAttributeType::MaxHP);
	const float DefaultHP = DefaultAttributeValues.Contains(EAttributeType::HP)
		? DefaultAttributeValues.FindRef(EAttributeType::HP)
		: DefaultMaxHP;

	SetAttributeInternal(EAttributeType::MaxHP, DefaultMaxHP, false);
	SetAttributeInternal(EAttributeType::HP, DefaultHP, false);

	for (const TPair<EAttributeType, float>& Pair : DefaultAttributeValues)
	{
		if (Pair.Key == EAttributeType::MaxHP || Pair.Key == EAttributeType::HP)
		{
			continue;
		}

		SetAttributeInternal(Pair.Key, Pair.Value, false);
	}
}

float UAttributeComponent::GetAttribute(EAttributeType Attribute) const
{
	if (const float* FoundValue = AttributeValues.Find(Attribute))
	{
		return *FoundValue;
	}

	if (const float* DefaultValue = DefaultAttributeValues.Find(Attribute))
	{
		return *DefaultValue;
	}

	return 0.0f;
}

void UAttributeComponent::SetAttribute(EAttributeType Attribute, float NewValue)
{
	SetAttributeInternal(Attribute, NewValue, true);
}

float UAttributeComponent::ModifyAttribute(EAttributeType Attribute, float Delta)
{
	const float NewValue = GetAttribute(Attribute) + Delta;
	SetAttribute(Attribute, NewValue);
	return GetAttribute(Attribute);
}

bool UAttributeComponent::HasAttribute(EAttributeType Attribute) const
{
	return AttributeValues.Contains(Attribute) || DefaultAttributeValues.Contains(Attribute);
}

void UAttributeComponent::ClampCoreAttributes()
{
	SetAttributeInternal(EAttributeType::MaxHP, GetAttribute(EAttributeType::MaxHP), true);
	SetAttributeInternal(EAttributeType::HP, GetAttribute(EAttributeType::HP), true);
	SetAttributeInternal(EAttributeType::AttackPower, GetAttribute(EAttributeType::AttackPower), true);
}

void UAttributeComponent::EnsureDefaultAttributes()
{
	DefaultAttributeValues.FindOrAdd(EAttributeType::MaxHP) = FMath::Max(
		DefaultAttributeValues.FindRef(EAttributeType::MaxHP),
		1.0f);

	const float MaxHP = DefaultAttributeValues.FindRef(EAttributeType::MaxHP);
	const bool bHasDefaultHP = DefaultAttributeValues.Contains(EAttributeType::HP);
	const float HPValue = bHasDefaultHP ? DefaultAttributeValues.FindRef(EAttributeType::HP) : MaxHP;
	DefaultAttributeValues.FindOrAdd(EAttributeType::HP) = FMath::Clamp(
		HPValue,
		0.0f,
		MaxHP);

	DefaultAttributeValues.FindOrAdd(EAttributeType::AttackPower) = FMath::Max(
		DefaultAttributeValues.FindRef(EAttributeType::AttackPower),
		0.0f);
}

float UAttributeComponent::GetClampedValue(EAttributeType Attribute, float Value) const
{
	switch (Attribute)
	{
	case EAttributeType::MaxHP:
		return FMath::Max(Value, 1.0f);
	case EAttributeType::HP:
		return FMath::Clamp(Value, 0.0f, FMath::Max(GetAttribute(EAttributeType::MaxHP), 1.0f));
	case EAttributeType::MaxSP:
		return FMath::Max(Value, 0.0f);
	case EAttributeType::SP:
		return FMath::Clamp(Value, 0.0f, FMath::Max(GetAttribute(EAttributeType::MaxSP), 0.0f));
	case EAttributeType::AttackPower:
	case EAttributeType::Defense:
	case EAttributeType::MoveSpeed:
	case EAttributeType::Poise:
	case EAttributeType::Stamina:
	case EAttributeType::SkillPower:
	default:
		return FMath::Max(Value, 0.0f);
	}
}

void UAttributeComponent::SetAttributeInternal(EAttributeType Attribute, float NewValue, bool bBroadcast)
{
	const float ClampedValue = GetClampedValue(Attribute, NewValue);
	const float OldValue = GetAttribute(Attribute);
	if (FMath::IsNearlyEqual(OldValue, ClampedValue))
	{
		AttributeValues.FindOrAdd(Attribute) = ClampedValue;
		return;
	}

	AttributeValues.FindOrAdd(Attribute) = ClampedValue;

	if (Attribute == EAttributeType::MaxHP)
	{
		SetAttributeInternal(EAttributeType::HP, GetAttribute(EAttributeType::HP), bBroadcast);
	}
	else if (Attribute == EAttributeType::MaxSP)
	{
		SetAttributeInternal(EAttributeType::SP, GetAttribute(EAttributeType::SP), bBroadcast);
	}

	if (bBroadcast)
	{
		OnAttributeChanged.Broadcast(Attribute, OldValue, ClampedValue);
	}
}
