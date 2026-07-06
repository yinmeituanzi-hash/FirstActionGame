#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/SkillCreatureTypes.h"
#include "Combat/Skills/SkillEffectSourceInterface.h"
#include "GameFramework/Actor.h"
#include "SkillCreature.generated.h"

class AActionCharacterBase;
class UActionSkillComponent;
class USphereComponent;
class USkillCreatureSubsystem;
struct FSkillCreatureRow;

/**
 * 技能生成物基础 Actor。
 *
 * 对应 010 的 ASkillCreature：投射物、剑气、追踪弹、抛物线炸弹、地面场等
 * 都由本类或其子类承担。命中回调统一走 HitEffectIds → USkillEffectLibrary，
 * 不在本类里直接写扣血逻辑。
 *
 * Day8.1 只启用 Straight / Homing / Gravity / Static 移动 + Sphere 碰撞 +
 * HitEnemy 单一 policy。骨架预留 Bound / Attach / Delay / 多 policy 支持。
 */
UCLASS()
class ACTIONGAME_API ASkillCreature : public AActor, public ISkillEffectSourceInterface
{
	GENERATED_BODY()

public:
	ASkillCreature();

	// ---------- 生命周期 API ----------

	/**
	 * Subsystem 从池中或新建后调用，用 Request + Row 一次性完成初始化。
	 *
	 * 内部会：
	 * - 缓存 Source / Skill / Target / Row
	 * - 解算出生 Transform 与初速度（走 SkillCreatureLogic）
	 * - 应用碰撞形状与 Profile
	 * - 挂载 LoopVFX
	 * - 打开 Tick
	 */
	void ActivateFromRequest(const FSkillCreatureSpawnRequest& InRequest, const FSkillCreatureRow& InRow);

	/**
	 * 显式销毁入口。任何路径想让 Creature 结束都走这里：
	 * - 生命周期到期
	 * - 命中触发销毁
	 * - 反弹次数用尽
	 * - Owner 死亡
	 *
	 * 内部会执行 DestroyEffectIds、停止 LoopVFX、向 Subsystem 反注册、DestroyActor。
	 * 后续接入对象池时改为 Recycle。
	 */
	void DestroyCreature(ECreatureDestroyReason Reason);

	// ---------- Getter ----------

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Creature")
	FName GetCreatureId() const { return CreatureId; }

	/** ISkillEffectSourceInterface 实现。 */
	virtual AActionCharacterBase* GetSourceCharacter() const override { return SourceCharacter.Get(); }
	virtual FName GetSourceSkillId() const override { return SourceSkillId; }
	virtual AActor* GetEffectSourceActor() override { return this; }

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Creature")
	AActor* GetCurrentTarget() const { return Target.Get(); }

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Creature")
	float GetRemainingLifeTime() const { return RemainingLifeTime; }

	/** 命中过的目标列表，供 SkillEffect / Debug 查询。 */
	const TSet<TWeakObjectPtr<AActor>>& GetHitActors() const { return HitActors; }

	// ---------- Tick ----------

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------- 移动 ----------

	/** Tick 分派入口：根据 Row.MoveMode 选择 Straight / Homing / Gravity / Static。 */
	void TickMovement(float DeltaSeconds);

	/** 高速时的路径 Sweep 兜底，避免一帧穿过目标。 */
	void SweepForHits(const FVector& OldLocation, const FVector& NewLocation);

	// ---------- 命中 ----------

	/**
	 * 统一命中入口。由 SphereComponent 的 BeginOverlap 或 SweepForHits 调用。
	 * 内部：阵营判定 → 命中去重 → 执行 HitEffectIds → 视配置销毁自身。
	 */
	void HandleHit(AActor* OtherActor, const FHitResult& OptionalHit);

	/** 判断 OtherActor 相对于发射者的阵营。 */
	ECreatureHitPolicy ClassifyHit(AActor* OtherActor) const;

	/** 执行给定 Policy 对应的 Effect 列表。 */
	void ExecuteHitEffects(ECreatureHitPolicy Policy, AActor* OtherActor);

	// ---------- BeginOverlap 委托 ----------

	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	// ---------- 组件 ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionSphere;

	// ---------- 运行时状态 ----------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature", meta = (AllowPrivateAccess = "true"))
	FName CreatureId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature", meta = (AllowPrivateAccess = "true"))
	FName SourceSkillId = NAME_None;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActionCharacterBase> SourceCharacter;

	UPROPERTY(Transient)
	TWeakObjectPtr<UActionSkillComponent> SourceSkillComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Target;

	/** 缓存 Row 的关键字段（Row 数据来自 DataTable，短时间内稳定）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature|Runtime", meta = (AllowPrivateAccess = "true"))
	float RemainingLifeTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature|Runtime", meta = (AllowPrivateAccess = "true"))
	FVector CurrentVelocity = FVector::ZeroVector;

	/** Homing / DelayEffect / CollisionDelay 用的累计时长。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature|Runtime", meta = (AllowPrivateAccess = "true"))
	float AliveTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature|Runtime", meta = (AllowPrivateAccess = "true"))
	int32 RemainingBreakCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature|Runtime", meta = (AllowPrivateAccess = "true"))
	int32 RemainingBoundCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature|Runtime", meta = (AllowPrivateAccess = "true"))
	bool bCollisionEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Skill|Creature|Runtime", meta = (AllowPrivateAccess = "true"))
	bool bMarkedForDestroy = false;

	/** 已命中目标集合，用于去重。 */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/** Row 的一份浅拷贝（Row 是 POD，直接值拷贝，避免每次命中都回查 DataTable）。 */
	TSharedPtr<FSkillCreatureRow> RuntimeRow;
};
