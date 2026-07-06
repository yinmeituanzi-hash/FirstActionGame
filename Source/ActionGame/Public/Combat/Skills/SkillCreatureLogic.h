#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/SkillCreatureTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SkillCreatureLogic.generated.h"

struct FSkillCreatureRow;

/**
 * SkillCreature 计算工具库。
 *
 * 为什么单独抽出一个库：
 * - Spawn Transform / TargetLocation / InitialVelocity 计算规则相对独立，
 *   不依赖 Creature 或 Subsystem 状态，纯输入 → 输出。
 * - 未来 BulletCreature（不生成 Actor）也要用到同样的公式，抽到库里能复用。
 * - 单元测试友好：不依赖 Actor World。
 */
UCLASS()
class ACTIONGAME_API USkillCreatureLogic : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 解算最终的出生 Transform。
	 * 综合考虑 Request.SpawnMode + Row.SpawnSocket + Row.BornLocationOffset + Request.SpawnOffset。
	 */
	static FTransform ResolveSpawnTransform(
		const FSkillCreatureSpawnRequest& Request,
		const FSkillCreatureRow& Row);

	/**
	 * 解算初速度向量。
	 * 综合 Request.DirectionMode + Row.InitialSpeed + Row.MoveMode（Straight/Homing 需要初始方向，
	 * Gravity 可能靠 bGravityAdapt 反推）。
	 */
	static FVector ResolveInitialVelocity(
		const FSkillCreatureSpawnRequest& Request,
		const FSkillCreatureRow& Row,
		const FTransform& SpawnTransform);

	/**
	 * 解算目标世界坐标（Homing / TowardsTarget 用）。
	 * 返回 false 表示"无有效目标"，调用者自行 fallback（例如改用 SourceForward）。
	 */
	static bool ResolveTargetLocation(
		const FSkillCreatureSpawnRequest& Request,
		FVector& OutTargetLocation);

	/** 散射多发时按序号取分裂方向。0..(Count-1) 均匀分布在 [-Spread, +Spread]。 */
	static FVector GetSpreadDirection(
		const FVector& BaseDirection,
		int32 Index,
		int32 Count,
		float SpreadHalfAngleDeg);
};
