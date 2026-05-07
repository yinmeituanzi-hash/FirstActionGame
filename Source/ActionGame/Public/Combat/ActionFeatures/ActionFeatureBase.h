#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Char/ActionCharacterBase.h"
#include "ActionFeatureBase.generated.h"

class AActionPlayerCharacter;
class UAnimInstance;
struct FBranchingPointNotifyPayload;

/**
 * UActionFeatureBase
 *
 * 一个独立"动作"的抽象基类，例如：闪避、跳跃、瞄准等。
 *
 * 设计目的：
 *   - 把分散在 PlayerCharacter 里的动作流程（启动、播放、判断、清理）下沉到独立类。
 *   - PlayerCharacter 只负责持有 Feature 列表 + 转发输入和 Notify。
 *   - 每个 Feature 自管自己的状态、配置、冷却、Notify 处理。
 *
 * 与 010 的对比：
 *   - 去掉了 Lua/网络/EMObject 体系，直接继承 UObject，更轻量。
 *   - 通过 GameplayTag 协调 Block/Window/State，对接你们已有的 ActionGameplayTags。
 *   - 不强制要求蒙太奇（蒙太奇逻辑在 UMontageActionFeature 里）。
 *
 * 生命周期：
 *   Initialize()  →  CanExecute() → Execute() → (运行中：OnNotify / Tick) → Stop()
 *   PlayerCharacter 在 BeginPlay 里调 Initialize；在销毁时不需要特殊清理（UObject GC 处理）。
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, ClassGroup = (Action))
class ACTIONGAME_API UActionFeatureBase : public UObject
{
	GENERATED_BODY()

public:
	UActionFeatureBase();

	/** Feature 的内部识别名，主要用于日志和调试。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActionFeature|Identity")
	FName FeatureName = NAME_None;

	/**
	 * 该 Feature 想进入的角色主状态。
	 * Execute 成功时会自动将角色切到这个状态。
	 * Stop 时如果当前还是该状态，会自动切回 Idle。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActionFeature|State")
	EActionCharacterState TargetState = EActionCharacterState::Idle;

	/**
	 * 任何一个 BlockTag 出现在角色身上时，禁止启动该 Feature。
	 * 例如：DodgeFeature 的 BlockTags 通常包含 Block.Dodge。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActionFeature|Limit")
	FGameplayTagContainer BlockTags;

	/** 启动后的全局冷却时间，单位秒，用于防止瞬时重复触发。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActionFeature|Limit", meta = (ClampMin = "0.0"))
	float CooldownTime = 0.0f;

	/** 是否需要每帧 Tick。子类如有需要请在构造函数里设为 true。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ActionFeature|Tick")
	bool bEnableTick = false;

	/** 由 PlayerCharacter 在 BeginPlay 里调用，建立反向引用。 */
	virtual void Initialize(AActionPlayerCharacter* InOwner);

	/** 通用前置检查：CD、BlockTag、死亡。子类可以 override 加入自定义条件。 */
	UFUNCTION(BlueprintCallable, Category = "ActionFeature")
	virtual bool CanExecute() const;

	/** 真正启动 Feature。子类必须实现具体逻辑，并在内部调用 BeginActive。 */
	UFUNCTION(BlueprintCallable, Category = "ActionFeature")
	virtual void Execute();

	/**
	 * 停止 Feature。
	 * @param bInterrupted true 表示被外部打断（例如被另一个 Feature 抢占）。
	 */
	UFUNCTION(BlueprintCallable, Category = "ActionFeature")
	virtual void Stop(bool bInterrupted = false);

	/** 每帧调用，仅当 bEnableTick = true 时由 PlayerCharacter 转发。 */
	virtual void Tick(float DeltaTime) {}

	/** 收到来自当前蒙太奇的 Notify。子类按 NotifyName 分发处理。 */
	virtual void OnNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload) {}

	/** 收到所属蒙太奇结束的回调。仅 MontageActionFeature 用得到，基类提供空实现。 */
	virtual void OnMontageEnded(class UAnimMontage* Montage, bool bInterrupted) {}

	UFUNCTION(BlueprintPure, Category = "ActionFeature")
	bool IsActive() const { return bIsActive; }

	UFUNCTION(BlueprintPure, Category = "ActionFeature")
	bool IsInCooldown() const;

	UFUNCTION(BlueprintPure, Category = "ActionFeature")
	AActionPlayerCharacter* GetOwnerPlayer() const { return OwnerChar.Get(); }

protected:
	/** 由子类在 Execute 成功时调用：标记激活、记录时间、切状态、与之前活跃 Feature 互斥。 */
	void BeginActive();

	/** 由子类在 Stop 时调用：清理激活标记、若仍处于 TargetState 则回到 Idle。 */
	void EndActive(bool bInterrupted);

	/** 工具方法：从 OwnerChar 拿 AnimInstance。 */
	UAnimInstance* GetOwnerAnimInstance() const;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActionPlayerCharacter> OwnerChar;

	UPROPERTY(Transient)
	float LastExecuteTime = -1.0f;

	UPROPERTY(Transient)
	bool bIsActive = false;
};
