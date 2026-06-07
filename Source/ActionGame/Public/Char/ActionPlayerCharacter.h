#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "Combat/Skills/ActionSkillTypes.h"
#include "ActionPlayerCharacter.generated.h"

class UActionCombatComponent;
class UActionFeatureBase;
class UAttackFeature;
class UCameraComponent;
class UDodgeFeature;
class UInputAction;
class UInputBufferComponent;
class UHitFeedbackComponent;
class UInputMappingContext;
class ULockOnComponent;
class UNormalJumpFeature;
class USpringArmComponent;
class UWeaponComponent;
struct FBranchingPointNotifyPayload;
struct FInputActionValue;

/**
 * 角色当前的移动档位。
 *   - Walk：慢速（潜行/瞄准时切到这档；目前未自动启用，预留接口）
 *   - Jog：默认中速，按方向键就跑
 *   - Sprint：冲刺（按住 Sprint 键时启用；锁定状态下禁用）
 */
UENUM(BlueprintType)
enum class EActionMovementGait : uint8
{
	Walk   UMETA(DisplayName = "Walk"),
	Jog    UMETA(DisplayName = "Jog"),
	Sprint UMETA(DisplayName = "Sprint")
};

/**
 * AActionPlayerCharacter
 *
 * 玩家角色。重构后该类只承担：
 *   1. 第三人称相机和 Enhanced Input 配置
 *   2. Action Feature 实例的创建、持有、查询、Tick 转发
 *   3. 输入回调 → TryActivateFeature(FeatureClass) 的极简转发
 *   4. 蒙太奇 Notify 总入口 → 转发给当前 ActiveFeature
 *
 * 具体动作（攻击、闪避、跳跃）的逻辑全部迁移到对应的 UActionFeatureBase 子类里。
 */
UCLASS()
class ACTIONGAME_API AActionPlayerCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

