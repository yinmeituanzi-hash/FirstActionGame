#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/SkillCreature.h"
#include "OrbitSkillCreature.generated.h"

/**
 * OrbitSkillCreature：环绕角色 / 挂在武器上的持续伤害体占位。
 *
 * Day8.1 仅骨架：MoveMode = Static + bAttachOwner = true 可以退化实现"贴身场"，
 * 但真正的环绕角速度、位置偏移曲线要到 Day8.5 才补齐。
 * 单独抽子类的好处是：环绕行为常常涉及 SocketRelative / 环绕节拍 / 分离销毁语义，
 * 未来加逻辑不必污染 SkillCreature 基类。
 */
UCLASS()
class ACTIONGAME_API AOrbitSkillCreature : public ASkillCreature
{
	GENERATED_BODY()
};
