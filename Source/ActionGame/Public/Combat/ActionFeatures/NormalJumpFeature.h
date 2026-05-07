#pragma once
#include "CoreMinimal.h"
#include "Combat/ActionFeatures/ActionFeatureBase.h"
#include "NormalJumpFeature.generated.h"

UENUM(BlueprintType)
enum class ENormalJumpState : uint8
{
	Grounded UMETA(DisplayName = "Grounded"),
	FirstJump UMETA(DisplayName = "First Jump"),
	SecondJump UMETA(DisplayName = "Second Jump"),
	Falling UMETA(DisplayName = "Falling")
};

/**
 * UNormalJumpFeature
 *
 * 普通跳跃 + 二段跳。
 *
 * 设计与 010 NormalJumpFeature 的对应：
 *   - 010 的 EJumpState 简化为 ENormalJumpState（去掉 BulletJump 相关项）。
 *   - 010 的 BulletJump 全部去掉（涉及 capsule 缩放、pitch 旋转、阻挡检测、网络同步）。
 *   - 保留 010 最有价值的部分：
 *       - 二段跳计数（MaxJumpCount = 2）
 *       - 跳跃打断攻击/闪避（Execute 时检查并 Stop 当前 Active Feature）
 *       - 二段跳速度独立配置（FirstJumpZVelocity / SecondJumpZVelocity）
 *
 * 与 ABP 的分工：
 *   - ABP 仍负责"地面/空中/落地"动画的状态切换，不动。
 *   - 本 Feature 负责"能不能跳、跳几段、给多大初速度、是否打断当前动作"。
 *   - 不需要播放跳跃蒙太奇——所以本 Feature 直接继承 UActionFeatureBase 而不是 UMontageActionFeature。
 *
 * 使用方式：
 *   - PlayerCharacter::OnJumpInput 调 NormalJumpFeature->Execute()。
 *   - PlayerCharacter::Landed 时调 NormalJumpFeature->NotifyLanded()。
 */
UCLASS(Blueprintable, ClassGroup = (Action))
class ACTIONGAME_API UNormalJumpFeature : public UActionFeatureBase
{
	GENERATED_BODY()

public:
	UNormalJumpFeature();

	// ---------- 跳跃配置 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump|Config", meta = (ClampMin = "1", ClampMax = "5"))
	int32 MaxJumpCount = 2;

	/** 第一段跳的 Z 初速度（与 UE 默认 ACharacter 跳跃保持一致）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump|Config", meta = (ClampMin = "0.0"))
	float FirstJumpZVelocity = 600.0f;

	/** 二段跳的 Z 初速度。一般略低于一段跳。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump|Config", meta = (ClampMin = "0.0"))
	float SecondJumpZVelocity = 520.0f;

	/**
	 * 起跳时是否覆盖当前 XY 速度的方向。
	 * true：用当前 ControlRotation 前向作为 XY 跳跃方向（适合需要"用跳跃改变方向"的玩法）。
	 * false：保留 LaunchCharacter 之前的 XY 速度（更自然）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Jump|Config")
	bool bOverrideXYVelocityOnJump = false;

	// ---------- 状态 ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump|State")
	ENormalJumpState CurrentJumpState = ENormalJumpState::Grounded;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jump|State")
	int32 JumpCount = 0;

	UFUNCTION(BlueprintPure, Category = "Jump|State")
	int32 GetJumpCount() const { return JumpCount; }

	UFUNCTION(BlueprintPure, Category = "Jump|State")
	int32 GetRemainingJumps() const { return FMath::Max(0, MaxJumpCount - JumpCount); }

	// ---------- Lifecycle ----------

	virtual bool CanExecute() const override;
	virtual void Execute() override;

	/** 由 PlayerCharacter::Landed 调用，重置二段跳计数。 */
	UFUNCTION(BlueprintCallable, Category = "Jump")
	void NotifyLanded();

	/** 由 PlayerCharacter Tick 调用：检测从地面变到空中的瞬间，方便从掉落（非主动跳）也算一段。 */
	UFUNCTION(BlueprintCallable, Category = "Jump")
	void NotifyFallingFromLedge();

private:
	void DoSingleJump(ENormalJumpState NewState);
};