public:
	AActionPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	// ---------- 公开输入接口（保持稳定，蓝图里若有绑定不会断） ----------

	UFUNCTION(BlueprintCallable, Category = "Action|Input")
	void OnAttackInput();

	UFUNCTION(BlueprintCallable, Category = "Action|Input")
	void OnDodgeInput();

	UFUNCTION(BlueprintCallable, Category = "Action|Input")
	void OnJumpInput();

	UFUNCTION(BlueprintCallable, Category = "Action|Input")
	void OnJumpInputReleased();

	UFUNCTION(BlueprintCallable, Category = "Action|Input")
	void OnLockOnInput();

	/** Sprint 按下：切到 Sprint 档（锁定状态下会被忽略，强制 Jog）。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Input")
	void OnSprintInputPressed();

	/** Sprint 松开：回到 Jog 档。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Input")
	void OnSprintInputReleased();

	// ---------- Feature 管理（供 Feature 自身和外部查询） ----------

	UFUNCTION(BlueprintCallable, Category = "Action|Feature")
	UActionFeatureBase* GetCurrentActiveFeature() const { return CurrentActiveFeature; }

	void SetCurrentActiveFeature(UActionFeatureBase* InFeature);
	void ClearCurrentActiveFeature(UActionFeatureBase* InFeature);

	/**
	 * 由 Feature 调用，请求把角色主状态切到 InState。
	 * 若 InState 是 Idle，会用 ResolveDefaultActionState() 决定真实回退值（HitReact/Dead 优先）。
	 */
	virtual void RequestActionState(EActionCharacterState InState) override;

	template<typename T>
	T* GetFeature() const
	{
		return Cast<T>(GetFeatureByClass(T::StaticClass()));
	}

	UFUNCTION(BlueprintCallable, Category = "Action|Feature", meta = (DeterminesOutputType = "FeatureClass"))
	UActionFeatureBase* GetFeatureByClass(TSubclassOf<UActionFeatureBase> FeatureClass) const;

	// ---------- 移动/方向工具 ----------

	UFUNCTION(BlueprintPure, Category = "Action|Input")
	FVector2D GetLastMoveInput() const { return LastMoveInput; }

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UActionCombatComponent* GetActionCombatComponent() const { return ActionCombatComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Input")
	UInputBufferComponent* GetInputBufferComponent() const { return InputBufferComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|LockOn")
	ULockOnComponent* GetLockOnComponent() const { return LockOnComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	/** 当前锁定目标位置（无锁则返回零向量）。供 AttackFeature/DodgeFeature 使用。 */
	UFUNCTION(BlueprintPure, Category = "Action|LockOn")
	FVector GetLockOnTargetLocation() const;

	UFUNCTION(BlueprintPure, Category = "Action|LockOn")
	bool HasLockOnTarget() const;

	UFUNCTION(BlueprintPure, Category = "Action|Equipment")
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UHitFeedbackComponent* GetHitFeedbackComponent() const { return HitFeedbackComponent; }

	// ---------- ABP 用的辅助函数（BlueprintPure，每帧由 ABP 调用）----------

	/**
	 * 获取角色当前 Velocity 在水平面的大小（cm/s）。
	 * 对应 ABP 里 BlendSpace 的 Speed 轴。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|Animation")
	float GetMovementSpeed() const;

	/**
	 * 计算角色 Velocity 相对自身朝向的方向角度（[-180°, 180°]）。
	 *   0°  = 正前
	 *   90° = 正右
	 *  -90° = 正左
	 * ±180° = 正后
	 *
	 * 对应 ABP 里 Strafe BlendSpace 2D 的 Direction 轴。
	 * 速度太小时返回 0，避免角色站住时 BlendSpace 抖动。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|Animation")
	float GetMovementDirection() const;

	/**
	 * 是否处于 Strafe 模式（侧步移动模式）。
	 * 当前实现：锁定目标时为 true，否则 false。
	 * ABP 用此值切换 Locomotion 子状态机。
	 */
	UFUNCTION(BlueprintPure, Category = "Action|Animation")
	bool IsStrafing() const;

	UFUNCTION(BlueprintPure, Category = "Action|Animation")
	EActionMovementGait GetCurrentGait() const { return CurrentGait; }

	void SetMovementControlScales(float InMoveInputScale, float InMaxSpeedMultiplier);
	void ClearMovementControlScales();
	void RefreshMovementSettings();

	UFUNCTION(BlueprintPure, Category = "Action|AI|Hearing")
	float GetAttackNoiseLoudness() const { return AttackNoiseLoudness; }

	// ---------- 闪避充能查询（旧 API 保持兼容） ----------

	UFUNCTION(BlueprintPure, Category = "Action|Combat|Dodge")
	int32 GetCurrentDodgeCharges() const;

	// ---------- 沿用基类接口 ----------

	virtual bool CanAttack() const override;

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	bool CanDodge() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState) override;
	virtual void OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void ClearMoveInput();
	void Look(const FInputActionValue& Value);
	void BuildRuntimeCombatInputMapping();

	void InitializeFeatures();
	UActionFeatureBase* CreateFeatureInstance(TSubclassOf<UActionFeatureBase> FeatureClass);
	bool TryCancelCurrentSkillForInput(EActionSkillCancelFlag IncomingType, EActionSkillStopReason StopReason);

	UFUNCTION()
	void OnAnyMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);

	void BindMontageNotifyDelegateIfNeeded();

	EActionCharacterState ResolveDefaultActionState() const;

protected:
	// ---------- 第三人称相机 ----------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	// ---------- Enhanced Input 资源 ----------
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LockOnAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActionCombatComponent> ActionCombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|LockOn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULockOnComponent> LockOnComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHitFeedbackComponent> HitFeedbackComponent;

	// ---------- 速度档位配置 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MaxWalkSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MaxJogSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float MaxSprintSpeed = 750.0f;

	/** 锁定状态下是否强制禁用 Sprint。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Movement", meta = (AllowPrivateAccess = "true"))
	bool bDisableSprintWhenLocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InputBufferLifetime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input|Look", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookYawSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input|Look", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookPitchSensitivity = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input|Look", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LockedLookPitchSensitivity = 0.3f;

	// ---------- 听觉感知发声（Day 5） ----------

	/**
	 * 奔跑发声节流间隔（秒）。玩家以 Sprint 档速度 + 在地面时，每 NoiseReportInterval
	 * 报一次 Footstep 噪音给 UAINoiseSubsystem。Walk/Jog 默认不发声。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Hearing", meta = (AllowPrivateAccess = "true", ClampMin = "0.05"))
	float NoiseReportInterval = 1.0f;

	/** 奔跑脚步声的传播半径（cm）。一般 2000~2500 之间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Hearing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FootstepNoiseLoudness = 1800.0f;

	/**
	 * 攻击命中噪音的传播半径（cm）。
	 * 命中后顺手由 AttackFeature 报一次 Combat 噪音，把附近未察觉的怪也拉到 Alert。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|AI|Hearing", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float AttackNoiseLoudness = 2500.0f;

	// ---------- Feature 配置（编辑器里指定子类） ----------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Feature")
	TSubclassOf<UAttackFeature> AttackFeatureClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Feature")
	TSubclassOf<UDodgeFeature> DodgeFeatureClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Feature")
	TSubclassOf<UNormalJumpFeature> JumpFeatureClass;

	// ---------- Feature 运行时实例 ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Feature")
	TObjectPtr<UAttackFeature> AttackFeature;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Feature")
	TObjectPtr<UDodgeFeature> DodgeFeature;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Feature")
	TObjectPtr<UNormalJumpFeature> JumpFeature;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Feature")
	TArray<TObjectPtr<UActionFeatureBase>> Features;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Feature")
	TObjectPtr<UActionFeatureBase> CurrentActiveFeature;

	// ---------- 状态缓存 ----------

	FVector2D LastMoveInput = FVector2D::ZeroVector;

	bool bMontageNotifyDelegateBound = false;

	bool bWasInAirLastFrame = false;

	/** 上一次脚步声 Report 的世界时间。-1 表示从未 Report。 */
	float LastFootstepReportTime = -1000.0f;

	/** 当前实际生效的速度档位。Sprint 输入 + 锁定状态共同决定。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Movement", meta = (AllowPrivateAccess = "true"))
	EActionMovementGait CurrentGait = EActionMovementGait::Jog;

	/** 玩家当前是否按住 Sprint 键。锁定时即使按住也不会切到 Sprint。 */
	bool bSprintInputHeld = false;

	/** 根据 bSprintInputHeld + 锁定状态重算 CurrentGait 并应用到 CharacterMovement。 */
	void UpdateCurrentGait();

	float MoveInputScale = 1.0f;
	float MaxSpeedMultiplier = 1.0f;

	/** LockOn 目标变化时回调（绑定到 ULockOnComponent::OnLockOnTargetChanged）。 */
	UFUNCTION()
	void OnLockOnTargetChangedHandler(AActor* NewTarget);
};
