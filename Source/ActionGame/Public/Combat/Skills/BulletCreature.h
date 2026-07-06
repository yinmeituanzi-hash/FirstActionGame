#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/SkillCreature.h"
#include "BulletCreature.generated.h"

/**
 * BulletCreature：直线弹 / 追踪弹 / 抛物线弹的具体子类占位。
 *
 * Day8.1 仅骨架：所有行为直接复用父类 ASkillCreature（Straight/Homing/Gravity + Sphere 碰撞）。
 * Day8.4 会：
 *   - 支持 "不生成 Actor" 的轻量子弹（由 BulletManager 统一 Tick）
 *   - 精细化穿透 / 反弹表现
 *   - 追踪弹的目标丢失回退
 */
UCLASS()
class ACTIONGAME_API ABulletCreature : public ASkillCreature
{
	GENERATED_BODY()
};
