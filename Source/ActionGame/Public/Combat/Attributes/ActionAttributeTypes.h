#pragma once

#include "CoreMinimal.h"
#include "ActionAttributeTypes.generated.h"

/**
 * 战斗属性名。
 *
 * 先只迁移当前已经被技能、受击、AI 共同依赖的基础属性。
 * 后续装备 / Buff / 元素系统可以继续扩展这个枚举，而不需要让 CharacterBase 增加散落字段。
 */
UENUM(BlueprintType)
enum class EActionAttributeType : uint8
{
	HP UMETA(DisplayName = "HP"),
	MaxHP UMETA(DisplayName = "Max HP"),
	AttackPower UMETA(DisplayName = "Attack Power"),

	Defense UMETA(DisplayName = "Defense"),
	MoveSpeed UMETA(DisplayName = "Move Speed"),
	Poise UMETA(DisplayName = "Poise"),
	Stamina UMETA(DisplayName = "Stamina"),
	SkillPower UMETA(DisplayName = "Skill Power"),

	SP UMETA(DisplayName = "SP"),
	MaxSP UMETA(DisplayName = "Max SP")
};
