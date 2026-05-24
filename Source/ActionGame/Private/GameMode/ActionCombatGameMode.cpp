// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameMode/ActionCombatGameMode.h"

#include "Char/ActionMonsterCharacter.h"
#include "Char/ActionPlayerCharacter.h"
#include "GameFramework/Controller.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/TargetPoint.h"

AActionCombatGameMode::AActionCombatGameMode()
{
	// 这里先给出 C++ 级别的默认类型，保证就算你还没建蓝图，项目也能先跑起来。
	// 但真正测试时，我们更希望在 GameMode 蓝图里把它们替换成带模型和动画的蓝图子类。
	PlayerCharacterClass = AActionPlayerCharacter::StaticClass();
	DefaultPawnClass = PlayerCharacterClass;
	MonsterCharacterClass = AActionMonsterCharacter::StaticClass();
}

void AActionCombatGameMode::BeginPlay()
{
	Super::BeginPlay();

	CacheMonsterSpawnPoints();

	if (bAutoStartCombat)
	{
		StartCombatTest();
	}
}

UClass* AActionCombatGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// 新手很容易踩到一个坑：
	// 虽然在 GameMode 蓝图里改了 PlayerCharacterClass，
	// 但如果 DefaultPawnClass 没有同步，运行时生成的仍可能是“没有绑 Mesh 的 C++ 基类”。
	//
	// 所以这里直接把“默认 Pawn 是谁”的最终决定权交给 PlayerCharacterClass。
	if (PlayerCharacterClass != nullptr)
	{
		return PlayerCharacterClass;
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void AActionCombatGameMode::StartCombatTest()
{
	AliveMonsterCount = 0;
	SpawnInitialMonsters();
}

void AActionCombatGameMode::CacheMonsterSpawnPoints()
{
	CachedMonsterSpawnPoints.Reset();

	// 优先使用关卡里显式配置的出生点，这样测试图的结果更可控。
	for (ATargetPoint* SpawnPoint : MonsterSpawnPoints)
	{
		if (IsValid(SpawnPoint))
		{
			CachedMonsterSpawnPoints.Add(SpawnPoint);
		}
	}

	// 同时按 Tag 自动搜集。这样测试关卡只要复制 TargetPoint 并加 MonsterSpawn Tag，
	// 开局就会在所有点生成怪物，不需要同步维护 InitialMonsterCount。
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		ATargetPoint* SpawnPoint = *It;
		if (IsValid(SpawnPoint) && SpawnPoint->ActorHasTag(MonsterSpawnPointTag))
		{
			CachedMonsterSpawnPoints.AddUnique(SpawnPoint);
		}
	}
}

void AActionCombatGameMode::SpawnInitialMonsters()
{
	if (MonsterCharacterClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ActionCombatGameMode: MonsterCharacterClass is not set. Skipping monster spawn."));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (int32 Index = 0; Index < CachedMonsterSpawnPoints.Num(); ++Index)
	{
		ATargetPoint* SpawnPoint = CachedMonsterSpawnPoints[Index];
		if (!IsValid(SpawnPoint))
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActionMonsterCharacter* SpawnedMonster = World->SpawnActor<AActionMonsterCharacter>(
			MonsterCharacterClass,
			SpawnPoint->GetActorTransform(),
			SpawnParams);

		if (IsValid(SpawnedMonster))
		{
			// 这里只记数量，后面接入怪物死亡逻辑时再把它和战斗结束检测串起来。
			++AliveMonsterCount;
		}
	}
}

void AActionCombatGameMode::HandleMonsterDied(AActionMonsterCharacter* DeadMonster)
{
	if (!IsValid(DeadMonster))
	{
		return;
	}

	AliveMonsterCount = FMath::Max(AliveMonsterCount - 1, 0);
}

void AActionCombatGameMode::RestartCombatTest()
{
	StartCombatTest();
}

bool AActionCombatGameMode::CheckCombatFinished() const
{
	return AliveMonsterCount <= 0;
}
