#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "ActionCharacterBase.generated.h"

class UActionCharacterMovementComponent;
class UHitPhysicsComponent;
class UHitReceiverComponent;
class UHitReactComponent;
struct FHitContext;

UENUM(BlueprintType)
enum class EActionCharacterState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Attacking UMETA(DisplayName = "Attacking"),
	Dodging UMETA(DisplayName = "Dodging"),
	HitReact UMETA(DisplayName = "HitReact"),
	Dead UMETA(DisplayName = "Dead")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActionStateChangedSignature, EActionCharacterState, OldState, EActionCharacterState, NewState);

/**
 * UE 里的 ACharacter 是“带胶囊体、移动组件、骨骼网格组件”的角色基类，
 * 很适合拿来做第三人称玩家和怪物。
 *
 * 我们当前这个类是在 ACharacter 之上再包一层自己的“战斗角色基类”，
 * 先只承接最核心职责：
 * 1. 生命值
 * 2. 受伤
 * 3. 死亡
 * 4. 能否发起攻击
 *
 * 这样做的目的，是避免像 010 的 CharacterBase 那样一开始就塞进太多系统。
 */
UCLASS()
class ACTIONGAME_API AActionCharacterBase : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AActionCharacterBase(const FObjectInitializer& ObjectInitializer);

protected:
	// BeginPlay 是 Actor 真正进入世界后的入口。
	// 构造函数更适合创建组件和设置默认值，BeginPlay 更适合依赖世界对象的初始化。
	virtual void BeginPlay() override;

	/** 最大生命值。先保留为基础数值，后续再迁移更正式的属性组件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Attributes")
	float MaxHP = 100.0f;

	/** 当前生命值。VisibleAnywhere 方便我们在编辑器里观察运行时状态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Attributes")
	float CurrentHP = 100.0f;

	/** 基础攻击力。Day 2 先只作为占位数值，后面接入伤害流程时会真正使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Attributes")
	float AttackPower = 10.0f;

	/** 是否已经死亡。先用一个简单布尔值建立最小状态闭环。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|State")
	bool bIsDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|State")
	EActionCharacterState CurrentActionState = EActionCharacterState::Idle;

	/** 当前角色拥有的动作标签。先用于状态/窗口/禁用规则，后续可扩展给 AnimBP、AI、技能判断。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Tags")
	FGameplayTagContainer ActionTags;

public:
	UPROPERTY(BlueprintAssignable, Category = "Action|State")
	FActionStateChangedSignature OnActionStateChanged;

	/** 角色当前是否允许发起攻击。后续可以在这里逐步接入更复杂的状态判断。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual bool CanAttack() const;

	/** 最小受伤入口。先直接按数值扣血，后续再替换成正式 DamageSpec 流程。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual void ApplyDamage(float InDamage);

	/**
	 * 完整受击入口（推荐使用）。会走 HitReceiver 三层调度：扣血 + 反馈 + 受击动画 + 物理。
	 * 旧的 ApplyDamage 仅扣血，保留是为了兼容现有调用点（环境/陷阱伤害）。
	 */
	virtual void ReceiveHit(const FHitContext& HitCtx);

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UHitReceiverComponent* GetHitReceiver() const { return HitReceiverComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UHitReactComponent* GetHitReactComponent() const { return HitReactComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UHitPhysicsComponent* GetHitPhysicsComponent() const { return HitPhysicsComponent; }

	/** 最小死亡入口。后续怪物死亡、玩家死亡都会从这里扩展。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual void Die();

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetAttackPower() const { return AttackPower; }

	UFUNCTION(BlueprintPure, Category = "Action|State")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Action|State")
	EActionCharacterState GetCurrentActionState() const { return CurrentActionState; }

	UFUNCTION(BlueprintPure, Category = "Action|State")
	bool IsInActionState(EActionCharacterState InState) const { return CurrentActionState == InState; }

	UFUNCTION(BlueprintPure, Category = "Action|Movement")
	virtual bool CanMove() const;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	UFUNCTION(BlueprintPure, Category = "Action|Tags")
	bool HasActionTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "Action|Tags")
	bool HasAnyActionTags(const FGameplayTagContainer& QueryTags) const;

	UFUNCTION(BlueprintPure, Category = "Action|Tags")
	bool HasAllActionTags(const FGameplayTagContainer& QueryTags) const;

	UFUNCTION(BlueprintPure, Category = "Action|Tags")
	FString GetActionTagsDebugString() const;

	/**
	 * 给外部 ActionFeature 添加/移除 Action Tag 的公开入口。
	 * 内部仍走 AddActionTag / RemoveActionTag，但允许 Feature 在不继承基类的情况下安全修改 Tag。
	 */
	UFUNCTION(BlueprintCallable, Category = "Action|Tags")
	void AddActionTagExternal(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Action|Tags")
	void RemoveActionTagExternal(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "Action|Movement")
	UActionCharacterMovementComponent* GetActionCharacterMovement() const;

	UFUNCTION(BlueprintCallable, Category = "Action|Movement")
	void SetEnableRootMotionZExtraction(bool bEnableExtraction);

	UFUNCTION(BlueprintCallable, Category = "Action|Movement")
	void SetRootMotionZScale(float InScale);

protected:
	/**
	 * 受击调度组件。挂在所有角色（玩家/怪物）上，作为受击体系的统一入口。
	 * 内部把请求分发到 HitFeedback / HitReact / HitPhysics 三层。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UHitReceiverComponent> HitReceiverComponent;

	/**
	 * 受击动画反应组件。所有角色共有；DataTable 配置在子类（玩家 BP / 怪物 BP）里独立指定。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UHitReactComponent> HitReactComponent;

	/**
	 * 受击物理组件。只处理击飞、Ragdoll、起身等物理层表现，不负责选择受击动画。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UHitPhysicsComponent> HitPhysicsComponent;

	virtual bool CanChangeActionState(EActionCharacterState OldState, EActionCharacterState NewState) const;
	virtual void OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState);
	virtual void OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState);
	virtual void SetActionState(EActionCharacterState NewState);

	void AddActionTag(FGameplayTag Tag);
	void RemoveActionTag(FGameplayTag Tag);
	void ResetActionTagsForState(EActionCharacterState State);

	UFUNCTION(BlueprintImplementableEvent, Category = "Action|State", meta = (DisplayName = "On Action State Exit"))
	void BP_OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Action|State", meta = (DisplayName = "On Action State Enter"))
	void BP_OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState);
};
