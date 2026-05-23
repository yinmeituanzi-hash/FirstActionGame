#pragma once

#include "CoreMinimal.h"
#include "AI/Noise/AINoiseTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "AINoiseSubsystem.generated.h"

class UNoiseListenerComponent;

/**
 * 全局噪音转发中心。
 *
 * 设计原则：
 *  - **职责单一**：只做"收到 ReportNoise → 遍历 Listener → 通知半径内的"。
 *  - **不感知业务**：不知道谁是怪物、不直接改 AlertState、不知道 BehaviorTree。
 *  - **解耦发声方与接收方**：玩家不需要遍历怪物；怪物也不需要 Tick 查玩家速度。
 *
 * 这套"主动 Report + 被动 Listen"的设计能让 Subsystem 本身只有 ~80 行 C++ 就够。
 * 对比 010 的 MonAlertComponent 自己拉 GameMode 查 HearData 的写法，
 * 这里把"听到声音后该不该做事 / 该做什么事"完全留给 Listener，扩展性更好。
 *
 * 用法：
 *   - 发声方（玩家 / 环境 / 其他怪物）：
 *       UAINoiseSubsystem::Get(this)->ReportNoise(Loc, Loudness, Instigator, Category);
 *   - 接收方：在 Actor 上挂 UNoiseListenerComponent，BeginPlay 会自动注册。
 *
 * Debug：
 *   - 控制台命令 `AI.NoiseDebug 1` 开启可视化（DrawDebugSphere 显示每次 Report 的半径）。
 *   - 红色=Footstep，橙色=Combat，黄色=Generic。
 */
UCLASS()
class ACTIONGAME_API UAINoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UAINoiseSubsystem();

	/** 便捷获取。WorldContext 是任意能 GetWorld() 的对象。 */
	static UAINoiseSubsystem* Get(const UObject* WorldContext);

	// ---------- USubsystem 接口 ----------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------- Listener 注册 ----------

	/** Listener BeginPlay 时调。重复注册会被静默忽略。 */
	void RegisterListener(UNoiseListenerComponent* Listener);

	/** Listener EndPlay 时调。未注册过的也安全。 */
	void UnregisterListener(UNoiseListenerComponent* Listener);

	// ---------- Report 接口 ----------

	/**
	 * 发声主入口。Subsystem 会构造 FActionAINoiseEvent 然后转发给所有 Listener。
	 * Listener 自己负责距离过滤、冷却、按 Owner 状态决定是否响应。
	 *
	 * @param Location   发声世界位置（一般是发声 Actor 的 ActorLocation）。
	 * @param Loudness   噪音半径（cm）。超出此半径的 Listener 不会收到。
	 * @param Instigator 发声方（可为空）。
	 * @param Category   噪音种类，默认 Generic。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|AI|Noise", meta = (AdvancedDisplay = "Category"))
	void ReportNoise(FVector Location, float Loudness, AActor* Instigator = nullptr, EAINoiseCategory Category = EAINoiseCategory::Generic);

	// ---------- Debug ----------

	/** 由控制台命令 `AI.NoiseDebug` 控制。1=画 Report 的可视化球，0=关闭。 */
	static int32 GDebugDrawNoise;

private:
	/**
	 * 已注册的 Listener 集合。
	 * - 用 TObjectPtr 让 GC 持有强引用（Subsystem 比 Actor 长寿，反过来若 Component 已被销毁 Listener 也会先调 EndPlay 取消注册）。
	 * - Listener.EndPlay 必须 UnregisterListener，否则 Subsystem 会一直持有已销毁组件的引用。
	 */
	UPROPERTY(Transient)
	TSet<TObjectPtr<UNoiseListenerComponent>> Listeners;
};
