#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ActionCharacterMovementComponent.generated.h"

/**
 * UActionCharacterMovementComponent
 *
 * 在 UCharacterMovementComponent 之上加了两组扩展：
 *
 * 1. RootMotion Z 抽取（保留原有功能）
 *    - 某些蒙太奇带垂直位移但我们不希望它影响重力/跳跃，可以选择性抽掉 Z 分量。
 *
 * 2. AttackComboTurn —— 连段攻击的转向（本次新增，用于修复"连段转向时屏幕抖动"）
 *    - 旧实现：UActionCombatComponent 用 60Hz Timer 调 SetActorRotation。
 *      问题在于：Timer 的旋转写入和 RootMotion 的旋转写入不在同一帧管线里，会互相覆盖，
 *      画面上看起来就是抖动 / 闪烁。
 *    - 新实现：把"目标 Yaw"传到 CharacterMovement，在 PerformMovement 的 PhysicsRotation 阶段
 *      与 RootMotion 同帧、同管线地推进角色 Yaw。从根上消除两个旋转源打架。
 *    - 触发：连段攻击切换的瞬间调用 RequestAttackComboTurn(NewYaw, MaxYawSpeed)。
 *    - 完成：当角色 Yaw 与目标 Yaw 之差小于阈值时自动停止；外部也可以调 ClearAttackComboTurn。
 */
UCLASS()
class ACTIONGAME_API UActionCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// ---------- RootMotion Z 抽取（保留原有功能）----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|RootMotion")
	bool bEnableRootMotionZExtraction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|RootMotion", meta = (ClampMin = "0.0"))
	float RootMotionZScale = 1.0f;

	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;

	// ---------- AttackComboTurn ----------

	/** 是否处于"连段转向"窗口。 */
	UFUNCTION(BlueprintPure, Category = "Action|AttackTurn")
	bool IsAttackComboTurnActive() const { return bAttackComboTurnActive; }

	/**
	 * 请求一次连段转向。
	 *
	 * @param TargetWorldYaw  目标世界 Yaw（角度）。
	 * @param MaxYawSpeedDeg  Yaw 最大转速（度/秒）。建议 600~1200，越大越"硬"。
	 *                        传 <= 0 表示瞬间到位。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|AttackTurn")
	void RequestAttackComboTurn(float TargetWorldYaw, float MaxYawSpeedDeg);

	/** 主动取消连段转向。 */
	UFUNCTION(BlueprintCallable, Category = "Action|AttackTurn")
	void ClearAttackComboTurn();

protected:
	/**
	 * UE 在每个 Move tick 走完 RootMotion 应用后，会调一次 PhysicsRotation
	 * （前提是 bUseControllerDesiredRotation / bOrientRotationToMovement / 我们手动 force 走）
	 * 我们 override 它，在内部把 AttackComboTurn 的目标 Yaw 应用到 Pawn 上。
	 */
	virtual void PhysicsRotation(float DeltaTime) override;

private:
	/** 把当前 Pawn Yaw 朝 TargetYaw 推进 DeltaYaw，返回新 Yaw。 */
	float StepYawTowards(float CurrentYaw, float TargetYaw, float DeltaTime) const;

	bool bAttackComboTurnActive = false;
	float AttackComboTurnTargetYaw = 0.0f;
	float AttackComboTurnMaxYawSpeedDeg = 0.0f;
};
