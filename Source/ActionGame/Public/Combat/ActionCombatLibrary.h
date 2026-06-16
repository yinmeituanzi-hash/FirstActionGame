#pragma once

#include "CoreMinimal.h"
#include "Combat/HitReact/HitReactTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionCombatLibrary.generated.h"

class AActionCharacterBase;
class UClass;

/**
 * 攻击命中通用库。
 *
 * 抽离原因：
 *  - 玩家走 SkillEffect + Notify 触发球形判定
 *  - 怪物走 Timer + Dist2D 直接取玩家（后续也会迁移到 SkillEffect）
 *  - 未来要扩"扇形/胶囊判定/武器 socket 起点"，必须只改一处
 *
 * 这里只负责"做球形判定 + 调 Victim->ReceiveHit"。
 *  - 不管 Notify 路由（玩家 / 怪物各自绑 OnPlayMontageNotifyBegin 后调本库）
 *  - 不管 HitContext 的具体字段填充策略（Attacker 端按需填好后传进来当模板）
 *  - 不管多目标去重（去重 Set 由调用方持有，跨 Notify 共享）
 *
 * 这种"只做判定，不管路由 / 不管去重"的设计让玩家的"一段连击多帧 Notify 共用一个 Set"
 * 和怪物的"单段攻击单 Notify 用临时 Set"都能复用同一个函数。
 */
struct ACTIONGAME_API FSphereAttackHitParams
{
	/** 攻击者。会被加入 IgnoredActors，HitContext.Attacker 也用此值。 */
	TWeakObjectPtr<AActor> Attacker;

	/** 球心世界位置。一般是 Attacker.Location + ForwardVector * ForwardOffset。 */
	FVector Center = FVector::ZeroVector;

	/** 球半径（cm）。 */
	float Radius = 100.0f;

	/** 命中目标的过滤类型。一般传 AActionCharacterBase::StaticClass()。null 表示不过滤。 */
	TSubclassOf<AActor> TargetClassFilter = nullptr;

	/**
	 * HitContext 模板。Library 会复制此模板用于每个被命中的目标，并按需填充：
	 *   - Attacker：覆盖为本 Params.Attacker
	 *   - HitLocation：覆盖为"目标中心 + 反向偏移 BackstepDistance"，看起来像刀刃接触点
	 *   - HitDirection：覆盖为 Attacker → Victim 的水平方向
	 * 模板里的 ReactType / DamageAmount / FeedbackScale / 击飞参数 / 等等都会被保留。
	 */
	FHitContext HitContextTemplate;

	/** HitLocation 沿"Attacker→Victim"方向回偏的距离（cm）。让特效不出在胶囊体正中心。 */
	float HitLocationBackstep = 40.0f;

	/** 是否在 PIE 里画 Debug 球。 */
	bool bDrawDebugSphere = false;

	/** Debug 球持续时间（秒）。 */
	float DebugDrawDuration = 1.0f;
};

UCLASS()
class ACTIONGAME_API UActionCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 球形范围攻击命中判定。
	 *
	 * 流程：
	 *   1. SphereOverlapActors（按 Params.TargetClassFilter 过滤）
	 *   2. 排除 Attacker 自己 + 已经在 InOutHitActors 集合中的目标
	 *   3. 命中：把目标加入 InOutHitActors（防止后续 Notify 重复扣血）
	 *      构造 HitContext（基于模板 + 自动填三件事）+ 调 Victim->ReceiveHit
	 *
	 * @param WorldContext  调用上下文（Attacker 或 Owner，Library 取 World 用）
	 * @param Params        球判定参数（见 FSphereAttackHitParams 注释）
	 * @param InOutHitActors 多目标去重集合。同一段攻击多次 Notify 调用此函数时传入同一个 Set。
	 * @return 本次命中的目标数量。
	 */
	static int32 PerformSphereAttackHit(
		UObject* WorldContext,
		const FSphereAttackHitParams& Params,
		TSet<TWeakObjectPtr<AActor>>& InOutHitActors);
};
