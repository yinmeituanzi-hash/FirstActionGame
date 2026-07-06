#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/SkillCreature.h"
#include "RaySkillCreature.generated.h"

/**
 * RaySkillCreature：激光 / 光柱 / 连续 Trace 型技能生成物占位。
 *
 * Day8.1 仅骨架：不启用射线扫描，Tick 内容与父类一致。
 * Day8.3 会：
 *   - 在 Tick 中做 SweepMultiByChannel 的射线段扫描
 *   - 支持沿角色朝向持续射线
 *   - 与 Motion Warping / 视线锁定接口对接
 */
UCLASS()
class ACTIONGAME_API ARaySkillCreature : public ASkillCreature
{
	GENERATED_BODY()
};
