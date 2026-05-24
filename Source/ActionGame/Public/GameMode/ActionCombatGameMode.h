// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ActionCombatGameMode.generated.h"

class AActionMonsterCharacter;
class AActionPlayerCharacter;
class ATargetPoint;

/**
 * UE 里的 GameMode 可以先理解成“这一局游戏规则和开局流程的编排者”。
 *
 * 它通常负责：
 * 1. 默认生成哪个玩家 Pawn
 * 2. 玩家和怪物从哪里进场
 * 3. 一局战斗什么时候开始、什么时候结束
 *
 * 它通常不负责：
 * 1. 具体怎么扣血
 * 2. 技能怎么命中
 * 3. 动画怎么播
 *
 * 所以我们当前这个类的定位很克制：
 * 先只做“测试战斗编排器”，不把伤害、受击、技能等细节塞进来。
 */
UCLASS()
class ACTIONGAME_API AActionCombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AActionCombatGameMode();

protected:
	// BeginPlay 是关卡真正开始运行后的入口。
	// 对 GameMode 来说，这里很适合做“开局编排”类的逻辑，例如收集出生点、启动测试战斗。
	virtual void BeginPlay() override;

	// 这个函数决定“当前控制器最终会生成哪个 Pawn 类”。
	// 我们重写它，是为了让蓝图里配置的 PlayerCharacterClass 真正生效，
	// 而不是永远退回到 C++ 构造函数里写死的默认值。
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	/** 是否在关卡开始时自动启动测试战斗。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Test")
	bool bAutoStartCombat = true;

	/** 当前测试战斗使用的怪物类。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Test")
	TSubclassOf<AActionMonsterCharacter> MonsterCharacterClass;

	/** 用于按 Tag 自动搜集怪物出生点。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Test")
	FName MonsterSpawnPointTag = TEXT("MonsterSpawn");

	/** 当前测试战斗使用的玩家角色类。 */
	UPROPERTY(EditDefaultsOnly, Category = "Combat Test")
	TSubclassOf<AActionPlayerCharacter> PlayerCharacterClass;

	/**
	 * 关卡里手动指定的怪物出生点。
	 * 这样做的好处是：以后做战斗测试图时，我们可以显式地把刷怪位置拖到 GameMode 配置里。
	 */
	UPROPERTY(EditInstanceOnly, Category = "Combat Test")
	TArray<TObjectPtr<ATargetPoint>> MonsterSpawnPoints;

	/** 运行时缓存下来的实际出生点列表。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ATargetPoint>> CachedMonsterSpawnPoints;

	/** 当前存活怪物数量，用于支撑最小战斗结束判断。 */
	UPROPERTY(VisibleInstanceOnly, Category = "Combat Test")
	int32 AliveMonsterCount = 0;

	/** 开始一局最小测试战斗。当前只做怪物生成和计数初始化。 */
	void StartCombatTest();

	/** 收集怪物出生点。优先用手动配置，缺省时再按 Tag 自动搜索。 */
	void CacheMonsterSpawnPoints();

	/** 按收集到的出生点生成初始怪物。 */
	void SpawnInitialMonsters();

public:
	/** 后续怪物死亡时会回调到这里，先只做最小计数。 */
	void HandleMonsterDied(AActionMonsterCharacter* DeadMonster);

	/** 方便测试时重开当前这波简单战斗。 */
	void RestartCombatTest();

	/** 当前最小结束条件：场上没有活着的怪物。 */
	bool CheckCombatFinished() const;
};
