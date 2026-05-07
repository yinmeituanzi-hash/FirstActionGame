#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "ActionCombatComponent.generated.h"

class ACharacter;
class UAnimInstance;
class UAnimMontage;

/**
 * UActionCombatComponent
 *
 * 战斗运行时的轻量数据/服务组件，避免角色类持续膨胀。
 *
 * 当前职责：
 *   - 连段切换的蒙太奇 BlendOut 协助
 *   - 连段转向（计算目标 Yaw 后，转交给 ActionCharacterMovementComponent 处理；
 *     不再使用 60Hz Timer + SetActorRotation 这种外挂方式，避免与 RootMotion 旋转打架）
 *   - 闪避充能（保留，DodgeFeature 已自带充能但 ActionCombatComponent 仍提供旧 API 兼容）
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UActionCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionCombatComponent();

	// ---------- 连段攻击 ----------

	/** 一段攻击切到下一段时使用的 BlendOut 时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (ClampMin = "0.0"))
	float AttackComboMontageBlendOutTime = 0.08f;

	/** 连段转向消费"转向窗口"时允许校正的最大 Yaw 角度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AttackTurnMaxDegrees = 45.0f;

	/**
	 * 连段转向的最大角速度（度/秒）。
	 *   - <= 0：瞬间到位（最早期推荐这么做，已经验证不会抖动）。
	 *   - > 0：在 CharacterMovement 的 PhysicsRotation 阶段以该角速度平滑转向。
	 *
	 * 这个值由 ActionCharacterMovementComponent 内部驱动，与 RootMotion 同帧、同管线，
	 * 因此不会再出现 60Hz Timer 旋转覆盖 RootMotion 旋转的抖动现象。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Attack", meta = (ClampMin = "0.0"))
	float AttackTurnMaxYawSpeedDeg = 0.0f;

	bool StopAttackMontageForComboTransition(UAnimInstance* AnimInstance, UAnimMontage* PreviousAttackMontage);

	void ApplyAttackTurnAtComboStart(ACharacter* Character, const FVector2D& LastMoveInput);

	/** 主动停止连段转向（例如攻击被打断）。 */
	void ClearAttackTurn(ACharacter* Character);

	// ---------- 闪避充能（DodgeFeature 已有自己的充能，这里保留兼容旧 API）----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge", meta = (ClampMin = "1"))
	int32 MaxDodgeCharges = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge")
	int32 CurrentDodgeCharges = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Dodge", meta = (ClampMin = "0.0"))
	float DodgeChargeCooldown = 2.0f;

	void InitializeDodgeCharges();
	bool HasAvailableDodgeCharge() const;
	void ConsumeDodgeCharge();
	int32 GetCurrentDodgeCharges() const { return CurrentDodgeCharges; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void RestoreDodgeCharge();

	FTimerHandle DodgeChargeRestoreTimerHandle;
};
