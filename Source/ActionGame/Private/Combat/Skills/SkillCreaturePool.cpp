#include "Combat/Skills/SkillCreaturePool.h"

#include "Combat/Skills/SkillCreature.h"

ASkillCreature* USkillCreaturePool::Acquire(UWorld* /*World*/, TSubclassOf<ASkillCreature> /*ActorClass*/)
{
	// Day8.1：池未启用。返回 nullptr，Subsystem 会走 SpawnActor 路径。
	// Day8.6 会替换成："若 InactiveActors 非空则 Pop、Reactivate、返回；否则仍返回 nullptr"。
	return nullptr;
}

void USkillCreaturePool::Release(ASkillCreature* Creature)
{
	// Day8.1：直接销毁。Day8.6 改为：
	//   - 关闭 Tick / 隐藏 Actor / SetActorLocation 到远处
	//   - 加入对应 Pool 的 InactiveActors
	if (Creature != nullptr)
	{
		Creature->Destroy();
	}
}

void USkillCreaturePool::ClearAll()
{
	for (FSkillCreaturePoolItem& Item : Pools)
	{
		for (const TObjectPtr<ASkillCreature>& C : Item.InactiveActors)
		{
			if (C != nullptr)
			{
				C->Destroy();
			}
		}
		Item.InactiveActors.Reset();
	}
	Pools.Reset();
}

FSkillCreaturePoolItem& USkillCreaturePool::FindOrAddPool(TSubclassOf<ASkillCreature> ActorClass)
{
	for (FSkillCreaturePoolItem& Item : Pools)
	{
		if (Item.ActorClass == ActorClass)
		{
			return Item;
		}
	}
	FSkillCreaturePoolItem NewItem;
	NewItem.ActorClass = ActorClass;
	return Pools.Add_GetRef(MoveTemp(NewItem));
}
