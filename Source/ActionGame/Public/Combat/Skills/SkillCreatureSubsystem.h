#pragma once

#include "CoreMinimal.h"
#include "Combat/Skills/SkillCreatureTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SkillCreatureSubsystem.generated.h"

class ASkillCreature;
class USkillCreaturePool;
class UDataTable;
struct FSkillCreatureRow;

/**
 * SkillCreature 世界子系统。
 *
 * 单一入口：SkillEffectLibrary 的 ExecuteSpawnCreature 只跟本子系统对话，
 * 不直接调用 SpawnActor，也不感知对象池的存在。
 *
 * 职责：
 * - 持有 DT_SkillCreatures 引用（由 SkillComponent 或全局配置初始化）
 * - 提供 SpawnSkillCreature(Request) 一站式生成
 * - 维护活跃 Creature 列表（调试 / 死亡广播时统一回收）
 * - 承接 Pool 的对外 API 边界，即使 Day8.1 池未启用
 */
UCLASS()
class ACTIONGAME_API USkillCreatureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---------- USubsystem ----------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------- DataTable ----------

	/** 由 SkillComponent 或启动脚本注入配置表。允许多次调用（后设置的覆盖前面的）。 */
	void SetSkillCreatureDataTable(UDataTable* InTable);

	/** 是否已注入数据表。 */
	bool HasSkillCreatureDataTable() const { return SkillCreatureDataTable != nullptr; }

	// ---------- 主入口 ----------

	/**
	 * 按 Request 生成 SkillCreature（含散射多发）。
	 * 返回本次实际创建的 Actor 列表（若 SpawnCount > 1 则可能多个）。
	 * 失败或数据缺失时返回空数组，调用方无需再做校验。
	 */
	TArray<ASkillCreature*> SpawnSkillCreature(const FSkillCreatureSpawnRequest& Request);

	// ---------- 注册 / 反注册 ----------

	/** Creature 在 ActivateFromRequest 末尾调用（Day8.1 由 Subsystem 主动登记，不需要 Creature 自己回调）。 */
	void RegisterCreature(ASkillCreature* Creature);

	/** Creature EndPlay 时反注册（Day8.1 由 EndPlay 或 DestroyCreature 触发）。 */
	void UnregisterCreature(ASkillCreature* Creature);

	// ---------- 查询 / 调试 ----------

	UFUNCTION(BlueprintPure, Category = "Action|Skill|Creature")
	int32 GetActiveCreatureCount() const { return ActiveCreatures.Num(); }

private:
	/** 查表：Day8.1 从 DT 中拷贝 Row。 */
	bool ResolveCreatureRow(FName CreatureId, FSkillCreatureRow& OutRow) const;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SkillCreatureDataTable;

	UPROPERTY(Transient)
	TObjectPtr<USkillCreaturePool> Pool;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ASkillCreature>> ActiveCreatures;
};
