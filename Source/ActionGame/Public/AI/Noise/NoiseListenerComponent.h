#pragma once

#include "CoreMinimal.h"
#include "AI/Noise/AINoiseTypes.h"
#include "Components/ActorComponent.h"
#include "NoiseListenerComponent.generated.h"

class AActionMonsterCharacter;

/**
 * 收到噪音时通知 Owner 的多播委托。
 * 蓝图 / 其他组件可以监听做自定义行为（比如怪物 BP 想播个"侧头听声音"动画）。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHearNoiseDynamic, const FActionAINoiseEvent&, Event);

/**
 * 怪物的"耳朵"组件。
 *
 * BeginPlay 时自动注册到 `UAINoiseSubsystem`，EndPlay 反注册。
 *
 * 收到 ReportNoise 时会按下面顺序过滤：
 *  1. 距离 > min(Loudness, HearingDistance) → 丢弃
 *  2. Instigator 是 Owner 自己 → 丢弃（不会被自己挥刀声拉走）
 *  3. 处于 Hearing CD 内 → 丢弃
 *  4. Owner 是 AActionMonsterCharacter 且 AlertState == Combat → 丢弃（已经在追玩家了，不查可疑点）
 *  5. Owner 死亡 / 被 Block_AIControl → 丢弃
 *
 * 全部过关后调 `HandleHearNoise_Internal` 做默认行为：
 *  - 把 Owner（如果是 ActionMonsterCharacter）的 LastNoiseLocation 写为声音位置
 *  - `SetAlertState(Alert)` 让 BTService_AlertStateTick 下一帧自然切到 Alert 分支
 *  - 启动 Hearing CD
 *
 * 同时 Broadcast `OnHearNoise` 供蓝图 / 其他组件做额外反应。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UNoiseListenerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNoiseListenerComponent();

	// ---------- UActorComponent ----------

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- 配置 ----------

	/**
	 * 听觉半径（cm）。最终生效距离 = min(Loudness, HearingDistance)。
	 *  - 小怪杂兵：1500~2000
	 *  - 警觉度高的精英怪：2500+
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Hearing", meta = (ClampMin = "0.0"))
	float HearingDistance = 1500.0f;

	/**
	 * 听到一次后多久内不再响应。避免玩家持续奔跑产生十几次 Footstep 让怪反复"拉一下"。
	 * 默认 1.5s，配合玩家奔跑 0.5s 一次 Report，等于"听到一声就足够拉警戒，后两秒不再被打扰"。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Hearing", meta = (ClampMin = "0.0"))
	float HearingCooldown = 1.5f;

	/** 是否在 Combat 状态下也响应（默认 false，避免被自己挥刀声拉走目标）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Hearing")
	bool bRespondWhenCombat = false;

	/** Owner 是怪物时，是否自动把 Owner.AlertState 推到 Alert。默认 true。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Hearing")
	bool bAutoPromoteAlertState = true;

	// ---------- 事件 ----------

	/** 听到噪音时广播（在所有过滤都过关后才会触发）。蓝图可挂额外反应。 */
	UPROPERTY(BlueprintAssignable, Category = "Action|AI|Hearing")
	FOnHearNoiseDynamic OnHearNoise;

	// ---------- Subsystem 调用 ----------

	/** 由 UAINoiseSubsystem 调。会做距离 / CD / 状态过滤后再决定是否触发默认行为。 */
	void HandleNoiseEvent(const FActionAINoiseEvent& Event);

	UFUNCTION(BlueprintPure, Category = "Action|AI|Hearing")
	float GetHearingCooldownRemaining() const;

private:
	/** 上一次听到声音的世界时间。-1 表示从未听到过。 */
	float LastHearTime = -1000.0f;

	/** 距离 / 自身 / CD / Combat 状态过滤的通用部分。 */
	bool ShouldRespondTo(const FActionAINoiseEvent& Event) const;

	/** 默认行为：写 Owner.LastNoiseLocation + SetAlertState(Alert)。 */
	void ApplyDefaultResponse(const FActionAINoiseEvent& Event, AActionMonsterCharacter* Monster);
};
