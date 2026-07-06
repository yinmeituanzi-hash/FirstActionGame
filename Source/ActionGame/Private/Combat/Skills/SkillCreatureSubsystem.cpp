#include "Combat/Skills/SkillCreatureSubsystem.h"

#include "Combat/Skills/SkillCreature.h"
#include "Combat/Skills/SkillCreatureTypes.h"
#include "Combat/Skills/SkillCreatureLogic.h"
#include "Combat/Skills/SkillCreaturePool.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkillCreatureSubsystem, Log, All);

void USkillCreatureSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Pool = NewObject<USkillCreaturePool>(this, USkillCreaturePool::StaticClass(), TEXT("SkillCreaturePool"));
}

void USkillCreatureSubsystem::Deinitialize()
{
	if (Pool != nullptr)
	{
		Pool->ClearAll();
		Pool = nullptr;
	}

	for (const TWeakObjectPtr<ASkillCreature>& Weak : ActiveCreatures)
	{
		if (ASkillCreature* C = Weak.Get())
		{
			C->Destroy();
		}
	}
	ActiveCreatures.Reset();
	SkillCreatureDataTable = nullptr;

	Super::Deinitialize();
}

void USkillCreatureSubsystem::SetSkillCreatureDataTable(UDataTable* InTable)
{
	SkillCreatureDataTable = InTable;
}

bool USkillCreatureSubsystem::ResolveCreatureRow(FName CreatureId, FSkillCreatureRow& OutRow) const
{
	if (SkillCreatureDataTable == nullptr || CreatureId.IsNone())
	{
		return false;
	}
	if (const FSkillCreatureRow* Found = SkillCreatureDataTable->FindRow<FSkillCreatureRow>(CreatureId, TEXT("SkillCreatureSubsystem")))
	{
		OutRow = *Found;
		return true;
	}
	return false;
}

TArray<ASkillCreature*> USkillCreatureSubsystem::SpawnSkillCreature(const FSkillCreatureSpawnRequest& Request)
{
	TArray<ASkillCreature*> Result;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return Result;
	}

	FSkillCreatureRow Row;
	if (!ResolveCreatureRow(Request.CreatureId, Row))
	{
		UE_LOG(
			LogSkillCreatureSubsystem,
			Warning,
			TEXT("SpawnSkillCreature: Row not found. Id=%s"),
			*Request.CreatureId.ToString());
		return Result;
	}

	if (Row.ActorClass == nullptr)
	{
		UE_LOG(
			LogSkillCreatureSubsystem,
			Warning,
			TEXT("SpawnSkillCreature: Row.ActorClass is null. Id=%s"),
			*Request.CreatureId.ToString());
		return Result;
	}

	const int32 SpawnCount = FMath::Max(1, Request.SpawnCount);

	// 基础 Transform 相同，每发在方向上做散射
	const FTransform BaseTransform = USkillCreatureLogic::ResolveSpawnTransform(Request, Row);
	const FVector BaseVelocity = USkillCreatureLogic::ResolveInitialVelocity(Request, Row, BaseTransform);
	const FVector BaseDirection = BaseVelocity.GetSafeNormal();

	Result.Reserve(SpawnCount);
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		FSkillCreatureSpawnRequest PerShot = Request;
		if (SpawnCount > 1 && Request.SpreadAngleDeg > KINDA_SMALL_NUMBER)
		{
			const FVector SpreadDir = USkillCreatureLogic::GetSpreadDirection(
				BaseDirection, Index, SpawnCount, Request.SpreadAngleDeg);
			PerShot.DirectionMode = ECreatureDirectionMode::WorldDirection;
			PerShot.ExplicitWorldDirection = SpreadDir;
		}

		// Day8.1：Pool.Acquire 返回 nullptr → 走 SpawnActor
		ASkillCreature* Creature = (Pool != nullptr)
			? Pool->Acquire(World, Row.ActorClass)
			: nullptr;

		if (Creature == nullptr)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.Owner = Request.SourceCharacter.Get();
			Creature = World->SpawnActor<ASkillCreature>(Row.ActorClass, BaseTransform, Params);
		}

		if (Creature == nullptr)
		{
			continue;
		}

		Creature->ActivateFromRequest(PerShot, Row);
		RegisterCreature(Creature);
		Result.Add(Creature);
	}

	return Result;
}

void USkillCreatureSubsystem::RegisterCreature(ASkillCreature* Creature)
{
	if (Creature == nullptr)
	{
		return;
	}
	ActiveCreatures.AddUnique(Creature);
}

void USkillCreatureSubsystem::UnregisterCreature(ASkillCreature* Creature)
{
	if (Creature == nullptr)
	{
		return;
	}
	ActiveCreatures.RemoveAllSwap([Creature](const TWeakObjectPtr<ASkillCreature>& W)
	{
		return !W.IsValid() || W.Get() == Creature;
	});
}
