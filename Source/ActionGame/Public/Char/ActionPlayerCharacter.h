#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "ActionPlayerCharacter.generated.h"

class UAnimMontage;
class UCameraComponent;
class UInputAction;
class UInputBufferComponent;
class UInputMappingContext;
class USpringArmComponent;
struct FBranchingPointNotifyPayload;
struct FInputActionValue;

/**
 * 玩家可控制角色的最小版本。
 *
 * 当前职责：
 * 1. 第三人称相机骨架
 * 2. Enhanced Input 接入
 * 3. 最小攻击输入缓存
 * 4. 最小攻击动画播放与命中检测
 */
UCLASS()
class ACTIONGAME_API AActionPlayerCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

public:
	AActionPlayerCharacter();
	virtual bool CanAttack() const override;

protected:
	/** 第三人称相机拉杆。常用来做镜头距离、碰撞回缩、跟随旋转。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** 真正的跟随相机。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** 复用模板项目的基础输入配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 战斗输入配置。建议在蓝图里指定 IMC_PlayerCombat。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> RuntimeCombatMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> DodgeAction;

	/** 最小输入缓存组件。当前只服务于攻击/闪避的短窗口输入。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent;

	/** 输入缓存有效时长，单位秒。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InputBufferLifetime = 0.25f;

	/** 当前默认攻击蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	/** 命中检测半径。当前先用球形范围做近战原型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackHitCheckRadius = 120.0f;

	/** 命中检测中心点相对角色前方的距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	float AttackHitCheckForwardOffset = 120.0f;

	/** 当蒙太奇异常结束时，用于兜底清状态。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackFallbackDuration = 0.8f;

	/** 当前是否正在进行一次攻击流程。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	bool bAttackInProgress = false;

	/** 当前这段攻击是否已经收到过一次命中通知。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	bool bAttackHitTriggeredThisSequence = false;

	/** 记录本次攻击已经命中过的目标，避免同一段攻击重复结算同一怪物。 */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 把 2D 输入转换成世界前后/左右移动。 */
	void Move(const FInputActionValue& Value);

	/** 处理视角输入。 */
	void Look(const FInputActionValue& Value);

	/** 检查战斗输入资源是否已配置完成。 */
	void BuildRuntimeCombatInputMapping();

	/** 尝试消费一次攻击输入缓存。 */
	bool TryConsumeAttackInput();

	/** 进入一次完整攻击流程。命中时机改由 Anim Notify 决定。 */
	void BeginAttackSequence();

	/** 当前攻击的命中判定。 */
	void HandleAttackHitCheck();

	/** 当前攻击流程结束。 */
	void EndAttackSequence();

	/** Montage Notify 回调：在指定帧触发命中。 */
	UFUNCTION()
	void OnAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	/** 攻击蒙太奇结束或被打断时的收尾入口。 */
	UFUNCTION()
	void OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

public:
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void OnAttackInput();

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void OnDodgeInput();

	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	void TryStartAttack();

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
