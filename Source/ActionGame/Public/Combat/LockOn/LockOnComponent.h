#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnComponent.generated.h"

class AActionPlayerCharacter;
class AActor;
class APlayerController;
class UCameraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChanged, AActor*, NewTarget);

/**
 * ULockOnComponent
 *
 * 玩家锁定目标系统。挂在 AActionPlayerCharacter 上。
 *
 * 核心职责：
 *   1. 搜索：从角色周围 LockOnRadius 范围内、IActionLockableInterface 实现者中筛选候选
 *   2. 选择：按"屏幕中心距离 + 世界距离"加权选最优目标
 *   3. 锁定/解锁：切换 PlayerCharacter 的运动模式（Strafe vs Forward），通知目标接口
 *   4. 维护：每帧检查目标是否仍可锁（死亡/超距离/被遮挡）→ 自动解锁
 *   5. 相机：每帧把 ControlRotation 平滑插值到 "看向目标" 的方向（不抢占玩家手感，保留鼠标微调）
 *   6. 切换：在锁定状态下读取鼠标横向输入，达到阈值时切换到左/右邻近目标
 *
 * 组件 Tick 启用，每帧驱动相机和目标维护逻辑。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnComponent();

	// ---------- 搜索/选择参数 ----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Search", meta = (ClampMin = "100.0"))
	float LockOnRadius = 1500.0f;

	/**
	 * 解锁阈值（世界距离）：目标超出该距离 → 自动解锁。
	 * 一般略大于 LockOnRadius，避免边缘抖动反复锁/解。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Search", meta = (ClampMin = "100.0"))
	float UnlockDistance = 1800.0f;

	/** 选择目标时，"屏幕中心距离"权重 vs "世界距离"权重。值越大越偏向屏幕中心。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Search", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float ScreenCenterWeight = 2.0f;

	/** 是否需要目标在视锥内（Pitch ±FOV/2）。锁定瞬间生效；锁定后即使转到背后也保持。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Search")
	bool bRequireInScreenForInitialLock = true;

	// ---------- 相机参数 ----------

	/** 相机插值速度（度/秒）。值越大相机越"硬"地跟向目标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Camera", meta = (ClampMin = "0.0"))
	float CameraInterpSpeed = 8.0f;

	/** 锁定时相机往下看的 Pitch 偏移（度）。负值=往下看，让目标处于屏幕中上方更舒服。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Camera", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float CameraPitchOffset = -15.0f;

	// ---------- 切换目标参数 ----------

	/** 鼠标横向输入超过该阈值时触发左/右切换目标。单位是 Look 输入累积值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch", meta = (ClampMin = "0.0"))
	float SwitchTargetThreshold = 3.0f;

	/** 两次切换目标的最小间隔（秒），防止误触一次切两个。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LockOn|Switch", meta = (ClampMin = "0.0"))
	float SwitchTargetCooldown = 0.3f;

	// ---------- 公开接口 ----------

	/** 切换锁定状态：当前无锁 → 尝试锁定；当前有锁 → 解锁。 */
	UFUNCTION(BlueprintCallable, Category = "LockOn")
	void ToggleLockOn();

	UFUNCTION(BlueprintCallable, Category = "LockOn")
	void RequestLockOn();

	UFUNCTION(BlueprintCallable, Category = "LockOn")
	void RequestUnlock();

	UFUNCTION(BlueprintPure, Category = "LockOn")
	bool IsLocked() const { return CurrentTarget.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "LockOn")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** 由 PlayerCharacter Look 输入回调转发：若锁定中且 |LookX| 超阈值，切换目标。 */
	UFUNCTION(BlueprintCallable, Category = "LockOn")
	void NotifyLookInput(float LookX);

	UPROPERTY(BlueprintAssignable, Category = "LockOn")
	FOnLockOnTargetChanged OnLockOnTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	AActionPlayerCharacter* GetPlayerOwner() const;
	APlayerController* GetPlayerController() const;
	UCameraComponent* GetFollowCamera() const;

	/** 收集所有候选目标。 */
	void CollectCandidates(TArray<AActor*>& OutCandidates) const;

	/** 评分并选最优。返回 nullptr 表示没有合适目标。 */
	AActor* PickBestTarget(const TArray<AActor*>& Candidates, AActor* Excluding = nullptr) const;

	/** 评分函数：分数越小越优。 */
	float ScoreCandidate(AActor* Candidate) const;

	/** 检查目标是否仍可被锁定。 */
	bool IsTargetStillValid(AActor* Target) const;

	/** Owner 是否处于"无法操作"的强控制状态（Ragdoll / 起身 / 死亡）。锁定期间命中则强制解锁。 */
	bool IsOwnerInStrongControlState() const;

	/** 设置当前目标，处理新旧目标的接口回调和状态切换。 */
	void SetCurrentTarget(AActor* NewTarget);

	/** 锁定时 / 解锁时调整 PlayerCharacter 的运动模式（Strafe vs Forward）。 */
	void ApplyMovementModeForLockState(bool bLocked);

	/** 每帧把 ControlRotation 朝向目标插值。 */
	void UpdateCameraTowardsTarget(float DeltaTime);

	/** 切换目标：bRight=true 切右边，false 切左边。 */
	void SwitchTarget(bool bRight);

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTarget;

	/** 缓存进入锁定前 PlayerCharacter 的运动模式，解锁时还原。 */
	bool bCachedOrientToMovement = true;
	bool bCachedUseControllerYaw = false;

	float LastSwitchTime = -10.0f;
};
