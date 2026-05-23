#include "AI/Decorators/BTDecorator_MonsterCooldown.h"

UBTDecorator_MonsterCooldown::UBTDecorator_MonsterCooldown()
{
	NodeName = TEXT("Monster Cooldown");
}

FString UBTDecorator_MonsterCooldown::GetStaticDescription() const
{
	return FString::Printf(TEXT("Monster Cooldown: %.1fs"), CoolDownTime);
}
