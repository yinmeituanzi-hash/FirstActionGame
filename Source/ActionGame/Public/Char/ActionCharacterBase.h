#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ActionCharacterBase.generated.h"

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
class ACTIONGAME_API AActionCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	AActionCharacterBase();

protected:
	// BeginPlay 是 Actor 真正进入世界后的入口。
	// 构造函数更适合创建组件和设置默认值，BeginPlay 更适合依赖世界对象的初始化。
	virtual void BeginPlay() override;

	/** 最大生命值。先保留为基础数值，后续再迁移更正式的属性组件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Attributes")
	float MaxHP = 50.0f;

	/** 当前生命值。VisibleAnywhere 方便我们在编辑器里观察运行时状态。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Attributes")
	float CurrentHP = 50.0f;

	/** 基础攻击力。Day 2 先只作为占位数值，后面接入伤害流程时会真正使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Attributes")
	float AttackPower = 10.0f;

	/** 是否已经死亡。先用一个简单布尔值建立最小状态闭环。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|State")
	bool bIsDead = false;

public:
	/** 角色当前是否允许发起攻击。后续可以在这里逐步接入更复杂的状态判断。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual bool CanAttack() const;

	/** 最小受伤入口。先直接按数值扣血，后续再替换成正式 DamageSpec 流程。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Combat")
	virtual void ApplyDamage(float InDamage);

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
	bool IsDead() const { return bIsDead; }
};
