#pragma once

#include "CoreMinimal.h"
#include "Char/ActionCharacterBase.h"
#include "ActionPlayerCharacter.generated.h"

class UActionCombatComponent;
class UActionFeatureBase;
class UAttackFeature;
class UCameraComponent;
class UDodgeFeature;
class UInputAction;
class UInputBufferComponent;
class UInputMappingContext;
class ULockOnComponent;
class UNormalJumpFeature;
class USpringArmComponent;
struct FBranchingPointNotifyPayload;
struct FInputActionValue;

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

	// ---------- Feature 管理（供 Feature 自身和外部查询） ----------

	UFUNCTION(BlueprintCallable, Category = "Action|Feature")
	UActionFeatureBase* GetCurrentActiveFeature() const { return CurrentActiveFeature; }

	void SetCurrentActiveFeature(UActionFeatureBase* InFeature);
	void ClearCurrentActiveFeature(UActionFeatureBase* InFeature);

	/**
	 * 由 Feature 调用，请求把角色主状态切到 InState。
	 * 若 InState 是 Idle，会用 ResolveDefaultActionState() 决定真实回退值（HitReact/Dead 优先）。
	 */
	void RequestActionState(EActionCharacterState InState);

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActionCombatComponent> ActionCombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|LockOn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULockOnComponent> LockOnComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float InputBufferLifetime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input|Look", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookYawSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input|Look", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LookPitchSensitivity = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Input|Look", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float LockedLookPitchSensitivity = 0.3f;

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
};
