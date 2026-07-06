#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkillCreaturePool.generated.h"

class ASkillCreature;
class UWorld;

/**
 * 单个池条目：一个 Creature 类对应一个 InactiveActors 队列。
 * Day8.1 只放骨架，Acquire 一律返回 nullptr（Subsystem 会回退 SpawnActor）。
 */
USTRUCT()
struct FSkillCreaturePoolItem
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TSubclassOf<ASkillCreature> ActorClass = nullptr;

	/** 池中当前处于 Inactive 的实例。Acquire 从末尾取，Release 从末尾追加。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ASkillCreature>> InactiveActors;
};

/**
 * SkillCreature 对象池。
 *
 * Day8.1 责任：提供正确的 API 边界，让 Subsystem / EffectLibrary 端已经按池化路径写代码，
 * 内部实现先"没有池"—— Acquire 永远返回 nullptr，Release 直接 Destroy。
 * 这样 Day8.6 加真实池化时只改本类，不需要改上游调用点。
 */
UCLASS()
class ACTIONGAME_API USkillCreaturePool : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 从池中取一个 Creature。返回 nullptr 表示池中无可用实例，需由调用方 SpawnActor 新建。
	 * Day8.1 一律返回 nullptr。
	 */
	ASkillCreature* Acquire(UWorld* World, TSubclassOf<ASkillCreature> ActorClass);

	/**
	 * 把 Creature 归还池。Day8.1 直接 Destroy，等 Day8.6 换成"隐藏 + 入队"。
	 */
	void Release(ASkillCreature* Creature);

	/** 清空所有池（世界销毁时调用）。 */
	void ClearAll();

private:
	FSkillCreaturePoolItem& FindOrAddPool(TSubclassOf<ASkillCreature> ActorClass);

	UPROPERTY(Transient)
	TArray<FSkillCreaturePoolItem> Pools;
};
