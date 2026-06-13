#include "Combat/Attributes/ActionAttributeComponent.h"

UActionAttributeComponent::UActionAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	DefaultAttributeValues.Add(EActionAttributeType::MaxHP, 100.0f);
	DefaultAttributeValues.Add(EActionAttributeType::HP, 100.0f);
	DefaultAttributeValues.Add(EActionAttributeType::AttackPower, 10.0f);
}

void UActionAttributeComponent::InitializeAttributesFromDefaults()
{
	EnsureDefaultAttributes();

	AttributeValues.Reset();

	const float DefaultMaxHP = DefaultAttributeValues.FindRef(EActionAttributeType::MaxHP);
	const float DefaultHP = DefaultAttributeValues.Contains(EActionAttributeType::HP)
		? DefaultAttributeValues.FindRef(EActionAttributeType::HP)
		: DefaultMaxHP;

	SetAttributeInternal(EActionAttributeType::MaxHP, DefaultMaxHP, false);
	SetAttributeInternal(EActionAttributeType::HP, DefaultHP, false);

	for (const TPair<EActionAttributeType, float>& Pair : DefaultAttributeValues)
	{
		if (Pair.Key == EActionAttributeType::MaxHP || Pair.Key == EActionAttributeType::HP)
		{
			continue;
		}

		SetAttributeInternal(Pair.Key, Pair.Value, false);
	}
}

float UActionAttributeComponent::GetAttribute(EActionAttributeType Attribute) const
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

void UActionAttributeComponent::SetAttribute(EActionAttributeType Attribute, float NewValue)
{
	SetAttributeInternal(Attribute, NewValue, true);
}

float UActionAttributeComponent::ModifyAttribute(EActionAttributeType Attribute, float Delta)
{
	const float NewValue = GetAttribute(Attribute) + Delta;
	SetAttribute(Attribute, NewValue);
	return GetAttribute(Attribute);
}

bool UActionAttributeComponent::HasAttribute(EActionAttributeType Attribute) const
{
	return AttributeValues.Contains(Attribute) || DefaultAttributeValues.Contains(Attribute);
}

void UActionAttributeComponent::ClampCoreAttributes()
{
	SetAttributeInternal(EActionAttributeType::MaxHP, GetAttribute(EActionAttributeType::MaxHP), true);
	SetAttributeInternal(EActionAttributeType::HP, GetAttribute(EActionAttributeType::HP), true);
	SetAttributeInternal(EActionAttributeType::AttackPower, GetAttribute(EActionAttributeType::AttackPower), true);
}

void UActionAttributeComponent::EnsureDefaultAttributes()
{
	DefaultAttributeValues.FindOrAdd(EActionAttributeType::MaxHP) = FMath::Max(
		DefaultAttributeValues.FindRef(EActionAttributeType::MaxHP),
		1.0f);

	const float MaxHP = DefaultAttributeValues.FindRef(EActionAttributeType::MaxHP);
	const bool bHasDefaultHP = DefaultAttributeValues.Contains(EActionAttributeType::HP);
	const float HPValue = bHasDefaultHP ? DefaultAttributeValues.FindRef(EActionAttributeType::HP) : MaxHP;
	DefaultAttributeValues.FindOrAdd(EActionAttributeType::HP) = FMath::Clamp(
		HPValue,
		0.0f,
		MaxHP);

	DefaultAttributeValues.FindOrAdd(EActionAttributeType::AttackPower) = FMath::Max(
		DefaultAttributeValues.FindRef(EActionAttributeType::AttackPower),
		0.0f);
}

float UActionAttributeComponent::GetClampedValue(EActionAttributeType Attribute, float Value) const
{
	switch (Attribute)
	{
	case EActionAttributeType::MaxHP:
		return FMath::Max(Value, 1.0f);
	case EActionAttributeType::HP:
		return FMath::Clamp(Value, 0.0f, FMath::Max(GetAttribute(EActionAttributeType::MaxHP), 1.0f));
	case EActionAttributeType::AttackPower:
	case EActionAttributeType::Defense:
	case EActionAttributeType::MoveSpeed:
	case EActionAttributeType::Poise:
	case EActionAttributeType::Stamina:
	case EActionAttributeType::SkillPower:
	default:
		return FMath::Max(Value, 0.0f);
	}
}

void UActionAttributeComponent::SetAttributeInternal(EActionAttributeType Attribute, float NewValue, bool bBroadcast)
{
	const float ClampedValue = GetClampedValue(Attribute, NewValue);
	const float OldValue = GetAttribute(Attribute);
	if (FMath::IsNearlyEqual(OldValue, ClampedValue))
	{
		AttributeValues.FindOrAdd(Attribute) = ClampedValue;
		return;
	}

	AttributeValues.FindOrAdd(Attribute) = ClampedValue;

	// MaxHP 变化后立即夹住 HP，避免 UI / 死亡判断读到非法血量。
	if (Attribute == EActionAttributeType::MaxHP)
	{
		SetAttributeInternal(EActionAttributeType::HP, GetAttribute(EActionAttributeType::HP), bBroadcast);
	}

	if (bBroadcast)
	{
		OnAttributeChanged.Broadcast(Attribute, OldValue, ClampedValue);
	}
}
