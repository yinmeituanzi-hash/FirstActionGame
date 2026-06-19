#pragma once

#include "CoreMinimal.h"
#include "Combat/Attributes/AttributeTypes.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "ActionCharacterBase.generated.h"

class UAttributeComponent;
class UActionCharacterMovementComponent;
class UActionSkillComponent;
class UActionVFXComponent;
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
	Dead UMETA(DisplayName = "Dead"),
	Skill UMETA(DisplayName = "Skill")
};

UENUM(BlueprintType)
enum class EActionCombatTeam : uint8
{
	Neutral UMETA(DisplayName = "Neutral"),
	Player UMETA(DisplayName = "Player"),
	Monster UMETA(DisplayName = "Monster")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FActionStateChangedSignature, EActionCharacterState, OldState, EActionCharacterState, NewState);

/**
 * 战斗角色基类。
 *
 * 只承接所有角色共用的动作状态、阵营、受击入口、技能入口和属性入口。
 * HP / MaxHP / AttackPower 等数值不再放在角色字段里，而是统一交给 UAttributeComponent。
 */
UCLASS()
class ACTIONGAME_API AActionCharacterBase : public ACharacter, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	AActionCharacterBase(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

	/** 最小阵营标记。Neutral 可以被任何非自身攻击；同阵营之间默认不互伤。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Combat|Team")
	EActionCombatTeam CombatTeam = EActionCombatTeam::Neutral;

	/** 是否已经死亡。真正死亡流程仍统一走 Die。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|State")
	bool bIsDead = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|State")
	EActionCharacterState CurrentActionState = EActionCharacterState::Idle;

	/** 当前角色拥有的动作标签，用于状态、窗口和禁用规则。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Tags")
	FGameplayTagContainer ActionTags;

public:
	UPROPERTY(BlueprintAssignable, Category = "Action|State")
	FActionStateChangedSignature OnActionStateChanged;

	/** 角色当前是否允许发起攻击。后续攻击迁入 Skill 后仍保留为统一查询口。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual bool CanAttack() const;

	/** 最小受伤入口。推荐新流程优先走 ReceiveHit，环境伤害等简单场景可继续直接调用。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual void ApplyDamage(float InDamage);

	/**
	 * 完整受击入口，会走扣血、受击反馈、受击动画和物理表现。
	 * 旧的 ApplyDamage 只负责扣血，保留用于兼容现有简单调用点。
	 */
	virtual void ReceiveHit(const FHitContext& HitCtx);

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UHitReceiverComponent* GetHitReceiver() const { return HitReceiverComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UHitReactComponent* GetHitReactComponent() const { return HitReactComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Combat")
	UHitPhysicsComponent* GetHitPhysicsComponent() const { return HitPhysicsComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill")
	UActionSkillComponent* GetActionSkillComponent() const { return SkillComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|VFX")
	UActionVFXComponent* GetActionVFXComponent() const { return VFXComponent; }

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	UAttributeComponent* GetAttributeComponent() const { return AttributeComponent; }

	/** 最小死亡入口。玩家死亡、怪物死亡和技能死亡打断都从这里扩展。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual void Die();

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetCurrentHP() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetMaxHP() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetAttackPower() const;

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetAttribute(EAttributeType Attribute) const;

	UFUNCTION(BlueprintPure, Category = "Action|Combat|Team")
	EActionCombatTeam GetCombatTeam() const { return CombatTeam; }

	UFUNCTION(BlueprintPure, Category = "Action|Combat|Team")
	bool IsFriendlyTo(const AActionCharacterBase* Other) const;

	UFUNCTION(BlueprintPure, Category = "Action|Combat|Team")
	bool CanDamageTarget(const AActionCharacterBase* Other) const;

	UFUNCTION(BlueprintPure, Category = "Action|State")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Action|State")
	EActionCharacterState GetCurrentActionState() const { return CurrentActionState; }

	UFUNCTION(BlueprintPure, Category = "Action|State")
	bool IsInActionState(EActionCharacterState InState) const { return CurrentActionState == InState; }

	UFUNCTION(BlueprintCallable, Category = "Action|State")
	virtual void RequestActionState(EActionCharacterState InState);

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

	/** 给 ActionFeature / Skill 等外部系统安全修改动作标签的公开入口。 */
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
	/** 统一战斗属性入口。HP / MaxHP / AttackPower 等属性的默认值和运行时值都由这里管理。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Attributes")
	TObjectPtr<UAttributeComponent> AttributeComponent;

	/** 受击调度组件：统一处理扣血后的反馈、受击动画和物理表现。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UHitReceiverComponent> HitReceiverComponent;

	/** 受击动画反应组件。玩家和怪物可在各自 BP 中指定不同 DataTable。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UHitReactComponent> HitReactComponent;

	/** 受击物理组件，只处理击飞、Ragdoll、起身等物理层表现。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Combat")
	TObjectPtr<UHitPhysicsComponent> HitPhysicsComponent;

	/** 技能系统入口。后续普攻连段也会逐步迁入这里。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill")
	TObjectPtr<UActionSkillComponent> SkillComponent;

	/** 角色侧特效播放门面，真正播放和登记由 UActionVFXSubsystem 负责。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|VFX")
	TObjectPtr<UActionVFXComponent> VFXComponent;

	virtual bool CanChangeActionState(EActionCharacterState OldState, EActionCharacterState NewState) const;
	virtual void OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState);
	virtual void OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState);
	virtual void SetActionState(EActionCharacterState NewState);

	UFUNCTION()
	void HandleAttributeChanged(EAttributeType Attribute, float OldValue, float NewValue);

	void AddActionTag(FGameplayTag Tag);
	void RemoveActionTag(FGameplayTag Tag);
	void ResetActionTagsForState(EActionCharacterState State);

	UFUNCTION(BlueprintImplementableEvent, Category = "Action|State", meta = (DisplayName = "On Action State Exit"))
	void BP_OnActionStateExit(EActionCharacterState OldState, EActionCharacterState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Action|State", meta = (DisplayName = "On Action State Enter"))
	void BP_OnActionStateEnter(EActionCharacterState OldState, EActionCharacterState NewState);
};
